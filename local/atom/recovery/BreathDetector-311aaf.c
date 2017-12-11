#include "BreathDetector.h"

#include "AppConfig.h"
#include "AlarmManager.h"
#include "AnalogInput.h"
#include "ConfigManager.h"
#include "DataManager.h"
#include "Debug.h"
#include "IirFilter.h"
#include "stdlib.h"
#include "TaskControl.h"
#include "UsbComm.h"
#include "Utility.h"


typedef enum {
  BREATH_DETECTOR_UNINITED,
  BREATH_DETECTOR_STOPPED,
  BREATH_DETECTOR_RUNNING,
} eBreathDetectorState;



static eBreathDetectorState breath_detector_state = BREATH_DETECTOR_UNINITED;
static void                 (*breath_detected_cb)(U16 period_ms) = NULL;
static U32                  time_since_last_breath = 0;
static U32                  trigger_holdoff = 0;
static S32                  last_pressure = 0;


#define DEFAULT_BREATH_PERIOD           3000  // ms; equivalent to 20BPM
#define MIN_BREATH_PERIOD               1500  // ms; equivalent to 40BPM
#define MAX_BREATH_PERIOD               7500  // ms; equivalent to 8BPM
#define APNEA_DETECTION_PERIOD          15000 // ms
#define HOLDOFF_DURATION_STARTUP        1500  // ms
#define HOLDOFF_DURATION_STANDARD       1500  // ms



/////////////////////////////////////////////
// Define IIR filter for breath pressure data
// Low-pass filter with cutoff at 10Hz.
// 1000Hz sample rate
#define BREATH_FILTER_GAIN_SHIFT  5   // Filter has 32x gain due to scaling factor between a and b coeffs.
#define BREATH_FILTER_LENGTH      4
#define BREATH_FILTER_A0_SHIFT    13  // log2(8192), implement the division by coeff a0 as a shift.
#define LEVEL_TO_FILTER_OUTPUT(level)    (level << BREATH_FILTER_GAIN_SHIFT)
static S32        breath_filter_a[] = {8192, -23982, 23433, -7642};
static S32        breath_filter_b[] = {130, -112, -112, 130};

static S32        breath_filter_x_history[BREATH_FILTER_LENGTH];
static S32        breath_filter_y_history[BREATH_FILTER_LENGTH];
static tIirFilter breath_filter;



#define ENGINEERING_TEST_BREATH_PERIOD_UNLOCK_CODE      0x2B4EE0F9
static U16 engineering_test_breath_period_ms     = 0;
static U32 engineering_test_breath_period_unlock = 0;


static void BreathDetector_Task(tTaskParam param);
static BOOL BreathDetector_ConsoleHandler(S32 argc, char* argv[], BOOL help);
static void BreathDetector_UsbCommHandler(tUsbCommMessage *msg, tUsbCommMessage *response);

static U32 task_id = 0;
static TASK_TEMPLATE_STRUCT task_def =
{
  0,                              // TaskID.  Will get loaded with unique value in module init function.
  (TASK_FPTR)BreathDetector_Task, // Task function.
  1024,                           // Stack space.
  BREATH_DETECTOR_TASK_PRIORITY,  // Priority.
  "Breath Detector",              // Task Name.
  MQX_AUTO_START_TASK,            // Attributes.
  0,                              // Creation parameter.  Passed to the task function's 'param' parameter.
  10                              // Default time slice.
};

/////////////////////////////////////////////
//
// Public functions
//
/////////////////////////////////////////////


void BreathDetector_Init(void (*cb)(U16 period))
{
  PREVENT_REINIT;

  // Initialize modules this module references
  AlarmManager_Init();
  ConfigManager_Init();
  DataManager_Init();
  UsbComm_Init();

  // Hook command handler
  U16 codes[7];
  codes[0] = MESSAGE_CODE_GET_BREATH_CALIBRATION;
  codes[1] = MESSAGE_CODE_SET_BREATH_ZERO_OFFSET;
  codes[2] = MESSAGE_CODE_SET_BREATH_TRIGGER_LEVEL;
  codes[3] = MESSAGE_CODE_SET_BREATH_EXHALATION_LEVEL;
  codes[4] = MESSAGE_CODE_GET_BREATH_PRESSURE;
  codes[5] = MESSAGE_CODE_GET_FAKE_BREATH_PERIOD;
  codes[6] = MESSAGE_CODE_SET_FAKE_BREATH_PERIOD;
  UsbCommMessage_HookHandlerForMessages(BreathDetector_UsbCommHandler, codes, ARRAY_SIZE(codes));

  engineering_test_breath_period_ms     = 0;
  engineering_test_breath_period_unlock = 0;

  breath_detector_state = BREATH_DETECTOR_STOPPED;
  breath_detected_cb = cb;

  DebugInit();
  HookDebugConsole("breath", BreathDetector_ConsoleHandler);
}


void BreathDetector_Enable(void)
{
  if (breath_detector_state == BREATH_DETECTOR_STOPPED)
  {
    IirFilter_Init(
      &breath_filter,
      breath_filter_a,
      breath_filter_b,
      breath_filter_x_history,
      breath_filter_y_history,
      BREATH_FILTER_LENGTH,
      BREATH_FILTER_A0_SHIFT,
      0
    );

    time_since_last_breath = 0;
    trigger_holdoff = HOLDOFF_DURATION_STARTUP;

    engineering_test_breath_period_ms     = 0;
    engineering_test_breath_period_unlock = 0;

    AnalogInput_Init(AIN_BREATH_PRESSURE, TRUE);

    task_def.TASK_TEMPLATE_INDEX = GetUniqueTaskIndex();
    task_id = _task_create(0, 0, (tTaskParam)&task_def);

    breath_detector_state = BREATH_DETECTOR_RUNNING;
  }
}


void BreathDetector_Disable(void)
{
  if (breath_detector_state == BREATH_DETECTOR_RUNNING)
  {
    _task_destroy(task_id);
    AnalogInput_Init(AIN_BREATH_PRESSURE, FALSE);

    breath_detector_state = BREATH_DETECTOR_STOPPED;
  }
}


U16 BreathDetector_GetEngineeringTestBreathPeriod(void)
{
  return engineering_test_breath_period_ms;
}


void BreathDetector_SetEngineeringTestBreathPeriod(U32 unlock, U16 period_ms)
{
  if (unlock == ENGINEERING_TEST_BREATH_PERIOD_UNLOCK_CODE)
  {
    engineering_test_breath_period_ms = period_ms;
    engineering_test_breath_period_unlock = unlock;
  }

  if (period_ms == 0)
  {
    // Turn off override
    engineering_test_breath_period_ms = 0;
    engineering_test_breath_period_unlock = 0;
  }
}


///////////////////////////////
//
// Private functions
//
///////////////////////////////
static void BreathDetector_Task(tTaskParam param)
{
  U16                pressure_samples[4];
  S32                current_pressure;
  tBreathCalibration calibration;

  TASK_LOOP_START
  ////////////////////////////////////

    U32 count = AnalogInput_GetSamples(AIN_BREATH_PRESSURE, pressure_samples, sizeof(pressure_samples) / sizeof(*pressure_samples));

    time_since_last_breath += count;

    ConfigManager_GetBreathCalibration(&calibration);

    for (U32 i = 0; i < count; i++)
    {
      current_pressure = (S32)pressure_samples[i] - calibration.zero_offset;
      current_pressure = IirFilter_Iterate(&breath_filter, current_pressure);

      last_pressure = current_pressure;

      BOOL trigger = false;
      if (
          (engineering_test_breath_period_ms != 0)
          &&
          (engineering_test_breath_period_unlock == ENGINEERING_TEST_BREATH_PERIOD_UNLOCK_CODE)
      )
      {
        // Simulated breathing for engineering test mode is set and unlocked
        if (time_since_last_breath >= engineering_test_breath_period_ms)
        {
          // It is time to trigger a breath
          trigger = true;
        }
      }
      else if (current_pressure <= LEVEL_TO_FILTER_OUTPUT(calibration.trigger_level))
      {
        // Pressure is low enough to trigger. (Holdoff is checked below.)
        trigger = true;
      }

      if (trigger_holdoff)
      {
        trigger_holdoff--;
      }
      else if (trigger)
      {
        AlarmManager_ClearAlarm(ALARM_APNEA);
        DataManager_ReportBreath(time_since_last_breath);

        if (time_since_last_breath > MAX_BREATH_PERIOD)
        {
          // Too long since the previous breath. Use the default this time.
          time_since_last_breath = DEFAULT_BREATH_PERIOD;
        }
        else if (time_since_last_breath < MIN_BREATH_PERIOD)
        {
          time_since_last_breath = MIN_BREATH_PERIOD;
        }

        if (breath_detected_cb)
        {
          (*breath_detected_cb)(time_since_last_breath);
        }

        trigger_holdoff = HOLDOFF_DURATION_STANDARD;
        time_since_last_breath = 0;
      }
    }

    if (time_since_last_breath >= APNEA_DETECTION_PERIOD)
    {
      AlarmManager_ActivateAlarm(ALARM_APNEA);
      time_since_last_breath = APNEA_DETECTION_PERIOD;
    }

  ////////////////////////////////////
  TASK_LOOP_END
}


////////////////////////////////////////////////////////////////////////////////////////////////////
static BOOL  BreathDetector_ConsoleHandler(S32 argc, char* argv[], BOOL help)
//
//  Parameters:    argc  - The number of arguments passed in from the shell
//                         input, including the command itself.
//                 argv  - Array of string pointers to the arguments.
//                 help  - TRUE says the handler should print out verbose
//                         usage instructions.
//
//  Return:        TRUE    The shell command was handled OK.
//                 FALSE   The shell command was not understood.
//
//  Descriptions:
//  Called from Debug module's shell processor.  If FALSE is returned, the
//  handler will be called again passing TRUE in 'help', where the handler
//  can then print verbose usage instructions.
{
  if( help == FALSE )
  {
    tBreathCalibration cal;
    BOOL print_cal = false;

    ConfigManager_GetBreathCalibration(&cal);

    if (argc == 2)
    {
      Arg(1, "read")
      {
        S32 raw_counts = (last_pressure >> BREATH_FILTER_GAIN_SHIFT); // shift to eliminate filter gain
        S32 mPa        = (S32)(1000*ADC_UNITS_TO_PASCALS(raw_counts));

        // Add the offset back in to get back a (filtered) ADC value
        raw_counts += cal.zero_offset;

        Printf( "Breath Pressure: %d mPa (%d)\n", mPa, raw_counts);
      }
      else Arg(1, "showcal")
      {
        // Just print out below
        print_cal = true;
      }
      else Arg(1, "zero")
      {
        Printf("\nZero Offset was: %d\n\n", cal.zero_offset);
        cal.zero_offset = (last_pressure >> BREATH_FILTER_GAIN_SHIFT) + cal.zero_offset; // shift to eliminate filter gain, add back the old offset
        ConfigManager_SetBreathCalibration(&cal);
        print_cal = true;
      }
      else
      {
        return FALSE;
      }
    }
    else if (argc == 3)
    {
      S32 new_val = atoi(argv[2]);

      Arg(1, "settrigger")
      {
        Printf("\nTrigger level was: %d\n\n", cal.trigger_level);
        cal.trigger_level = (S32)PASCALS_TO_ADC_UNITS(((float)new_val / 1000));
        ConfigManager_SetBreathCalibration(&cal);
        print_cal = true;
      }
      else
      {
        return FALSE;
      }
    }
    else
    {
      return FALSE;
    }

    if (print_cal)
    {
      ConfigManager_GetBreathCalibration(&cal);
      Printf( "\n--------------------------------\n");
      Printf( "Breath Calibration\n");
      Printf( "--------------------------------\n");
      Printf( "Zero Offset (counts):  %d\n", cal.zero_offset);
      Printf( "Trigger Level (mPa):   %d\n", (S32)(1000*ADC_UNITS_TO_PASCALS(cal.trigger_level)));
      Printf( "--------------------------------\n\n");
    }
  }
  else
  {
    Printf( "breath { read | showcal | settrigger [level mPa]} | zero" );
  }

  return TRUE;
}

static void BreathDetector_UsbCommHandler(tUsbCommMessage *msg, tUsbCommMessage *response)
{
  tBreathCalibration cal;
  S32                values[3];
  U32                unlock_code;
  U32                period_ms;

  switch(msg->code)
  {
  case MESSAGE_CODE_GET_BREATH_CALIBRATION:
    ConfigManager_GetBreathCalibration(&cal);
    // Convert 32-bit values. Zero offset is still an integer. Others convert
    // to Pascals so the protocol is hardware independent.
    values[0] = cal.zero_offset;
    ((float*)values)[1] = ADC_UNITS_TO_PASCALS(cal.trigger_level);
    ((float*)values)[2] = ADC_UNITS_TO_PASCALS(cal.exhalation_detect_level);

    UsbCommMessage_BuildArrayResponse(
        response,
        MESSAGE_CODE_GET_BREATH_CALIBRATION,
        (U8*)values,
        sizeof(values)
    );
    break;

  case MESSAGE_CODE_SET_BREATH_ZERO_OFFSET:
    ConfigManager_GetBreathCalibration(&cal);

    // Return old value
    values[0] = cal.zero_offset;

    // Calculate the new value. Shift to eliminate filter gain, add back the old offset.
    cal.zero_offset = (last_pressure >> BREATH_FILTER_GAIN_SHIFT) + cal.zero_offset;

    ConfigManager_SetBreathCalibration(&cal);

    // Return new value
    values[1] = cal.zero_offset;

    UsbCommMessage_BuildArrayResponse(
        response,
        MESSAGE_CODE_SET_BREATH_ZERO_OFFSET,
        (U8*)values,
        2*sizeof(S32)
    );
    break;

  case MESSAGE_CODE_SET_BREATH_TRIGGER_LEVEL:
    ConfigManager_GetBreathCalibration(&cal);

    // Return old value
    ((float*)values)[0] = ADC_UNITS_TO_PASCALS(cal.trigger_level);

    // Convert provided float in Pascals to an integer in ADC units
    cal.trigger_level = PASCALS_TO_ADC_UNITS(((float*)&msg->payload_start)[0]);

    ConfigManager_SetBreathCalibration(&cal);

    // Return new value
    ((float*)values)[1] = ADC_UNITS_TO_PASCALS(cal.trigger_level);

    UsbCommMessage_BuildArrayResponse(
        response,
        MESSAGE_CODE_SET_BREATH_TRIGGER_LEVEL,
        (U8*)values,
        2*sizeof(float)
    );
    break;

  case MESSAGE_CODE_SET_BREATH_EXHALATION_LEVEL:
    ConfigManager_GetBreathCalibration(&cal);

    // Return old value
    ((float*)values)[0] = ADC_UNITS_TO_PASCALS(cal.exhalation_detect_level);

    // Convert provided float in Pascals to an integer in ADC units
    cal.exhalation_detect_level = PASCALS_TO_ADC_UNITS(((float*)&msg->payload_start)[0]);

    ConfigManager_SetBreathCalibration(&cal);

    // Return new value
    ((float*)values)[1] = ADC_UNITS_TO_PASCALS(cal.exhalation_detect_level);

    UsbCommMessage_BuildArrayResponse(
        response,
        MESSAGE_CODE_SET_BREATH_EXHALATION_LEVEL,
        (U8*)values,
        2*sizeof(float)
    );
    break;

  case MESSAGE_CODE_GET_BREATH_PRESSURE:
    // Convert ADC value to float in Pascals
    ((float*)values)[0] = ADC_UNITS_TO_PASCALS(last_pressure);

    UsbCommMessage_BuildArrayResponse(
        response,
        MESSAGE_CODE_GET_BREATH_PRESSURE,
        (U8*)values,
        sizeof(float)
    );
    break;

  case MESSAGE_CODE_GET_FAKE_BREATH_PERIOD:
    UsbCommMessage_BuildIntegerResponse(
        response,
        MESSAGE_CODE_GET_FAKE_BREATH_PERIOD,
        BreathDetector_GetEngineeringTestBreathPeriod()
    );
    break;

  case MESSAGE_CODE_SET_FAKE_BREATH_PERIOD:
    unlock_code = ((U32*)&msg->payload_start)[0];
    period_ms   = ((U32*)&msg->payload_start)[1];

    BreathDetector_SetEngineeringTestBreathPeriod(unlock_code, period_ms);

    UsbCommMessage_BuildIntegerResponse(
        response,
        MESSAGE_CODE_SET_FAKE_BREATH_PERIOD,
        BreathDetector_GetEngineeringTestBreathPeriod()
    );
    break;
  }
}
