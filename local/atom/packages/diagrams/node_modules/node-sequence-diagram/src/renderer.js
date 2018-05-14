var Diagram = require('./diagram.js');
var Svg = require('./svg.js');

// Following the CSS convention
// Margin is the gap outside the box
// Padding is the gap inside the box
// Each object has x/y/width/height properties
// The x/y should be top left corner
// width/height is with both margin and padding

var DIAGRAM_MARGIN = 10;

var ACTOR_MARGIN   = 10; // Margin around a actor
var ACTOR_PADDING  = 10; // Padding inside a actor

var SIGNAL_MARGIN  = 5; // Margin around a signal
var SIGNAL_PADDING = 5; // Padding inside a signal

var NOTE_MARGIN   = 10; // Margin around a note
var NOTE_PADDING  = 5; // Padding inside a note
var NOTE_OVERLAP  = 15; // Overlap when using a "note over A,B"

var TITLE_MARGIN   = 0;
var TITLE_PADDING  = 5;

var SELF_SIGNAL_WIDTH = 20; // How far out a self signal goes

var PLACEMENT = Diagram.PLACEMENT;
var LINETYPE  = Diagram.LINETYPE;
var ARROWTYPE = Diagram.ARROWTYPE;

/******************
* Drawing extras
******************/

function getCenterX(box) {
  return box.x + box.width / 2;
}

function getCenterY(box) {
  return box.y + box.height / 2;
}

/******************
* Renderer
******************/

var Renderer = function(diagram, stylesheet) {
  this.svg_ = new Svg(stylesheet);

  this.diagram = diagram;

  this._title  = undefined; // hack - This should be somewhere better

  this._actors_height  = 0;
  this._signals_height = 0;

  var a = this.arrow_types = {};
  a[ARROWTYPE.NONE] = 'none';
  a[ARROWTYPE.FILLED] = 'block';
  a[ARROWTYPE.OPEN]   = 'open';

  var l = this.line_types = {};
  l[LINETYPE.SOLID]  = '';
  l[LINETYPE.DOTTED] = '-';
};

Renderer.LINE_CLASS_ = [
  'signal-solid',
  'signal-dotted'
];

Renderer.ARROW_CLASS_ = [
  'arrow-none',
  'arrow-filled',
  'arrow-open'
];

Renderer.TEXT_BACKGROUND_CLASS_ = 'textbg';
Renderer.RECT_CLASS_ = 'rect';
Renderer.TIMELINE_CLASS_ = 'timeline';

Renderer.prototype.draw = function(container) {
  container.appendChild(this.svg_.getDocument());

  this.layout();
  this.svg_.setDocumentSize(this.diagram.width, this.diagram.height);

  var titleHeight = this._title ? this._title.height : 0;
  var y = DIAGRAM_MARGIN + titleHeight;

  this.draw_title();
  this.draw_actors(y);
  this.draw_signals(y + this._actors_height);
};

Renderer.prototype.layout = function() {
  // Local copies
  var diagram = this.diagram;
  var actors  = diagram.actors;
  var signals = diagram.signals;

  diagram.width = 0;  // min width
  diagram.height = 0; // min width

  // Setup some layout stuff
  if (diagram.title) {
    var title = this._title = {};
    var textNode = this.svg_.text(diagram.title);
    var bb = this.svg_.getElementSize(textNode);
    title.text_bb = bb;
    title.message = diagram.title;

    title.width  = bb.width  + (TITLE_PADDING + TITLE_MARGIN) * 2;
    title.height = bb.height + (TITLE_PADDING + TITLE_MARGIN) * 2;
    title.x = DIAGRAM_MARGIN;
    title.y = DIAGRAM_MARGIN;

    diagram.width  += title.width;
    diagram.height += title.height;
  }

  actors.forEach(function(a) {
    var textNode = this.svg_.text(a.name);
    var bb = this.svg_.getElementSize(textNode);
    a.text_bb = bb;

    a.x = 0; a.y = 0;
    a.width  = bb.width  + (ACTOR_PADDING + ACTOR_MARGIN) * 2;
    a.height = bb.height + (ACTOR_PADDING + ACTOR_MARGIN) * 2;

    a.distances = [];
    a.padding_right = 0;
    this._actors_height = Math.max(a.height, this._actors_height);
  }, this);

  function actor_ensure_distance(a, b, d) {
    console.assert(a < b, "a must be less than or equal to b");

    if (a < 0) {
      // Ensure b has left margin
      b = actors[b];
      b.x = Math.max(d - b.width / 2, b.x);
    } else if (b >= actors.length) {
      // Ensure a has right margin
      a = actors[a];
      a.padding_right = Math.max(d, a.padding_right);
    } else {
      a = actors[a];
      a.distances[b] = Math.max(d, a.distances[b] ? a.distances[b] : 0);
    }
  }

  signals.forEach(function(s) {
    var a, b; // Indexes of the left and right actors involved

    var textNode = this.svg_.text(s.message);
    var bb = this.svg_.getElementSize(textNode);

    s.text_bb = bb;
    s.width   = bb.width;
    s.height  = bb.height;

    var extra_width = 0;

    if (s.type == "Signal") {

      s.width  += (SIGNAL_MARGIN + SIGNAL_PADDING) * 2;
      s.height += (SIGNAL_MARGIN + SIGNAL_PADDING) * 2;

      if (s.isSelf()) {
        a = s.actorA.index;
        b = a + 1;
        s.width += SELF_SIGNAL_WIDTH;
      } else {
        a = Math.min(s.actorA.index, s.actorB.index);
        b = Math.max(s.actorA.index, s.actorB.index);
      }

    } else if (s.type == "Note") {
      s.width  += (NOTE_MARGIN + NOTE_PADDING) * 2;
      s.height += (NOTE_MARGIN + NOTE_PADDING) * 2;

      // HACK lets include the actor's padding
      extra_width = 2 * ACTOR_MARGIN;

      if (s.placement == PLACEMENT.LEFTOF) {
        b = s.actor.index;
        a = b - 1;
      } else if (s.placement == PLACEMENT.RIGHTOF) {
        a = s.actor.index;
        b = a + 1;
      } else if (s.placement == PLACEMENT.OVER && s.hasManyActors()) {
        // Over multiple actors
        a = Math.min(s.actor[0].index, s.actor[1].index);
        b = Math.max(s.actor[0].index, s.actor[1].index);

        // We don't need our padding, and we want to overlap
        extra_width = - (NOTE_PADDING * 2 + NOTE_OVERLAP * 2);

      } else if (s.placement == PLACEMENT.OVER) {
        // Over single actor
        a = s.actor.index;
        actor_ensure_distance(a - 1, a, s.width / 2);
        actor_ensure_distance(a, a + 1, s.width / 2);
        this._signals_height += s.height;

        return; // Bail out early
      }
    } else {
      throw new Error("Unhandled signal type:" + s.type);
    }

    actor_ensure_distance(a, b, s.width + extra_width);
    this._signals_height += s.height;
  }, this);

  // Re-jig the positions
  var actors_x = 0;
  actors.forEach(function(a) {
    a.x = Math.max(actors_x, a.x);

    // TODO This only works if we loop in sequence, 0, 1, 2, etc
    a.distances.forEach(function(distance, b) {
      b = actors[b];
      distance = Math.max(distance, a.width / 2, b.width / 2);
      b.x = Math.max(b.x, a.x + a.width/2 + distance - b.width/2);
    });

    actors_x = a.x + a.width + a.padding_right;
  }, this);

  diagram.width = Math.max(actors_x, diagram.width);

  // TODO Refactor a little
  diagram.width  += 2 * DIAGRAM_MARGIN;
  diagram.height += 2 * DIAGRAM_MARGIN + 2 * this._actors_height + this._signals_height;

  return this;
};

Renderer.prototype.draw_title = function() {
  var title = this._title;
  if (title)
    this.draw_text_box(title, title.message, TITLE_MARGIN, TITLE_PADDING);
};

Renderer.prototype.draw_actors = function(offsetY) {
  var y = offsetY;
  this.diagram.actors.forEach(function(a) {
    // Top box
    this.draw_actor(a, y, this._actors_height);

    // Bottom box
    this.draw_actor(a, y + this._actors_height + this._signals_height, this._actors_height);

    // Vertical line
    var aX = getCenterX(a);
    var line = this.svg_.path('M{0},{1} v{2}',
        aX,
        y + this._actors_height - ACTOR_MARGIN,
        2 * ACTOR_MARGIN + this._signals_height);

    line.classList.add(Renderer.TIMELINE_CLASS_);
    this.svg_.getDocument().appendChild(line);
  }, this);
};

Renderer.prototype.draw_actor = function (actor, offsetY, height) {
  actor.y      = offsetY;
  actor.height = height;
  this.draw_text_box(actor, actor.name, ACTOR_MARGIN, ACTOR_PADDING);
};

Renderer.prototype.draw_signals = function (offsetY) {
  var y = offsetY;
  this.diagram.signals.forEach(function(s) {
    if (s.type == "Signal") {
      if (s.isSelf()) {
        this.draw_self_signal(s, y);
      } else {
        this.draw_signal(s, y);
      }

    } else if (s.type == "Note") {
      this.draw_note(s, y);
    }

    y += s.height;
  }, this);
};

Renderer.prototype.draw_self_signal = function(signal, offsetY) {
  console.assert(signal.isSelf(), "signal must be a self signal");

  var text_bb = signal.text_bb;
  var aX = getCenterX(signal.actorA);

  var x = aX + SELF_SIGNAL_WIDTH + SIGNAL_PADDING + text_bb.width / 2;
  var y = offsetY + signal.height / 2;

  this.draw_text(x, y, signal.message);

  // 3 segment polyline.
  var line = this.svg_.path("M{0},{1} h{2} v{3} h{4}", aX, offsetY + SIGNAL_MARGIN,
      SELF_SIGNAL_WIDTH,
      signal.height - SIGNAL_MARGIN,
      -SELF_SIGNAL_WIDTH);

  line.classList.add(Renderer.LINE_CLASS_[signal.linetype]);
  line.classList.add(Renderer.ARROW_CLASS_[signal.arrowtype]);
  this.svg_.getDocument().appendChild(line);
};

Renderer.prototype.draw_signal = function (signal, offsetY) {
  var aX = getCenterX( signal.actorA );
  var bX = getCenterX( signal.actorB );

  // Mid point between actors
  var x = (bX - aX) / 2 + aX;
  var y = offsetY + SIGNAL_MARGIN + 2*SIGNAL_PADDING;

  // Draw the text in the middle of the signal
  this.draw_text(x, y, signal.message);

  // Draw the line along the bottom of the signal
  y = offsetY + signal.height - SIGNAL_MARGIN - SIGNAL_PADDING;
  var line = this.svg_.path('M{0},{1} h{2}', aX, y, (bX - aX));

  line.classList.add(Renderer.LINE_CLASS_[signal.linetype]);
  line.classList.add(Renderer.ARROW_CLASS_[signal.arrowtype]);
  this.svg_.getDocument().appendChild(line);
};

Renderer.prototype.draw_note = function (note, offsetY) {
  note.y = offsetY;
  var actorA = note.hasManyActors() ? note.actor[0] : note.actor;
  var aX = getCenterX( actorA );
  switch (note.placement) {
    case PLACEMENT.RIGHTOF:
      note.x = aX + ACTOR_MARGIN;
      break;
    case PLACEMENT.LEFTOF:
      note.x = aX - ACTOR_MARGIN - note.width;
      break;
    case PLACEMENT.OVER:
      if (note.hasManyActors()) {
        var bX = getCenterX( note.actor[1] );
        var overlap = NOTE_OVERLAP + NOTE_PADDING;
        note.x = aX - overlap;
        note.width = (bX + overlap) - note.x;
      } else {
        note.x = aX - note.width / 2;
      }
      break;
    default:
      throw new Error("Unhandled note placement:" + note.placement);
  }

  this.draw_text_box(note, note.message, NOTE_MARGIN, NOTE_PADDING);
};

/**
 * Draws text with a white background
 * x,y (int) x,y center point for this text
 * TODO Horz center the text when it's multi-line print
 */
Renderer.prototype.draw_text = function (x, y, text, opt_dontDrawBox) {
  var t = this.svg_.text(text);
  t.setAttribute('x', x);
  t.setAttribute('y', y);
  t.style.textAnchor = 'middle';
  t.style.alignmentBaseline = 'central';

  if (!opt_dontDrawBox) {
    var bb = this.svg_.getElementSize(t);
    var r = this.svg_.rect(bb.width, bb.height);
    r.setAttribute('x', bb.x);
    r.setAttribute('y', bb.y);
    r.classList.add(Renderer.TEXT_BACKGROUND_CLASS_);
    this.svg_.getDocument().appendChild(r);
  }

  this.svg_.getDocument().appendChild(t);
};

Renderer.prototype.draw_text_box = function (box, text, margin, padding) {
  var x = box.x + margin;
  var y = box.y + margin;
  var w = box.width  - 2 * margin;
  var h = box.height - 2 * margin;

  // Draw inner box
  var rect = this.svg_.rect(w, h);
  rect.setAttribute('x', x);
  rect.setAttribute('y', y);
  rect.classList.add(Renderer.RECT_CLASS_);
  this.svg_.getDocument().appendChild(rect);

  // Draw text (in the center)
  x = getCenterX(box);
  y = getCenterY(box);

  this.draw_text(x, y, text, true /* opt_dontDrawBox */);
};

module.exports = Renderer;
