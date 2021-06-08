#include "sysy.hpp"
#include "sysy.tab.hpp"
#include "utils.hpp"
#include "eeyore.hpp"
#include "tigger.hpp"
#include "utils_dump.hpp"
#include <variant>
#include <iostream>

using std::endl;

#define DEFOUT(tigger_type) \
  inline static std::ostream &operator << (std::ostream &out, const tigger_type &t)

DEFOUT(tg_reg) {
  return out << reglist[t.id];
}

DEFOUT(tg_rval) {
  std::visit([&] (const auto &v) {
    out << v;
  }, t);
  return out;
}

DEFOUT(tg_expr_op) {
  if(t.numop == 1) return out << t.lval << " = " << opname2str(t.op) << " " << t.a << endl;
  else return out << t.lval << " = " << t.a << " " << opname2str(t.op) << " " << t.b << endl;
}

DEFOUT(tg_expr_assign_c) {
  return out << t.lval << " = " << t.a << endl;
}

DEFOUT(tg_expr_assign_la) {
  return out << t.lreg << "[" << t.lidx << "] = " << t.a << endl;
}

DEFOUT(tg_expr_assign_ra) {
  return out << t.lval << " = " << t.areg << "[" << t.aidx << "]" << endl;
}

DEFOUT(tg_expr_cond_goto) {
  return out << "if " << t.a << " " << opname2str(t.lop) << " " << t.b << " goto l" << t.label_id << endl;
}

DEFOUT(tg_expr_goto) {
  return out << "goto l" << t.label_id << endl;
}

DEFOUT(tg_expr_label) {
  return out << "l" << t.label_id << ":" << endl;
}

DEFOUT(tg_expr_call) {
  return out << "call f_" << t.func << endl;
}

DEFOUT(tg_expr_ret) {
  (void)t;
  return out << "return" << endl;
}

DEFOUT(tg_expr_stack_store) {
  return out << "store " << t.val << " " << t.pos << endl;
}

DEFOUT(tg_expr_stack_load) {
  return out << "load " << t.pos << " " << t.lval << endl;
}

DEFOUT(tg_expr_stack_loadaddr) {
  return out << "loadaddr " << t.pos << " " << t.addr << endl;
}

DEFOUT(tg_expr_global_load) {
  return out << "load v" << t.vid << " " << t.lval << endl;
}

DEFOUT(tg_expr_global_loadaddr) {
  return out << "loadaddr v" << t.vid << " " << t.addr << endl;
}

DEFOUT(tg_funcdef) {
  out << "f_" << t.name << " [" << t.num_params << "] [" << t.size_stack << "]" << endl;
  for(const auto &expr: t.exprs) {
    std::visit(overloaded{
        [&] (const tg_expr_label &lbl) {
          out << lbl;
        },
        [&] (const auto &t) {
          out << "  " << t;
        }
      }, expr);
  }
  out << "end f_" << t.name << endl;
  return out;
}

DEFOUT(tg_global_decl) {
  if(t.sz) return out << "v" << t.vid << " = malloc " << (4 * *t.sz) << endl;
  else return out << "v" << t.vid << " = 0" << endl;
}

DEFOUT(tg_program) {
  for(const auto &decl: t.decls) out << decl;
  out << endl;
  for(const auto &f: t.funcdefs) {
    out << f;
    out << endl;
  }
  return out;
}

void dump_tigger(std::shared_ptr<tg_program> tgprog, std::ostream &out) {
  out << *tgprog;
}
