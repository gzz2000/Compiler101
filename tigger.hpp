#pragma once

#include <variant>
#include <vector>
#include <string>
#include <optional>

const static char *reglist[28] = {
  // 0
  "x0",
  // 1
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
  // 13
  "t0", "t1", "t2", "t3", "t4", "t5", "t6",
  // 20
  "a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7"
  // 28
};

struct tg_reg {
  int id;    // [0, 28)
};

typedef std::variant<int, tg_reg> tg_rval;

struct tg_expr_op {
  int op, numop;
  tg_reg lval;
  tg_reg a;
  tg_rval b;
};

struct tg_expr_assign_c {
  tg_reg lval;
  tg_rval a;
};

struct tg_expr_assign_la {
  tg_reg lreg;
  int lidx;
  tg_reg a;
};

struct tg_expr_assign_ra {
  tg_reg lval;
  tg_reg areg;
  int aidx;
};

struct tg_expr_cond_goto {
  tg_reg a, b;
  int lop;
  int label_id;
};

struct tg_expr_goto {
  int label_id;
};

struct tg_expr_label {
  int label_id;
};

struct tg_expr_call {
  std::string func;
};

struct tg_expr_ret {};

struct tg_expr_stack_store {
  tg_reg val;
  int pos;
};

struct tg_expr_stack_load {
  int pos;
  tg_reg lval;
};

struct tg_expr_stack_loadaddr {
  int pos;
  tg_reg addr;
};

struct tg_expr_global_load {
  int vid;
  tg_reg lval;
};

struct tg_expr_global_loadaddr {
  int vid;
  tg_reg addr;
};

typedef std::variant<
  tg_expr_op,
  tg_expr_assign_c,
  tg_expr_assign_la,
  tg_expr_assign_ra,
  tg_expr_cond_goto,
  tg_expr_goto,
  tg_expr_label,
  tg_expr_call,
  tg_expr_ret,
  tg_expr_stack_store,
  tg_expr_stack_load,
  tg_expr_stack_loadaddr,
  tg_expr_global_load,
  tg_expr_global_loadaddr
  > tg_expr_types;

struct tg_funcdef {
  std::string name;
  int num_params, size_stack = 0;
  std::vector<tg_expr_types> exprs;
};

struct tg_global_decl {
  int vid;
  std::optional<int> sz;
};

struct tg_program {
  std::vector<tg_global_decl> decls;
  std::vector<tg_funcdef> funcdefs;
};
