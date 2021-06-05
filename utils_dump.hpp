#pragma once

#include "sysy.tab.hpp"

inline const char *opname2str(int op) {
  switch(op) {
  case OP_ADD: return "+";
  case OP_SUB: return "-";
  case OP_MUL: return "*";
  case OP_DIV: return "/";
  case OP_REM: return "%";
  case OP_NEG: return "!";
  case OP_LOR: return "||";
  case OP_LAND: return "&&";
  case OP_EQ: return "==";
  case OP_NEQ: return "!=";
  case OP_LT: return "<";
  case OP_GT: return ">";
  case OP_LE: return "<=";
  case OP_GE: return ">=";
  default: return "???";
  }
}
