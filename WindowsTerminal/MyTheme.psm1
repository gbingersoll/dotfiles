#requires -Version 2 -Modules posh-git

function Get-OSPathSeparator {
    return [System.IO.Path]::DirectorySeparatorChar
}

function Write-Theme {

    param(
        [bool]
        $lastCommandFailed,
        [string]
        $with
    )

    $lastColor = $sl.Colors.PromptBackgroundColor

    $prompt = ''

    $path = (Get-FullPath -dir $pwd).Split((Get-OSPathSeparator))
    If ($path.Count -gt 3) {
      $path = $path[0],$path[1],"...",$path[-1]
    }

    $inVirtualEnv = (Test-VirtualEnv)
    If ($inVirtualEnv) {
      $prompt += Write-Prompt -Object " $(Get-VirtualEnvName) " -ForegroundColor $sl.Colors.VirtualEnvForegroundColor -BackgroundColor $sl.Colors.VirtualEnvBackgroundColor
      $lastColor = $sl.Colors.VirtualEnvBackgroundColor

      If ($path[0] -eq "~") {
        $firstPathColor = $sl.Colors.HomeBackgroundColor
      } Else {
        $firstPathColor = $sl.Colors.PathBackgroundColor
      }
      $prompt += Write-Prompt -Object $sl.PromptSymbols.SegmentForwardSymbol -ForegroundColor $lastColor -BackgroundColor $firstPathColor
    }

    # Writes the drive portion
    $first = $TRUE
    while ($path.Count -ne 0)
    {
      $pathNext, $path = $path

      If ($first -AND $pathNext -eq "~") {
        $first = $FALSE
        $lastColor = $sl.Colors.HomeBackgroundColor
        $prompt += Write-Prompt -Object " $($pathNext) " -ForegroundColor $sl.Colors.HomeForegroundColor -BackgroundColor $sl.Colors.HomeBackgroundColor
        If ($path.Count -ne 0) {
          $prompt += Write-Prompt -Object $sl.PromptSymbols.SegmentForwardSymbol -ForegroundColor $sl.Colors.HomeBackgroundColor -BackgroundColor $sl.Colors.PathBackgroundColor
        }
      } Else {
        $lastColor = $sl.Colors.PathBackgroundColor
        $prompt += Write-Prompt -Object " $($pathNext) " -ForegroundColor $sl.Colors.PathForegroundColor -BackgroundColor $sl.Colors.PathBackgroundColor
        If ($path.Count -ne 0) {
          $prompt += Write-Prompt -Object $sl.PromptSymbols.PathForwardSymbol -ForegroundColor $sl.Colors.PathForegroundColor -BackgroundColor $sl.Colors.PathBackgroundColor
        }
      }
    }

    $status = Get-VCSStatus
    if ($status) {
        $themeInfo = Get-VcsInfo -status ($status)
        $prompt += Write-Prompt -Object $sl.PromptSymbols.SegmentForwardSymbol -ForegroundColor $lastColor -BackgroundColor $themeInfo.BackgroundColor
        $lastColor = $themeInfo.BackgroundColor
        $prompt += Write-Prompt -Object " $($themeInfo.VcInfo) " -BackgroundColor $lastColor -ForegroundColor $sl.Colors.GitForegroundColor
    }

    if ($with) {
        $prompt += Write-Prompt -Object $sl.PromptSymbols.SegmentForwardSymbol -ForegroundColor $lastColor -BackgroundColor $sl.Colors.WithBackgroundColor
        $prompt += Write-Prompt -Object " $($with.ToUpper()) " -BackgroundColor $sl.Colors.WithBackgroundColor -ForegroundColor $sl.Colors.WithForegroundColor
        $lastColor = $sl.Colors.WithBackgroundColor
    }

    # Writes the postfix to the prompt
    $postfixBackgroundColor = [System.ConsoleColor]::DarkGray
    $postfixForegroundColor = [System.ConsoleColor]::Gray
    $postfixHighlightColor = [System.ConsoleColor]::Gray
    If ($lastCommandFailed) {
      $postfixBackgroundColor = [System.ConsoleColor]::DarkRed
      $postfixForegroundColor = [System.ConsoleColor]::Black
      $postfixHighlightColor = [System.ConsoleColor]::Red
    }
    $prompt += Write-Prompt -Object $sl.PromptSymbols.SegmentForwardSymbol -ForegroundColor $lastColor -BackgroundColor $postfixBackgroundColor
    $prompt += Write-Prompt -Object ' % ' -ForegroundColor $postfixForegroundColor -BackgroundColor $postfixBackgroundColor
    $prompt += Write-Prompt -Object $sl.PromptSymbols.SegmentForwardSymbol -ForegroundColor $postfixHighlightColor
    $prompt += ' '
    $prompt
}

$sl = $global:ThemeSettings #local settings
$sl.PromptSymbols.SegmentForwardSymbol = [char]::ConvertFromUtf32(0xE0B0)
$sl.Colors.PromptForegroundColor = [ConsoleColor]::White
$sl.Colors.PromptSymbolColor = [ConsoleColor]::White
$sl.Colors.PromptHighlightColor = [ConsoleColor]::DarkBlue
$sl.Colors.GitForegroundColor = [ConsoleColor]::Black
$sl.Colors.WithForegroundColor = [ConsoleColor]::White
$sl.Colors.WithBackgroundColor = [ConsoleColor]::DarkRed
$sl.Colors.VirtualEnvBackgroundColor = [System.ConsoleColor]::Black
$sl.Colors.VirtualEnvForegroundColor = [System.ConsoleColor]::Green

$sl.PromptSymbols.PathForwardSymbol = [char]::ConvertFromUtf32(0xE0B1)
$sl.Colors.HomeBackgroundColor = [System.ConsoleColor]::Blue
$sl.Colors.HomeForegroundColor = [System.ConsoleColor]::Yellow
$sl.Colors.PathBackgroundColor = [System.ConsoleColor]::DarkGray
$sl.Colors.PathForegroundColor = [System.ConsoleColor]::Gray
