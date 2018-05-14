%{
var Diagram = require('./diagram.js');
%}

%lex

%options case-insensitive

%%

[\r\n]+           return 'NL';
\s+               /* skip whitespace */
\#[^\r\n]*        /* skip comments */
"participant"     return 'participant';
"left of"         return 'left_of';
"right of"        return 'right_of';
"over"            return 'over';
"note"            return 'note';
"title"           return 'title';
","               return ',';
[^\->:,\r\n]+     return 'ACTOR';
"--"              return 'DOTLINE';
"-"               return 'LINE';
">>"              return 'OPENARROW';
">"               return 'ARROW';
:[^\r\n]+         return 'MESSAGE';
<<EOF>>           return 'EOF';
.                 return 'INVALID';

/lex

%start document

%% /* language grammar */

document
    : newlines title_section alias_section sequence_section 'EOF' { return yy.parser.yy; }
    ;

title_section
    : 'title' message newlines      { yy.parser.yy.setTitle($2); }
    | /* empty */
    ;

alias_section
    : alias_section 'participant' actor_alias newlines { $3; }
    | /* empty */
    ;

sequence_section
    : sequence_section signal newlines           { yy.parser.yy.addSignal($2); }
    | sequence_section note_statement newlines   { yy.parser.yy.addSignal($2); }
    | /* empty */
    ;

note_statement
    : 'note' placement actor message   { $$ = new Diagram.Note($3, $2, $4); }
    | 'note' 'over' actor_pair message { $$ = new Diagram.Note($3, Diagram.PLACEMENT.OVER, $4); }
    ;

actor_pair
    : actor             { $$ = $1; }
    | actor ',' actor   { $$ = [$1, $3]; }
    ;

placement
    : 'left_of'   { $$ = Diagram.PLACEMENT.LEFTOF; }
    | 'right_of'  { $$ = Diagram.PLACEMENT.RIGHTOF; }
    ;

signal
    : actor signaltype actor message
    { $$ = new Diagram.Signal($1, $2, $3, $4); }
    ;

actor
    : ACTOR { $$ = yy.parser.yy.getActor(Diagram.unescape($1)); }
    ;

actor_alias
    : ACTOR { $$ = yy.parser.yy.addAlias(Diagram.unescape($1)); }
    ;

signaltype
    : linetype arrowtype  { $$ = $1 | ($2 << 2); }
    | linetype            { $$ = $1; }
    ;

linetype
    : LINE      { $$ = Diagram.LINETYPE.SOLID; }
    | DOTLINE   { $$ = Diagram.LINETYPE.DOTTED; }
    ;

arrowtype
    : ARROW     { $$ = Diagram.ARROWTYPE.FILLED; }
    | OPENARROW { $$ = Diagram.ARROWTYPE.OPEN; }
    ;

message
    : MESSAGE { $$ = Diagram.unescape($1.substring(1)); }
    ;

newlines
    : 'NL' newlines
    | /* empty */
    ;

%%
