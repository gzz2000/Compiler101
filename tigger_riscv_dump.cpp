#include "sysy.hpp"
#include "sysy.tab.hpp"
#include "utils.hpp"
#include "eeyore.hpp"
#include "tigger.hpp"
#include "utils_dump.hpp"
#include <variant>
#include <iostream>

using std::endl;

namespace tigger_riscv_dump {

__attribute__((noreturn))
void rverror_print(const char *str, int lineno) {
  printf("RISC-V generation error: %s\n", str);
  exit(lineno % 256);
}

#define rverror(str) rverror_print(str, __LINE__)

#define DEFOUT(tigger_type) \
  inline static std::ostream &operator << (std::ostream &out, const tigger_type &t)

DEFOUT(tg_reg) {
  return out << reglist[t.id];
}

inline static tg_reg tmpreg(tg_reg occupy) {
  return occupy.id == 13 ? tg_reg{14} : tg_reg{13};
}

DEFOUT(tg_expr_op) {
  if(t.numop == 1) {
    switch(t.op) {
    case OP_SUB:
      out << "  neg " << t.lval << ", " << t.a << endl;
      break;
    case OP_NEG:
      out << "  seqz " << t.lval << ", " << t.a << endl;
      break;
    default:
      rverror("1-ary op");
    }
  }
  else {
    const auto popreg = [&] (tg_reg b) {
      switch(t.op) {
      case OP_ADD:
        out << "  add " << t.lval << ", " << t.a << ", " << b << endl;
        break;
      case OP_SUB:
        out << "  sub " << t.lval << ", " << t.a << ", " << b << endl;
        break;
      case OP_MUL:
        out << "  mul " << t.lval << ", " << t.a << ", " << b << endl;
        break;
      case OP_DIV:
        out << "  div " << t.lval << ", " << t.a << ", " << b << endl;
        break;
      case OP_REM:
        out << "  rem " << t.lval << ", " << t.a << ", " << b << endl;
        break;
      case OP_LT:
        out << "  slt " << t.lval << ", " << t.a << ", " << b << endl;
        break;
      case OP_GT:
        out << "  sgt " << t.lval << ", " << t.a << ", " << b << endl;
        break;
      case OP_LE:
        out << "  sgt " << t.lval << ", " << t.a << ", " << b << endl;
        out << "  seqz " << t.lval << ", " << t.lval << endl;
        break;
      case OP_GE:
        out << "  slt " << t.lval << ", " << t.a << ", " << b << endl;
        out << "  seqz " << t.lval << ", " << t.lval << endl;
        break;
      case OP_LAND:
        out << "  snez " << t.lval << ", " << t.a << endl;
        out << "  snez " << tmpreg(t.lval) << ", " << b << endl;
        out << "  and " << t.lval << ", " << t.lval << ", " << tmpreg(t.lval) << endl;
        break;
      case OP_LOR:
        out << "  or " << t.lval << ", " << t.a << ", " << b << endl;
        out << "  snez " << t.lval << ", " << t.lval << endl;
        break;
      case OP_NEQ:
        out << "  xor " << t.lval << ", " << t.a << ", " << b << endl;
        out << "  snez " << t.lval << ", " << t.lval << endl;
        break;
      case OP_EQ:
        out << "  xor " << t.lval << ", " << t.a << ", " << b << endl;
        out << "  seqz " << t.lval << ", " << t.lval << endl;
        break;
      default:
        rverror("2-ary op");
      }
    };
    std::visit(overloaded{
        popreg,
        [&] (int b) {
          if((t.op == OP_ADD || t.op == OP_LT) &&
             (-2048 <= b && b < 2048)) {
            switch(t.op) {
            case OP_ADD:
              out << "  addi " << t.lval << ", " << t.a << ", " << b << endl;
              break;
            case OP_LT:
              out << "  slti " << t.lval << ", " << t.a << ", " << b << endl;
              break;
            default:
              rverror("impossible");
            }
          }
          else if(b > 1 && !(b & (b - 1)) &&
                  (t.op == OP_MUL || t.op == OP_DIV || t.op == OP_REM)) {
            int p = 0;
            while((1 << p) != b) ++p;
            switch(t.op) {
            case OP_MUL:
              out << "  slli " << t.lval << ", " << t.a << ", " << p << endl;
              break;
            case OP_DIV:
              out << "  srai " << tmpreg(t.a) << ", " << t.a << ", 31" << endl;
              out << "  andi " << tmpreg(t.a) << ", " << tmpreg(t.a) << ", " << (b - 1) << endl;
              out << "  add " << t.lval << ", " << tmpreg(t.a) << ", " << t.a << endl;
              out << "  srai " << t.lval << ", " << t.lval << ", " << p << endl;
              break;
            case OP_REM:
              out << "  srai " << tmpreg(t.a) << ", " << t.a << ", 31" << endl;
              out << "  srli " << tmpreg(t.a) << ", " << tmpreg(t.a) << ", " << (32 - p) << endl;
              out << "  add " << tmpreg(tmpreg(t.a)) << ", " << t.a << ", " << tmpreg(t.a) << endl;
              out << "  andi " << tmpreg(tmpreg(t.a)) << ", " << tmpreg(tmpreg(t.a)) << ", " << (b - 1) << endl;
              out << "  sub " << t.lval << ", " << tmpreg(tmpreg(t.a)) << ", " << tmpreg(t.a) << endl;
              break;
            default:
              rverror("impossible");
            }
          }
          else {
            out << "  li " << tmpreg(t.a) << ", " << b << endl;
            popreg(tmpreg(t.a));
          }
        }
      }, t.b);
  }
  return out;
}

DEFOUT(tg_expr_assign_c) {
  std::visit(overloaded{
      [&] (tg_reg a) {
        out << "  mv " << t.lval << ", " << a << endl;
      },
      [&] (int a) {
        out << "  li " << t.lval << ", " << a << endl;
      }
    }, t.a);
  return out;
}

DEFOUT(tg_expr_assign_la) {
  if(t.lidx >= -2048 && t.lidx < 2048) {
    out << "  sw " << t.a << ", " << t.lidx << "(" << t.lreg << ")" << endl;
    return out;
  }
  tg_reg c = tmpreg(t.lreg);
  if(c.id == t.a.id) c = tmpreg(t.a);
  if(c.id == t.lreg.id) {
    // need to borrow space from stack..
    out << "  sw " << t.a << ", -4(sp)" << endl;
    out << "  li " << t.a << ", " << t.lidx << endl;
    out << "  add " << t.lreg << ", " << t.lreg << ", " << t.a << endl;
    out << "  lw " << t.a << ", -4(sp)" << endl;
    out << "  sw " << t.a << ", 0(" << t.lreg << ")" << endl;
    return out;
  }
  out << "  li " << c << ", " << t.lidx << endl;
  out << "  add " << c << ", " << c << ", " << t.lreg << endl;
  out << "  sw " << t.a << ", 0(" << c << ")" << endl;
  return out;
}

DEFOUT(tg_expr_assign_ra) {
  if(t.aidx >= -2048 && t.aidx < 2048) {
    out << "  lw " << t.lval << ", " << t.aidx << "(" << t.areg << ")" << endl;
    return out;
  }
  tg_reg c = tmpreg(t.areg);
  out << "  li " << c << ", " << t.aidx << endl;
  out << "  add " << c << ", " << c << ", " << t.areg << endl;
  out << "  lw " << t.lval << ", 0(" << c << ")" << endl;
  return out;
}

DEFOUT(tg_expr_cond_goto) {
  const char *inst = NULL;
  switch(t.lop) {
  case OP_LT:
    inst = "blt";
    break;
  case OP_GT:
    inst = "bgt";
    break;
  case OP_LE:
    inst = "ble";
    break;
  case OP_GE:
    inst = "bge";
    break;
  case OP_NEQ:
    inst = "bne";
    break;
  case OP_EQ:
    inst = "beq";
    break;
  default:
    rverror("impossible");
  }
  return out << "  " << inst << " " << t.a << ", " << t.b << ", .l" << t.label_id << endl;
}

DEFOUT(tg_expr_goto) {
  return out << "  j .l" << t.label_id << endl;
}

DEFOUT(tg_expr_label) {
  return out << ".l" << t.label_id << ":" << endl;
}

DEFOUT(tg_expr_call) {
  if(t.func == "starttime" || t.func == "stoptime") {
    return out << "  call _sysy_" << t.func << endl;
  }
  return out << "  call " << t.func << endl;
}

DEFOUT(tg_expr_stack_store) {
  if(t.pos >= -512 && t.pos < 512) {
    out << "  sw " << t.val << ", " << (t.pos * 4) << "(sp)" << endl;
  }
  else {
    out << "  li " << tmpreg(t.val) << ", " << (t.pos * 4) << endl
        << "  add " << tmpreg(t.val) << ", " << tmpreg(t.val) << ", sp" << endl
        << "  sw " << t.val << ", 0(" << tmpreg(t.val) << ")" << endl;
  }
  return out;
}

DEFOUT(tg_expr_stack_load) {
  if(t.pos >= -512 && t.pos < 512) {
    out << "  lw " << t.lval << ", " << (t.pos * 4) << "(sp)" << endl;
  }
  else {
    out << "  li " << t.lval << ", " << (t.pos * 4) << endl
        << "  add " << t.lval << ", " << t.lval << ", sp" << endl
        << "  sw " << t.lval << ", 0(" << t.lval << ")" << endl;
  }
  return out;
}

DEFOUT(tg_expr_stack_loadaddr) {
  if(t.pos >= -512 && t.pos < 512) {
    out << "  addi " << t.addr << ", sp, " << (t.pos * 4) << endl;
  }
  else {
    out << "  li " << t.addr << ", " << (t.pos * 4) << endl
        << "  add " << t.addr << ", sp, " << t.addr << endl;
  }
  return out;
}

DEFOUT(tg_expr_global_load) {
  out << "  lui " << t.lval << ", %hi(v" << t.vid << ")" << endl
      << "  lw " << t.lval << ", %lo(v" << t.vid << ")(" << t.lval << ")" << endl;
  return out;
}

DEFOUT(tg_expr_global_loadaddr) {
  out << "  la " << t.addr << ", v" << t.vid << endl;
  return out;
}

DEFOUT(tg_funcdef) {
  int STK = (t.size_stack / 4 + 1) * 16;
  out << "  .text" << endl
      << "  .align 2" << endl
      << "  .global " << t.name << endl
      << "  .type " << t.name << ", @function" << endl
      << t.name << ":" << endl;
  if(STK >= -2048 && STK < 2048) {
    out << "  sw ra, -4(sp)" << endl
        << "  addi sp, sp, -" << STK << endl;
  }
  else {
    out << "  sw ra, -4(sp)" << endl
        << "  li t0, " << -STK << endl
        << "  add sp, sp, t0" << endl;
  }
  for(const auto &expr: t.exprs) {
    std::visit(overloaded{
        [&] (tg_expr_ret) {
          if(STK >= -2048 && STK < 2048) {
            out << "  addi sp, sp, " << STK << endl
                << "  lw ra, -4(sp)" << endl
                << "  ret" << endl;
          }
          else {
            out << "  li t0, " << STK << endl
                << "  add sp, sp, t0" << endl
                << "  lw ra, -4(sp)" << endl
                << "  ret" << endl;
          }
        },
        [&] (const auto &t) {
          out << t;
        }
      }, expr);
  }
  out << "  .size " << t.name << ", .-" << t.name << endl;
  out << endl;
  return out;
}

DEFOUT(tg_global_decl) {
  if(!t.sz) {
    out << "  .global v" << t.vid << endl
        << "  .section .sdata" << endl
        << "  .align 2" << endl
        << "  .type v" << t.vid << ", @object" << endl
        << "  .size v" << t.vid << ", 4" << endl
        << "v" << t.vid << ":" << endl
        << "  .word 0" << endl;
  }
  else {
    out << "  .comm v" << t.vid << ", " << (4 * *t.sz) << ", 4" << endl;
  }
  return out;
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

}

void dump_riscv(std::shared_ptr<tg_program> tgprog, std::ostream &out) {
  using namespace tigger_riscv_dump;
  out << *tgprog;
}
