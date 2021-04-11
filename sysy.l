%option noyywrap

%{
#include "sysy.tab.h"
%}

%%

 /* keywords */
"const" { return K_CONST; }
"int" { return K_INT; }
"void" { return K_VOID; }
"if" { return K_IF; }
"else" { return K_ELSE; }
"while" { return K_WHILE; }
"break" { return K_BREAK; }
"continue" { return K_CONTINUE; }
"return" { return K_RETURN; }

 /* integer constant */

[1-9][0-9]* {
  /* decimal */
  yylval.int_value = 0;
  for(int i = 0; yytext[i]; ++i) {
    yylval.int_value = yylval.int_value * 10 + yytext[i] - '0';
  }
  return INT_LITERAL;
}

0[0-7]* {
  /* octal, or zero */
  yylval.int_value = 0;
  for(int i = 1; yytext[i]; ++i) {
    yylval.int_value = yylval.int_value * 8 + yytext[i] - '0';
  }
  return INT_LITERAL;
}

0[xX][0-9a-fA-F]+ {
  /* hex */
  yylval.int_value = 0;
  for(int i = 2; yytext[i]; ++i) {
    int c = yytext[i];
    if(c >= '0' && c <= '9') c = c - '0';
    else if(c >= 'a' && c <= 'f') c = c - 'a' + 10;
    else c = c - 'A' + 10;
    yylval.int_value = yylval.int_value * 16 + c;
  }
  return INT_LITERAL;
}

 /* identifier */
[_a-zA-Z][0-9_a-zA-Z]* {
  yylval.ident_name = yytext;
  return IDENT;
}

 /* operators */
"+" { return OP_ADD; }
"-" { return OP_SUB; }
"*" { return OP_MUL; }
"/" { return OP_DIV; }
"%" { return OP_REM; }
"!" { return OP_NEG; }
"||" { return OP_LOR; }
"&&" { return OP_LAND; }
"==" { return OP_EQ; }
"!=" { return OP_NEQ; }
"<" { return OP_LT; }
">" { return OP_GT; }
"<=" { return OP_LE; }
">=" { return OP_GE; }

";" { return OP_SEMICOLON; }
"," { return OP_COMMA; }
"=" { return OP_ASSIGN; }

"[" { return OP_LBRACKET; }
"]" { return OP_RBRACKET; }

"{" { return OP_LBRACE; }
"}" { return OP_RBRACE; }

"(" { return OP_LPAREN; }
")" { return OP_RPAREN; }

 /* empty chars and comments */
[ \t\n]+ {}
\/\/[^\n]*/\n {}
\/\*([^\*]|\*[^\/])*\*\/ {}

 /* invalid chars */
. {
  yylval.error_char = yytext[0];
  return SP_LEX_ERROR;
}

%%
