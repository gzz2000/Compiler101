#pragma once

#include <vector>
#include <string>
#include <memory>
#include <utility>
#include <variant>
#include <iterator>

template<typename vec_t1, typename vec_t2>
inline void append_move(vec_t1 &a, vec_t2 &b) {
  a.reserve(a.size() + b.size());
  a.insert(a.end(), std::make_move_iterator(b.begin()), std::make_move_iterator(b.end()));
}

template<typename vec_t>
inline void append_copy(vec_t &a, const vec_t &b) {
  a.reserve(a.size() + b.size());
  a.insert(a.end(), b.begin(), b.end());
}

// implementation from https://en.cppreference.com/w/cpp/memory/shared_ptr/pointer_cast, copied here for C++17
template< class T, class U > 
std::shared_ptr<T> dcast( const std::shared_ptr<U>& r ) noexcept {
  if (auto p = dynamic_cast<typename std::shared_ptr<T>::element_type*>(r.get())) {
    return std::shared_ptr<T>(r, p);
  } else {
    return std::shared_ptr<T>();
  }
}

struct ast_nodebase {
  virtual ~ast_nodebase() {}
};

// typedef std::shared_ptr<ast_nodebase> YYSTYPE;

struct ast_term_int: ast_nodebase {   // intermediate only
  int val;
  inline ast_term_int(int _val): val(_val) {}
};

struct ast_term_ident: ast_nodebase { // intermediate only
  std::string name;
  inline ast_term_ident(std::string &&_name): name(std::move(_name)) {}
};

struct ast_term_generic: ast_nodebase { // intermediate only
  int type;
  inline ast_term_generic(int _type): type(_type) {}
};

struct ast_compunit;
struct ast_constdef;
struct ast_def;
struct ast_funcdef;
struct ast_exp;
struct ast_initval;
struct ast_funcfparam;
struct ast_block;
struct ast_blockitem;
struct ast_stmt;
struct ast_lval;

struct ast_compunit: ast_nodebase {
  std::vector<std::shared_ptr<ast_def>> defs;
  std::vector<std::shared_ptr<ast_funcdef>> funcdefs;
};

struct ast_blockitem: ast_nodebase {};

// type is always int
struct ast_constdecl: ast_nodebase {  // intermediate only
  std::vector<std::shared_ptr<ast_constdef>> constdefs;
};

struct ast_decl: ast_nodebase {  // intermediate only
  std::vector<std::shared_ptr<ast_def>> defs;
};

struct ast_defarraydimensions: ast_nodebase {  // intermediate only
  std::vector<std::shared_ptr<ast_exp>> dims;
};

struct ast_def: ast_blockitem {
  std::string name;
  std::vector<std::shared_ptr<ast_exp>> dims;
  std::shared_ptr<ast_initval> init;  // optional
  
  inline ast_def(std::string &&_name, std::vector<std::shared_ptr<ast_exp>> &&_dims, std::shared_ptr<ast_initval> &&_init): name(std::move(_name)), dims(std::move(_dims)), init(std::move(_init)) {}
};

struct ast_constdef: ast_def {
  inline ast_constdef(std::string &&_name, std::vector<std::shared_ptr<ast_exp>> &&_dims, std::shared_ptr<ast_initval> &&_init): ast_def(std::move(_name), std::move(_dims), std::move(_init)) {}
};

struct ast_initval: ast_nodebase {
  std::variant<std::shared_ptr<ast_exp>,
               std::vector<std::shared_ptr<ast_initval>>> content;
};

struct ast_initvallist: ast_nodebase {  // intermediate only
  std::vector<std::shared_ptr<ast_initval>> list;
};

struct ast_funcdef: ast_nodebase {
  int type;
  std::string name;
  std::vector<std::shared_ptr<ast_funcfparam>> params;
  std::shared_ptr<ast_block> block;

  inline ast_funcdef(int _type, std::string &&_name, std::vector<std::shared_ptr<ast_funcfparam>> &&_params, std::shared_ptr<ast_block> &&_block): type(_type), name(std::move(_name)), params(std::move(_params)), block(std::move(_block)) {}
};

struct ast_funcfparams: ast_nodebase {  // intermediate only
  std::vector<std::shared_ptr<ast_funcfparam>> params;
};

// type is always int
struct ast_funcfparam: ast_nodebase {
  std::string name;
  std::vector<std::shared_ptr<ast_exp>> dims; // if present, the first element is defined to be nullptr.
  
  // inline ast_funcfparam(std::string &&_name, std::vector<std::shared_ptr<ast_exp>> &&_dims): name(std::move(_name)), dims(std::move(_dims)) {}
};

struct ast_block: ast_nodebase {
  std::vector<std::shared_ptr<ast_blockitem>> items;
};

struct ast_stmt: ast_blockitem {};

struct ast_stmt_assign: ast_stmt {
  std::shared_ptr<ast_lval> l;
  std::shared_ptr<ast_exp> r;

  inline ast_stmt_assign(std::shared_ptr<ast_lval> _l, std::shared_ptr<ast_exp> _r): l(_l), r(_r) {}
};

struct ast_stmt_eval: ast_stmt {
  std::shared_ptr<ast_exp> v;
  inline ast_stmt_eval(std::shared_ptr<ast_exp> _v): v(_v) {}
};

struct ast_stmt_subblock: ast_stmt {
  std::shared_ptr<ast_block> b;
  inline ast_stmt_subblock(std::shared_ptr<ast_block> _b): b(_b) {}
};

struct ast_stmt_if: ast_stmt {
  std::shared_ptr<ast_exp> cond;
  std::shared_ptr<ast_stmt> exec;
  std::shared_ptr<ast_stmt> exec_else;

  inline ast_stmt_if(std::shared_ptr<ast_exp> _cond, std::shared_ptr<ast_stmt> _exec, std::shared_ptr<ast_stmt> _exec_else): cond(_cond), exec(_exec), exec_else(_exec_else) {}
};

struct ast_stmt_while: ast_stmt {
  std::shared_ptr<ast_exp> cond;
  std::shared_ptr<ast_stmt> exec;

  inline ast_stmt_while(std::shared_ptr<ast_exp> _cond, std::shared_ptr<ast_stmt> _exec): cond(_cond), exec(_exec) {}
};

struct ast_break: ast_stmt {};

struct ast_continue: ast_stmt {};

struct ast_return: ast_stmt {
  std::shared_ptr<ast_exp> val;   // nullptr if no return value.
  inline ast_return(std::shared_ptr<ast_exp> _val): val(_val) {}
};

struct ast_unaryop: ast_nodebase {
  int type;   // OP_ADD, OP_SUB, OP_NEG
  std::shared_ptr<ast_unaryop> op;
};

struct ast_exp_term: ast_nodebase {};

struct ast_lval: ast_exp_term {
  std::string name;
  std::vector<std::shared_ptr<ast_exp>> dims;

  inline ast_lval(std::string &&_name, std::vector<std::shared_ptr<ast_exp>> &&_dims): name(std::move(_name)), dims(std::move(_dims)) {}
};

struct ast_refarraydimensions: ast_nodebase {  // intermediate only
  std::vector<std::shared_ptr<ast_exp>> dims;
};

struct ast_int_literal: ast_exp_term {
  int val;
  inline ast_int_literal(int _val): val(_val) {}
};

struct ast_funccall: ast_exp_term {
  std::string name;
  std::vector<std::shared_ptr<ast_exp>> params;

  inline ast_funccall(std::string &&_name, std::vector<std::shared_ptr<ast_exp>> &&_params): name(std::move(_name)), params(std::move(_params)) {}
};

struct ast_funcrparams: ast_nodebase {  // intermediate only
  std::vector<std::shared_ptr<ast_exp>> params;
};

struct ast_exp: ast_nodebase {
  // reverse polish notation
  std::vector<std::variant<
                std::shared_ptr<ast_exp_term>,  // expression terminal
                std::pair<int, int /* #operands */> // operator
                >> rpn;
};
