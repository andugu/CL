//////////////////////////////////////////////////////////////////////
//
//    Asl - Another simple language (grammar)
//
//    Copyright (C) 2017  Universitat Politecnica de Catalunya
//
//    This library is free software; you can redistribute it and/or
//    modify it under the terms of the GNU General Public License
//    as published by the Free Software Foundation; either version 3
//    of the License, or (at your option) any later version.
//
//    This library is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//    Affero General Public License for more details.
//
//    You should have received a copy of the GNU Affero General Public
//    License along with this library; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
//    contact: José Miguel Rivero (rivero@cs.upc.edu)
//             Computer Science Department
//             Universitat Politecnica de Catalunya
//             despatx Omega.110 - Campus Nord UPC
//             08034 Barcelona.  SPAIN
//
//////////////////////////////////////////////////////////////////////

grammar Asl;

//////////////////////////////////////////////////
/// Parser Rules
//////////////////////////////////////////////////

// A program is a list of functions
program : function+ EOF
        ;

// A function has a name, a list of parameters and a list of statements
function
        : FUNC ID '(' ')' declarations statements ENDFUNC
        ;

declarations
        : (variable_decl)*
        ;

variable_decl
        : VAR ID (COMMA ID)* ':' type
        ;

type2    : INT
        | FLOAT
        | BOOL
        | CHAR
        ;

type    : type2                          # BasicType
        | ARRAY '[' INTVAL ']' OF type2  # ArrayType
        ;


statements
        : (statement)*
        ;

// The different types of instructions
statement
          // Assignment
        : left_expr ASSIGN expr ';'           # assignStmt
          // if-then-else statement (else is optional)
        | IF expr THEN statements ENDIF       # ifStmt
          // A function/procedure call has a list of arguments in parenthesis (possibly empty)
        | ident '(' ')' ';'                   # procCall
          // Read a variable
        | READ left_expr ';'                  # readStmt
          // Write an expression
        | WRITE expr ';'                      # writeExpr
          // Write a string
        | WRITE STRING ';'                    # writeString
        ;

// Grammar for left expressions (l-values in C++)
left_expr
        : ident
        ;

// Grammar for expressions with boolean, relational and aritmetic operators
expr    : '(' expr ')'                                                          # parentesisExpr
        | ident '[' INTVAL ']'                                                  # value
        | (NOT|SUB|PLUS) expr                                                   # value
        | expr op=(MUL|DIV|MOD) expr                                            # arithmetic
        | expr op=(PLUS|SUB) expr                                               # arithmetic
        | expr op=(EQUAL|NOTEQUAL|LESS|LESSEQUAL|GREATER|GREATEREQUAL) expr     # relational
        | expr op=AND expr                                                      # relational
        | expr op=OR  expr                                                      # relational
        | INTVAL                                                                # intValue
        | FLOATVAL                                                              # floatValue
        | CHARVAL                                                               # charValue
        | ident                                                                 # exprIdent
        ;

ident   : ID
        ;

//////////////////////////////////////////////////
/// Lexer Rules
//////////////////////////////////////////////////

// BOOLEAN OPERATORS 
ASSIGN         : '=' ;
EQUAL          : '==' ;
NOTEQUAL       : '!=' ;
GREATER        : '>' ;
GREATEREQUAL   : '>=' ;
LESS           : '<' ;
LESSEQUAL      : '<=' ;
NOT            : 'not';
AND            : 'and';
OR             : 'or';


OF             : 'of';

// ARITHMETIC OPERATORS 
PLUS      : '+' ;
SUB       : '-' ;
DIV       : '/' ;
MUL       : '*' ;
MOD       : '%' ;

VAR       : 'var';
INT       : 'int';
FLOAT     : 'float';
BOOL      : 'bool';
CHAR      : 'char';
IF        : 'if' ;
THEN      : 'then' ;
ELSE      : 'else' ;
ENDIF     : 'endif' ;
FUNC      : 'func' ;
ENDFUNC   : 'endfunc' ;
READ      : 'read' ;
WRITE     : 'write' ;
COMMA     : ',';
ARRAY     : 'array';
ID        : ('a'..'z'|'A'..'Z'|'_') ('a'..'z'|'A'..'Z'|'_'|'0'..'9')* ;

// VALUES 
INTVAL    : ('0'..'9')+ ;
FLOATVAL  : ('0'..'9')+ '.' ('0'..'9')* 
            | '.'  ('0'..'9')+; 
CHARVAL   : '\'' (ESC_SEQ | ~('\\'|'"')) '\'';

// Strings (in quotes) with escape sequences
STRING    : '"' ( ESC_SEQ | ~('\\'|'"') )* '"' ;

fragment
ESC_SEQ   : '\\' ('b'|'t'|'n'|'f'|'r'|'"'|'\''|'\\') ;

// Comments (inline C++-style)
COMMENT   : '//' ~('\n'|'\r')* '\r'? '\n' -> skip ;

// White spaces
WS        : (' '|'\t'|'\r'|'\n')+ -> skip ;
// Alternative description
// WS        : [ \t\r\n]+ -> skip ;
