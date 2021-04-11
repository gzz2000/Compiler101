%{
int yylex();
void yyerror(const char *s);
%}

%union {
  char error_char;
  int int_value;
  const char *ident_name;
}

%token <error_char> SP_LEX_ERROR

%token K_CONST
%token K_INT
%token K_VOID
%token K_IF
%token K_ELSE
%token K_WHILE
%token K_BREAK
%token K_CONTINUE
%token K_RETURN

%token <int_value> INT_LITERAL
%token <ident_name> IDENT

%token OP_ADD   /*  +  */
%token OP_SUB   /*  -  */
%token OP_MUL   /*  *  */
%token OP_DIV   /*  /  */
%token OP_REM   /*  %  */
%token OP_NEG   /*  !  */
%token OP_LOR    /* || */
%token OP_LAND    /* && */
%token OP_EQ     /* == */
%token OP_NEQ     /* != */
%token OP_LT   /*  <  */
%token OP_GT   /*  >  */
%token OP_LE  /* <= */
%token OP_GE  /* >= */

%token OP_SEMICOLON   /*  ;  */
%token OP_COMMA   /*  ,  */
%token OP_ASSIGN   /*  =  */

%token OP_LBRACKET   /*  [  */
%token OP_RBRACKET   /*  ]  */

%token OP_LBRACE   /*  {  */
%token OP_RBRACE   /*  }  */

%token OP_LPAREN   /*  (  */
%token OP_RPAREN   /*  )  */

%%

/* CompUnit ::= [CompUnit] (Decl | FuncDef); */
CompUnit
: {}
| CompUnit DeclOrFuncDef {}
;

DeclOrFuncDef
: Decl {}
| FuncDef {}
;

/* Decl ::= ConstDecl | VarDecl; */
Decl
: ConstDecl {}
| VarDecl {}
;

/* ConstDecl ::= "const" BType ConstDef {"," ConstDef} ";"; */
ConstDecl
: K_CONST Types ConstDefList OP_SEMICOLON {}
;

Types
: K_INT {}
| K_VOID {}
;

ConstDefList
: ConstDef {}
| ConstDefList OP_COMMA ConstDef {}
;

/* ConstDef ::= IDENT {"[" ConstExp "]"} "=" ConstInitVal; */
ConstDef
: IDENT DefArrayDimensions OP_ASSIGN ConstInitVal {}
;

DefArrayDimensions
: {} /* epsilon */
| OP_LBRACKET ConstExp OP_RBRACKET {}
;

/* ConstInitVal ::= ConstExp | "{" [ConstInitVal {"," ConstInitVal}] "}"; */
ConstInitVal
: ConstExp {}
| OP_LBRACE ConstInitValList OP_RBRACE {}
;

ConstInitValList
: ConstInitVal {}
| ConstInitValList OP_COMMA ConstInitVal {}
;

/* VarDecl ::= BType VarDef {"," VarDef} ";" */
VarDecl
: Types VarDefList OP_SEMICOLON {}
;

VarDefList
: VarDef {}
| VarDefList OP_COMMA VarDef {}
;

/* VarDef ::= IDENT {"[" ConstExp "]"} ["=" InitVal] */
VarDef
: IDENT DefArrayDimensions {}
| IDENT DefArrayDimensions OP_ASSIGN InitVal {}
;

/* InitVal ::= Exp | "{" [InitVal {"," InitVal}] "}"; */
InitVal
: Exp {}
| OP_LBRACE InitValList OP_RBRACE {}
;

InitValList
: InitVal {}
| InitValList OP_COMMA InitVal {}
;

/* FuncDef ::= FuncType IDENT "(" [FuncFParams] ")" Block; */
FuncDef
: Types IDENT OP_LPAREN FuncFParamsOptional OP_RPAREN Block {}
;

FuncFParamsOptional
: {}
| FuncFParams {}
;

/* FuncFParams ::= FuncFParam {"," FuncFParam}; */
FuncFParams
: FuncFParam {}
| FuncFParams OP_COMMA FuncFParam {}
;

/* FuncFParam ::= BType IDENT ["[" "]" {"[" ConstExp "]"}]; */
FuncFParam
: Types IDENT {}
| Types IDENT OP_LBRACKET OP_RBRACKET DefArrayDimensions {}
;

/* Block ::= "{" {BlockItem} "}"; */
Block
: OP_LBRACE BlockItems OP_RBRACE {}
;

BlockItems
: {}
| BlockItems BlockItem {}
;

/* BlockItem ::= Decl | Stmt; */
BlockItem
: Decl {}
| Stmt {}
;

/* Stmt ::= LVal "=" Exp ";" */
/*        | [Exp] ";" */
/*        | Block */
/*        | "if" "(" Cond ")" Stmt ["else" Stmt] */
/*        | "while" "(" Cond ")" Stmt */
/*        | "break" ";" */
/*        | "continue" ";" */
/*        | "return" [Exp] ";"; */
Stmt
: OP_SEMICOLON {}
| LVal OP_ASSIGN Exp OP_SEMICOLON {}
| Exp OP_SEMICOLON {}
| Block {}
| K_IF OP_LPAREN Exp OP_RPAREN Stmt {}
| K_WHILE OP_LPAREN Exp OP_RPAREN Stmt {}
| K_BREAK OP_SEMICOLON {}
| K_CONTINUE OP_SEMICOLON {}
| K_RETURN OP_SEMICOLON {}
| K_RETURN Exp OP_SEMICOLON {}
;

/* ConstExp ::= AddExp; */
/* Caveat: This can only be checked in semantic analysis. */
/* I changed the definition to ConstExp ::= Exp; for easy parsing. */
ConstExp
: Exp {}
;

/* Exp definitions. I extended it so we can now mix logic and arithmetic calculations */
Exp
: LOrExp {}
;

LOrExp
: LAndExp {}
| LOrExp OP_LOR LAndExp {}
;

LAndExp
: EqExp {}
| LAndExp OP_LAND EqExp {}
;

EqExp
: RelExp {}
| EqExp OP_EQ RelExp {}
| EqExp OP_NEQ RelExp {}
;

RelExp
: AddExp {}
| RelExp OP_LT AddExp {}
| RelExp OP_GT AddExp {}
| RelExp OP_LE AddExp {}
| RelExp OP_GE AddExp {}
;

AddExp
: MulExp {}
| AddExp OP_ADD MulExp {}
| AddExp OP_SUB MulExp {}
;

MulExp
: UnaryExp {}
| MulExp OP_MUL UnaryExp {}
| MulExp OP_DIV UnaryExp {}
| MulExp OP_REM UnaryExp {}
;

UnaryExp
: PrimaryExp {}
| IDENT OP_LPAREN FuncRParamsOptional OP_RPAREN {}
| OP_ADD UnaryExp {}
| OP_SUB UnaryExp {}
| OP_NEG UnaryExp {}
;

FuncRParamsOptional
: {}
| FuncRParams {}
;

FuncRParams
: Exp {}
| FuncRParams OP_COMMA Exp {}
;

PrimaryExp
: OP_LPAREN Exp OP_RPAREN {}
| LVal {}
| INT_LITERAL {}
;

LVal
: IDENT RefArrayDimensions {}
;

RefArrayDimensions
: {}
| RefArrayDimensions OP_LBRACKET Exp OP_RBRACKET {}
;

%%
