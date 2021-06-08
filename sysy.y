%{
#include "sysy.hpp"

int yylex();
void yyerror(std::shared_ptr<ast_nodebase>, const char *s);

using std::make_shared;

#define YYSTYPE std::shared_ptr<ast_nodebase>

/* ltag: for support of short-circuit logical operators */
#define concat_op2(ret, aval, bval, optype) {    \
    auto r = make_shared<ast_exp_op>();          \
    r->op = optype;                              \
    r->numop = 2;                                \
    r->a = dcast<ast_exp>(aval);                 \
    r->b = dcast<ast_exp>(bval);                 \
    ret = r;                                     \
  }
#define concat_op1(ret, aval, optype) {          \
    auto r = make_shared<ast_exp_op>();          \
    r->op = optype;                              \
    r->numop = 1;                                \
    r->a = dcast<ast_exp>(aval);                 \
    ret = r;                                     \
  }
%}

%parse-param {std::shared_ptr<ast_compunit> &root_store}

%token SP_LEX_ERROR

%token K_CONST
%token K_INT
%token K_VOID
%token K_IF
%token K_ELSE
%token K_WHILE
%token K_BREAK
%token K_CONTINUE
%token K_RETURN

%token INT_LITERAL
%token IDENT

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
/* Decl ::= ConstDecl | VarDecl; */
CompUnit
: {
  root_store = make_shared<ast_compunit>();
  $$ = root_store;
 }
| CompUnit ConstDecl {
  $$ = std::move($1);
  append_move(
    dcast<ast_compunit>($$)->defs,
    dcast<ast_constdecl>($2)->constdefs);
 }
| CompUnit VarDecl {
  $$ = std::move($1);
  append_move(
    dcast<ast_compunit>($$)->defs,
    dcast<ast_decl>($2)->defs);
 }
| CompUnit FuncDef {
  $$ = std::move($1);
  dcast<ast_compunit>($$)->funcdefs.push_back(
    dcast<ast_funcdef>($2));
 }
;

/* ConstDecl ::= "const" BType ConstDef {"," ConstDef} ";"; */
ConstDecl
: K_CONST Types ConstDefList OP_SEMICOLON {
  if(dcast<ast_term_generic>($2)->type == K_VOID) {
    yyerror(nullptr, "Definition cannot be void");
  }
  else {
    $$ = std::move($3);
  }
 }
;

Types
: K_INT {
  $$ = make_shared<ast_term_generic>(K_INT);
 }
| K_VOID {
  $$ = make_shared<ast_term_generic>(K_VOID);
 }
;

ConstDefList
: ConstDef {
  auto r = make_shared<ast_constdecl>();
  r->constdefs.push_back(dcast<ast_constdef>($1));
  $$ = r;
 }
| ConstDefList OP_COMMA ConstDef {
  $$ = std::move($1);
  dcast<ast_constdecl>($$)->constdefs.push_back(dcast<ast_constdef>($3));
 }
;

/* ConstDef ::= IDENT {"[" ConstExp "]"} "=" ConstInitVal; */
ConstDef
: IDENT DefArrayDimensions OP_ASSIGN InitVal {
  $$ = make_shared<ast_constdef>(
    std::move(dcast<ast_term_ident>($1)->name),
    std::move(dcast<ast_defarraydimensions>($2)->dims),
    dcast<ast_initval>($4));
 }
;

DefArrayDimensions
: {
  $$ = make_shared<ast_defarraydimensions>();
 }
| DefArrayDimensions OP_LBRACKET Exp OP_RBRACKET {
  $$ = std::move($1);
  dcast<ast_defarraydimensions>($$)->dims.push_back(
    dcast<ast_exp>($3));
 }
;

/* VarDecl ::= BType VarDef {"," VarDef} ";" */
VarDecl
: Types VarDefList OP_SEMICOLON {
  if(dcast<ast_term_generic>($1)->type == K_VOID) {
    yyerror(nullptr, "Definition cannot be void");
  }
  else {
    $$ = std::move($2);
  }
 }
;

VarDefList
: VarDef {
  auto r = make_shared<ast_decl>();
  r->defs.push_back(dcast<ast_def>($1));
  $$ = r;
 }
| VarDefList OP_COMMA VarDef {
  $$ = std::move($1);
  dcast<ast_decl>($$)->defs.push_back(dcast<ast_def>($3));
 }
;

/* VarDef ::= IDENT {"[" ConstExp "]"} ["=" InitVal] */
VarDef
: IDENT DefArrayDimensions InitValOptional {
  $$ = make_shared<ast_def>(
    std::move(dcast<ast_term_ident>($1)->name),
    std::move(dcast<ast_defarraydimensions>($2)->dims),
    dcast<ast_initval>($3));
 }
;

InitValOptional
: {
  $$ = nullptr;
}
| OP_ASSIGN InitVal {
  $$ = std::move($2);
 }
;

/* InitVal ::= Exp | "{" [InitVal {"," InitVal}] "}"; */
InitVal
: Exp {
  auto r = make_shared<ast_initval>();
  r->content.emplace<std::shared_ptr<ast_exp>>(
    dcast<ast_exp>($1));
  $$ = r;
 }
| OP_LBRACE InitValList OP_RBRACE {
  auto r = make_shared<ast_initval>();
  r->content.emplace<std::vector<std::shared_ptr<ast_initval>>>(
    std::move(dcast<ast_initvallist>($2)->list));
  $$ = r;
 }
| OP_LBRACE OP_RBRACE {
  auto r = make_shared<ast_initval>();
  r->content.emplace<std::vector<std::shared_ptr<ast_initval>>>();
  $$ = r;
 }
;

InitValList
: InitVal {
  auto r = make_shared<ast_initvallist>();
  r->list.push_back(dcast<ast_initval>($1));
  $$ = r;
 }
| InitValList OP_COMMA InitVal {
  $$ = std::move($1);
  dcast<ast_initvallist>($$)->list.push_back(
    dcast<ast_initval>($3));
 }
;

/* FuncDef ::= FuncType IDENT "(" [FuncFParams] ")" Block; */
FuncDef
: Types IDENT OP_LPAREN FuncFParamsOptional OP_RPAREN Block {
  $$ = make_shared<ast_funcdef>(
    dcast<ast_term_generic>($1)->type,
    std::move(dcast<ast_term_ident>($2)->name),
    std::move(dcast<ast_funcfparams>($4)->params),
    dcast<ast_block>($6));
 }
;

FuncFParamsOptional
: {
  $$ = make_shared<ast_funcfparams>();
 }
| FuncFParams {
  $$ = std::move($1);
 }
;

/* FuncFParams ::= FuncFParam {"," FuncFParam}; */
FuncFParams
: FuncFParam {
  auto r = make_shared<ast_funcfparams>();
  r->params.push_back(dcast<ast_funcfparam>($1));
  $$ = r;
 }
| FuncFParams OP_COMMA FuncFParam {
  $$ = std::move($1);
  dcast<ast_funcfparams>($$)->params.push_back(
    dcast<ast_funcfparam>($3));
 }
;

/* FuncFParam ::= BType IDENT ["[" "]" {"[" ConstExp "]"}]; */
FuncFParam
: Types IDENT {
  if(dcast<ast_term_generic>($1)->type == K_VOID) {
    yyerror(nullptr, "Function parameters cannot be void");
  }
  else {
    $$ = make_shared<ast_funcfparam>(std::move(dcast<ast_term_ident>($2)->name));
  }
 }
| Types IDENT OP_LBRACKET OP_RBRACKET DefArrayDimensions {
  if(dcast<ast_term_generic>($1)->type == K_VOID) {
    yyerror(nullptr, "Function parameters cannot be void");
  }
  else {
    auto r = make_shared<ast_funcfparam>(std::move(dcast<ast_term_ident>($2)->name));
    r->dims.emplace_back();
    append_move(r->dims, dcast<ast_defarraydimensions>($5)->dims);
    $$ = r;
  }
 }
;

/* Block ::= "{" {BlockItem} "}"; */
/* BlockItem ::= Decl | Stmt; */
Block
: OP_LBRACE BlockItems OP_RBRACE {
  $$ = std::move($2);
 }
;

BlockItems
: {
  $$ = make_shared<ast_block>();
 }
| BlockItems ConstDecl {
  auto r = dcast<ast_block>($1);
  for(std::shared_ptr<ast_constdef> &def: dcast<ast_constdecl>($2)->constdefs) {
    r->items.emplace_back(std::move(def));
  }
  $$ = r;
 }
| BlockItems VarDecl {
  auto r = dcast<ast_block>($1);
  for(std::shared_ptr<ast_def> &def: dcast<ast_decl>($2)->defs) {
    r->items.emplace_back(std::move(def));
  }
  $$ = r;
 }
| BlockItems AnyStmt {
  $$ = std::move($1);
  if($2) {
    dcast<ast_block>($$)->items.emplace_back(dcast<ast_stmt>($2));
  }
 }
;

/* Stmt ::= LVal "=" Exp ";" */
/*        | [Exp] ";" */
/*        | Block */
/*        | "if" "(" Cond ")" Stmt ["else" Stmt] */
/*        | "while" "(" Cond ")" Stmt */
/*        | "break" ";" */
/*        | "continue" ";" */
/*        | "return" [Exp] ";"; */
AnyStmt
: Stmt {
  $$ = std::move($1);
 }
| OpenStmt {
  $$ = std::move($1);
 }
;

OpenStmt
: K_IF OP_LPAREN Exp OP_RPAREN AnyStmt {
  $$ = make_shared<ast_stmt_if>(
    dcast<ast_exp>($3),
    dcast<ast_stmt>($5),
    nullptr);
 }
| K_IF OP_LPAREN Exp OP_RPAREN Stmt K_ELSE OpenStmt {
  $$ = make_shared<ast_stmt_if>(
    dcast<ast_exp>($3),
    dcast<ast_stmt>($5),
    dcast<ast_stmt>($7));
 }
;

Stmt
: OP_SEMICOLON {
  $$ = nullptr;
 }
| LVal OP_ASSIGN Exp OP_SEMICOLON {
  $$ = make_shared<ast_stmt_assign>(
    dcast<ast_lval>($1),
    dcast<ast_exp>($3));
 }
| Exp OP_SEMICOLON {
  $$ = make_shared<ast_stmt_eval>(
    dcast<ast_exp>($1));
 }
| Block {
  $$ = make_shared<ast_stmt_subblock>(
    dcast<ast_block>($1));
 }
| K_IF OP_LPAREN Exp OP_RPAREN Stmt K_ELSE Stmt {
  $$ = make_shared<ast_stmt_if>(
    dcast<ast_exp>($3),
    dcast<ast_stmt>($5),
    dcast<ast_stmt>($7));
 }
| K_WHILE OP_LPAREN Exp OP_RPAREN Stmt {
  $$ = make_shared<ast_stmt_while>(
    dcast<ast_exp>($3),
    dcast<ast_stmt>($5));
 }
| K_BREAK OP_SEMICOLON {
  $$ = make_shared<ast_stmt_break>();
 }
| K_CONTINUE OP_SEMICOLON {
  $$ = make_shared<ast_stmt_continue>();
 }
| K_RETURN OP_SEMICOLON {
  $$ = make_shared<ast_stmt_return>(nullptr);
 }
| K_RETURN Exp OP_SEMICOLON {
  $$ = make_shared<ast_stmt_return>(
    dcast<ast_exp>($2));
 }
;

/* ConstExp ::= AddExp; */
/* Caveat: This can only be checked in semantic analysis. */

/* Exp definitions. I extended it so we can now mix logic and arithmetic calculations */
Exp
: LOrExp {
  $$ = std::move($1);
 }
;

LOrExp
: LAndExp {
  $$ = std::move($1);
 }
| LOrExp OP_LOR LAndExp {
  concat_op2($$, $1, $3, OP_LOR);
 }
;

LAndExp
: EqExp {
  $$ = std::move($1);
 }
| LAndExp OP_LAND EqExp {
  concat_op2($$, $1, $3, OP_LAND);
 }
;

EqExp
: RelExp {
  $$ = std::move($1);
 }
| EqExp OP_EQ RelExp {
  concat_op2($$, $1, $3, OP_EQ);
 }
| EqExp OP_NEQ RelExp {
  concat_op2($$, $1, $3, OP_NEQ);
 }
;

RelExp
: AddExp {
  $$ = std::move($1);
 }
| RelExp OP_LT AddExp {
  concat_op2($$, $1, $3, OP_LT);
 }
| RelExp OP_GT AddExp {
  concat_op2($$, $1, $3, OP_GT);
 }
| RelExp OP_LE AddExp {
  concat_op2($$, $1, $3, OP_LE);
 }
| RelExp OP_GE AddExp {
  concat_op2($$, $1, $3, OP_GE);
 }
;

AddExp
: MulExp {
  $$ = std::move($1);
 }
| AddExp OP_ADD MulExp {
  concat_op2($$, $1, $3, OP_ADD);
 }
| AddExp OP_SUB MulExp {
  concat_op2($$, $1, $3, OP_SUB);
 }
;

MulExp
: UnaryExp {
  $$ = std::move($1);
 }
| MulExp OP_MUL UnaryExp {
  concat_op2($$, $1, $3, OP_MUL);
 }
| MulExp OP_DIV UnaryExp {
  concat_op2($$, $1, $3, OP_DIV);
 }
| MulExp OP_REM UnaryExp {
  concat_op2($$, $1, $3, OP_REM);
 }
;

UnaryExp
: TerminalExp {
  $$ = std::move($1);
 }
| OP_ADD UnaryExp {
  concat_op1($$, $2, OP_ADD);
 }
| OP_SUB UnaryExp {
  concat_op1($$, $2, OP_SUB);
 }
| OP_NEG UnaryExp {
  concat_op1($$, $2, OP_NEG);
 }
| OP_LPAREN Exp OP_RPAREN {
  $$ = std::move($2);
 }
;

FuncRParamsOptional
: {
  $$ = make_shared<ast_funcrparams>();
 }
| FuncRParams {
  $$ = std::move($1);
 }
;

FuncRParams
: Exp {
  auto r = make_shared<ast_funcrparams>();
  r->params.push_back(dcast<ast_exp>($1));
  $$ = r;
 }
| FuncRParams OP_COMMA Exp {
  $$ = std::move($1);
  dcast<ast_funcrparams>($$)->params.push_back(dcast<ast_exp>($3));
 }
;

TerminalExp
: IDENT OP_LPAREN FuncRParamsOptional OP_RPAREN {
  $$ = make_shared<ast_funccall>(
    std::move(dcast<ast_term_ident>($1)->name),
    std::move(dcast<ast_funcrparams>($3)->params));
 }
| LVal {
  $$ = std::move($1);
 }
| INT_LITERAL {
  $$ = make_shared<ast_int_literal>(
    dcast<ast_term_int>($1)->val);
 }
;

LVal
: IDENT RefArrayDimensions {
  $$ = make_shared<ast_lval>(
    std::move(dcast<ast_term_ident>($1)->name),
    std::move(dcast<ast_refarraydimensions>($2)->dims));
 }
;

RefArrayDimensions
: {
  $$ = make_shared<ast_refarraydimensions>();
 }
| RefArrayDimensions OP_LBRACKET Exp OP_RBRACKET {
  $$ = std::move($1);
  dcast<ast_refarraydimensions>($$)->dims.push_back(dcast<ast_exp>($3));
 }
;

%%
