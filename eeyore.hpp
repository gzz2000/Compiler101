#pragma once

#include <string>
#include <optional>
#include <variant>
#include <vector>
#include "utils.hpp"

struct ee_base {
  // std::string comment;
  // virtual ~ee_base() {}
};

struct ee_symbol {
  char type;
  int id;
};

inline bool operator == (ee_symbol a, ee_symbol b) {
  return a.type == b.type && a.id == b.id;
}

inline bool operator != (ee_symbol a, ee_symbol b) {
  return a.type != b.type || a.id != b.id;
}

namespace std {
template <>
struct hash<ee_symbol> {
  inline std::size_t operator () (ee_symbol sym) const {
    return std::size_t(sym.id) * 256 + sym.type;
  }
};
}

struct ee_decl: ee_base {
  ee_symbol sym;
  std::optional<int> size;
};

struct ee_expr: ee_base {};

typedef std::variant<int, ee_symbol> ee_rval;

struct ee_expr_op: ee_expr {
  ee_symbol sym;
  ee_rval a, b;
  int op, numop;
};

struct ee_lval {
  ee_symbol sym;
  std::optional<ee_rval> sym_idx;
};

inline bool operator == (ee_lval a, ee_lval b) {
  return a.sym == b.sym && a.sym_idx == b.sym_idx;
}

namespace std {
template <>
struct hash<ee_lval> {
  hash<std::tuple<ee_symbol, std::optional<ee_rval>>> h0;
  inline std::size_t operator () (ee_lval lv) const {
    return h0(std::make_tuple(lv.sym, lv.sym_idx));
  }
};
}

struct ee_expr_assign: ee_expr {
  ee_lval lval;
  ee_rval a;
};

struct ee_expr_assign_arr: ee_expr {
  ee_symbol sym;
  ee_lval a;       // eeyore requires a.sym_idx to be valid.
};

struct ee_expr_cond_goto: ee_expr {
  ee_rval a, b;
  int lop;   // logic op, {EQ, NEQ, GT, LT, GE, LE}
  int label_id;
};

struct ee_expr_goto: ee_expr {
  int label_id;
  inline ee_expr_goto(int _label_id): label_id(_label_id) {}
};

struct ee_expr_label: ee_expr {
  int label_id;
  inline ee_expr_label(int _label_id): label_id(_label_id) {}
};

struct ee_expr_call: ee_expr {
  std::optional<ee_symbol> store;
  std::vector<ee_rval> params;
  std::string func;
};

struct ee_expr_ret: ee_expr {
  std::optional<ee_rval> val;
};

typedef std::variant<
  ee_expr_op,
  ee_expr_assign,
  ee_expr_assign_arr,
  ee_expr_cond_goto,
  ee_expr_goto,
  ee_expr_label,
  ee_expr_call,
  ee_expr_ret> ee_expr_types;

struct ee_funcdef: ee_base {
  std::string name;
  int num_params;
  std::vector<ee_decl> decls;
  std::vector<ee_expr_types> exprs;
};

struct ee_program: ee_base {
  std::vector<ee_decl> decls;
  std::vector<ee_funcdef> funcdefs;
};
