var NS = 'http://www.w3.org/2000/svg';

var Svg = function(stylesheet) {
  this.root_ = document.createElementNS(NS, 'svg');

  var defs = document.createElementNS(NS, 'defs');

  var filledArrow = document.createElementNS(NS, 'marker');
  var filledArrowPath = this.path('M0,0 5,2.5 0,5z');

  var openArrow = document.createElementNS(NS, 'marker');
  var openArrowPath = this.path('M0,0 5,2.5 0,5');

  filledArrow.setAttribute('id', 'arrow-filled');
  filledArrow.setAttribute('refX', '5');
  filledArrow.setAttribute('refY', '2.5');
  filledArrow.setAttribute('markerWidth', '5');
  filledArrow.setAttribute('markerHeight', '5');
  filledArrow.setAttribute('orient', 'auto');
  filledArrowPath.setAttribute('style', 'stroke: none; fill: #000');

  openArrow.setAttribute('id', 'arrow-open');
  openArrow.setAttribute('refX', '5');
  openArrow.setAttribute('refY', '2.5');
  openArrow.setAttribute('markerWidth', '5');
  openArrow.setAttribute('markerHeight', '5');
  openArrow.setAttribute('orient', 'auto');
  openArrowPath.setAttribute('style', 'stroke-width: 1');

  this.root_.appendChild(stylesheet);
  this.root_.appendChild(defs);
  defs.appendChild(filledArrow);
  filledArrow.appendChild(filledArrowPath);
  defs.appendChild(openArrow);
  openArrow.appendChild(openArrowPath);
};

Svg.prototype.getDocument = function() {
  return this.root_;
};

Svg.prototype.setDocumentSize = function(width, height) {
  this.root_.setAttribute('width', width);
  this.root_.setAttribute('height', height);
};

Svg.prototype.getElementSize = function(element) {
  this.root_.appendChild(element);
  var boundingBox = element.getBBox();
  element.parentNode.removeChild(element);

  return boundingBox;
};

// -- Element Constructors --

Svg.prototype.group = function() {
  var group = document.createElementNS(NS, 'g');
  for (var i = 0; i < arguments.length; ++i) {
    group.appendChild(arguments[i]);
  }
  return group;
};

Svg.prototype.rect = function(width, height) {
  var rect = document.createElementNS(NS, 'rect');
  rect.setAttribute('width', width);
  rect.setAttribute('height', height);
  return rect;
};

Svg.prototype.text = function(message) {
  var text = document.createElementNS(NS, 'text');
  text.textContent = message;
  return text;
};

Svg.prototype.path = function(format) {
  var args = arguments;
  var pathSpec = format.replace(/\{(\d+)\}/g, function(string, index) {
    return args[++index];
  });

  var path = document.createElementNS(NS, 'path');
  path.setAttribute('d', pathSpec);

  return path;
};

module.exports = Svg;
