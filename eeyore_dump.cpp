#include "sysy.hpp"
#include "sysy.tab.hpp"
#include "utils.hpp"
#include "eeyore.hpp"
#include "utils_dump.hpp"
#include <variant>
#include <iostream>

using std::endl;

#define DEFOUT(def) \
  inline std::ostream &operator << (std::ostream &out, def)

DEFOUT(const ee_symbol &sym) {
  out << sym.type << sym.id;
  return out;
}

DEFOUT(const ee_decl &decl) {
  if(decl.size) out << "  var " << 4 * *decl.size << " " << decl.sym << endl;
  else out << "  var " << decl.sym << endl;
  return out;
}

DEFOUT(const std::vector<ee_decl> &decls) {
  for(const ee_decl &decl: decls) {
    out << decl;
  }
  return out;
}

DEFOUT(const ee_rval &rv) {
  std::visit(overloaded{
      [&] (int v) { out << v; },
      [&] (ee_symbol sym) { out << sym; }
    }, rv);
  return out;
}

DEFOUT(const ee_lval &lv) {
  out << lv.sym;
  if(lv.sym_idx) out << "[" << *lv.sym_idx << "]";
  return out;
}

DEFOUT(const ee_expr_op &op) {
  if(op.numop == 1) {
    if(op.op == OP_ADD) out << "  " << op.sym << " = " << op.b << endl;
    else out << "  " << op.sym << " = " << opname2str(op.op) << op.a << endl;
  }
  else {
    out << "  " << op.sym << " = " << op.a << " " << opname2str(op.op) << " " << op.b << endl;
  }
  return out;
}

DEFOUT(const ee_expr_assign &a) {
  out << "  " << a.lval << " = " << a.a << endl;
  return out;
}

DEFOUT(const ee_expr_assign_arr &a) {
  out << "  " << a.sym << " = " << a.a << endl;
  return out;
}

DEFOUT(const ee_expr_cond_goto &cgo) {
  out << "  if " << cgo.a << " " << opname2str(cgo.lop) << " " << cgo.b << " goto l" << cgo.label_id << endl;
  return out;
}

DEFOUT(const ee_expr_goto &go) {
  out << "  goto l" << go.label_id << endl;
  return out;
}

DEFOUT(const ee_expr_label &lbl) {
  out << "l" << lbl.label_id << ":" << endl;
  return out;
}

DEFOUT(const ee_expr_call &call) {
  if(call.func == "starttime" || call.func == "stoptime") return out;
  for(const ee_rval &rv: call.params) {
    out << "  param " << rv << endl;
  }
  if(call.store) out << "  " << *call.store << " = call f_";
  else out << "  call f_";
  out << call.func << endl;
  return out;
}

DEFOUT(const ee_expr_ret &ret) {
  if(ret.val) out << "  return " << *ret.val << endl;
  else out << "  return" << endl;
  return out;
}

DEFOUT(const ee_funcdef &fdef) {
  out << "f_" << fdef.name << " [" << fdef.num_params << "]" << endl;
  out << fdef.decls;
  out << endl;
  for(const ee_expr_types &expr: fdef.exprs) {
    std::visit(
      [&] (const auto &expr_t) {
        out << expr_t;
      }, expr);
  }
  out << "end f_" << fdef.name << endl;
  return out;
}

DEFOUT(const ee_program &eeprog) {
  out << eeprog.decls;
  for(const ee_funcdef &fdef: eeprog.funcdefs) {
    out << endl;
    out << fdef;
  }
  return out;
}

void dump_eeyore(std::shared_ptr<ee_program> eeprog, std::ostream &out) {
  out << *eeprog;
}
