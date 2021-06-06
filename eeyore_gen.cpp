#include "sysy.hpp"
#include "sysy.tab.hpp"
#include "eeyore.hpp"
#include <vector>
#include <unordered_map>
#include <stack>
#include <variant>
#include <iostream>

__attribute__((noreturn))
void egerror_print(const char *str, int lineno) {
  printf("Eeyore generation error: %s\n", str);
  exit(lineno % 256);
}

#define egerror(str) egerror_print(str, __LINE__)

static int global_label_id = 0;

struct g_def {
  std::vector<int> dims;
  ee_symbol sym;                         // for both const and non-const.
  std::optional<std::vector<int>> vals;  // for const only.
};

template<typename T>
struct layered_store {
  std::unordered_map<std::string, T> m;
  layered_store<T> *const last;

  inline layered_store(layered_store<T> *_last): last(_last) {}
  
  inline T *query(const std::string &key) {
    if(auto it = m.find(key); it != m.end()) {
      return &it->second;
    }
    else if(last) {
      return last->query(key);
    }
    else return nullptr;
  }
  
  inline const T *query(const std::string &key) const {
    if(auto it = m.find(key); it != m.end()) {
      return &it->second;
    }
    else if(last) {
      return last->query(key);
    }
    else return nullptr;
  }
};

struct decl_symbol_manager {
  std::vector<ee_decl> &out_decls;
  int cnt_T = 0, cnt_t = 0, cnt_p = 0;
  
  inline decl_symbol_manager(std::vector<ee_decl> &_out_decls): out_decls(_out_decls) {}
  inline decl_symbol_manager(std::vector<ee_decl> &_out_decls, const decl_symbol_manager &cnts): out_decls(_out_decls), cnt_T(cnts.cnt_T), cnt_t(cnts.cnt_t), cnt_p(cnts.cnt_p) {}

  template<char c>
  inline int &get_cnt() {
    if constexpr (c == 'T') return cnt_T;
    else if constexpr (c == 't') return cnt_t;
    else if constexpr (c == 'p') return cnt_p;
    else static_assert(always_false_v<c>, "Invalid char!");
  }

  template<char c>
  inline ee_symbol next() {
    ee_decl decl;
    decl.sym.type = c;
    decl.sym.id = get_cnt<c>()++;
    if constexpr (c != 'p') out_decls.push_back(decl);
    return decl.sym;
  }
  
  template<char c>
  inline ee_symbol next(int size) {
    if constexpr (c == 'p') {
      static_assert(always_false_v<c>, "Function parameter should not be provided size.");
    }
    ee_decl decl;
    decl.sym.type = c;
    decl.sym.id = get_cnt<c>()++;
    decl.size.emplace(size);
    out_decls.push_back(decl);
    return decl.sym;
  }
};

// eval_exp is the general expression evaluator.
// if const, then it will eventually yield a number.
// if not, it is compiled to a temp variable.

ee_rval eval_exp(const ast_exp &exp,
                 const layered_store<g_def> &defs,
                 std::vector<ee_expr_types> &out_assigns,
                 decl_symbol_manager &out_decls);

typedef std::variant<
  ee_lval,     // evaluates to a reference
  ee_symbol,   // evaluates to a pointer
  int          // evaluates to a constant
  > ev_lval_ret;

ev_lval_ret eval_lval(const ast_lval &lval,
                      const layered_store<g_def> &defs,
                      std::vector<ee_expr_types> &out_assigns,
                      decl_symbol_manager &out_decls) {
  const g_def *def = defs.query(lval.name);
  if(!def) {
    std::cerr << "Symbol \"" << lval.name
              << "\" not found in current context." << std::endl;
    egerror("Undefined symbol");
  }
  if(lval.dims.size() > def->dims.size()) {
    egerror("Too many dimensions in array reference.");
  }

  ee_rval index = 0;
  if(lval.dims.size()) {
    std::vector<ee_rval> subindices(lval.dims.size());
    int sz = 4;
    for(int j = (int)def->dims.size() - 1; j >= (int)lval.dims.size(); --j) {
      sz *= def->dims[j];
    }
    for(int j = (int)lval.dims.size() - 1; j >= 0; --j) {
      ast_exp_op op;
      op.op = OP_MUL;
      op.numop = 2;
      op.a = std::make_shared<ast_int_literal>(sz);
      op.b = lval.dims[j];
      subindices[j] = eval_exp(op, defs, out_assigns, out_decls);
      // reorder so that the constants are added first.
      std::visit(overloaded{
          [&] (int v) {
            index = std::get<int>(index) + v;
          },
          [&] (ee_symbol) {}
        }, subindices[j]);
      sz *= def->dims[j];
    }
    
    // reorder so that the first dimensions are added first.
    // this is for raising the computation outside.
    for(int j = 0; j < (int)lval.dims.size(); ++j) {
      std::visit(overloaded{
          [&] (int) {},
          [&] (ee_symbol sym) {
            if(int *pvi = std::get_if<int>(&index); pvi && !*pvi) {
              index = subindices[j];
            }
            else {
              // construct add
              ee_symbol r = out_decls.next<'t'>();
              ee_expr_op op;
              op.sym = r;
              op.a = sym;
              op.b = index;
              op.op = OP_ADD;
              op.numop = 2;
              out_assigns.emplace_back(op);
              index = r;
            }
          }
        }, subindices[j]);
    }
  }
  if(lval.dims.size() < def->dims.size()) {
    // this expression generates a pointer.
    if(int *pvi = std::get_if<int>(&index); pvi && !*pvi) return def->sym;
    // add
    ee_symbol r = out_decls.next<'t'>();
    ee_expr_op op;
    op.sym = r;
    op.a = def->sym;
    op.b = index;
    op.op = OP_ADD;
    op.numop = 2;
    out_assigns.emplace_back(op);
    return r;
  }
  else {
    // this expression generates an int reference.
    if(def->vals && std::get_if<int>(&index)) {
      // constant. congrats.
      int idx = std::get<int>(index) / 4;
      if(idx >= (int)def->vals->size()) {
        egerror("Too large index when accessing a const array.");
      }
      return def->vals->operator[](idx);
    }
    else {
      // we need a runtime access.
      ee_lval ret;
      ret.sym = def->sym;
      if(def->dims.size()) ret.sym_idx = index;
      return ret;
    }
  }
}

ee_rval eval_exp(const ast_exp &exp,
                 const layered_store<g_def> &defs,
                 std::vector<ee_expr_types> &out_assigns,
                 decl_symbol_manager &out_decls) {
  if(dynamic_cast<const ast_exp_term *>(&exp)) {
    if(auto t_int = dynamic_cast<const ast_int_literal *>(&exp); t_int) {
      return t_int->val;
    }
    else if(auto t_fcall = dynamic_cast<const ast_funccall *>(&exp); t_fcall) {
      // a function call
      ee_expr_call call;
      call.func = t_fcall->name;
      call.params.reserve(t_fcall->params.size());
      for(auto expp: t_fcall->params) {
        call.params.push_back(
          eval_exp(*expp, defs, out_assigns, out_decls));
      }
      ee_symbol sym = out_decls.next<'t'>();
      call.store.emplace(sym);
      out_assigns.push_back(call);
      return sym;
    }
    else if(auto t_lval = dynamic_cast<const ast_lval *>(&exp); t_lval) {
      ev_lval_ret lvret = eval_lval(*t_lval, defs, out_assigns, out_decls);
      ee_rval ret;
      std::visit(overloaded{
          [&] (ee_symbol sym) {
            // a pointer.
            ret = sym;
          },
          [&] (ee_lval lv) {
            // a runtime int reference.
            if(lv.sym_idx) {
              // we need one instruction to extract the array value.
              ee_symbol r = out_decls.next<'t'>();
              ee_expr_assign_arr op;
              op.sym = r;
              op.a = lv;
              out_assigns.emplace_back(op);
              ret = r;
            }
            else ret = lv.sym;
          },
          [&] (int v) {
            // a constant
            ret = v;
          }
        }, lvret);
      return ret;
    }
    else {
      egerror("Unhandled expterm: a bug within the compiler.");
    }
  }
  else if(auto t_op = dynamic_cast<const ast_exp_op *>(&exp); t_op) {
    if(t_op->op == OP_LOR || t_op->op == OP_LAND) {
      ee_rval a = eval_exp(*t_op->a, defs, out_assigns, out_decls);
      ee_rval ret;
      std::visit(overloaded{
          [&] (int v) {
            // the first part evaluated to constant.
            // we shall check if it determines the whole exp.
            if((t_op->op == OP_LOR && v == 1) || (t_op->op == OP_LAND && v == 0)) {
              ret = !!v;
            }
            else {
              ret = eval_exp(*t_op->b, defs, out_assigns, out_decls);
            }
          },
          [&] (ee_symbol sym) {
            // the first part is up to runtime input.
            // structure:  [t1 = a; ifAND(t1 == 0) exit; t2 = b; t1 = t2; exit:;]
            ret = sym;
            
            // generate a conditional jump.
            ee_expr_cond_goto cgo;
            cgo.a = sym;
            cgo.b = 0;
            cgo.lop = (t_op->op == OP_LAND ? OP_EQ : OP_NEQ);
            cgo.label_id = ++global_label_id;
            out_assigns.emplace_back(cgo);

            // generate second evaluation
            ee_rval b = eval_exp(*t_op->b, defs, out_assigns, out_decls);
            // generate back assignment
            ee_expr_assign ea;
            ea.lval.sym = sym;
            ea.a = b;
            out_assigns.emplace_back(ea);

            // generate the end label
            out_assigns.push_back(ee_expr_label(cgo.label_id));
          }
        }, a);
      return ret;
    }
    else {   // a normal operator
      ee_rval a = eval_exp(*t_op->a, defs, out_assigns, out_decls);
      ee_rval b(0);
      if(t_op->numop == 2) b = eval_exp(*t_op->b, defs, out_assigns, out_decls);
      if(std::get_if<int>(&a) && std::get_if<int>(&b)) {
        // compile time evaluation
        int na = std::get<int>(a), nb = std::get<int>(b), ans;
        if(t_op->numop == 2) {
#define P(name, op)                             \
          case OP_##name:                       \
            ans = (na op nb);                   \
            break;
          switch(t_op->op) {
            // P(LOR, ||);
            // P(LAND, &&);
            P(EQ, ==);
            P(NEQ, !=);
            P(LT, <);
            P(GT, >);
            P(LE, <=);
            P(GE, >=);
            P(ADD, +);
            P(SUB, -);
            P(MUL, *);
            P(DIV, /);
            P(REM, %);
          default:
            egerror("unexpected 2-op when doing compile-time computation.");
          }
#undef P
        }
        else {
#define P(name, op)                             \
          case OP_##name:                       \
            ans = (op na);                      \
            break;
          switch(t_op->op) {
            P(ADD, +);
            P(SUB, -);
            P(NEG, !);
          default:
            egerror("unexpected 1-op when doing compile-time computation.");
          }
#undef P
        }
        return ans;
      }
      else {
        // runtime op
        ee_symbol r = out_decls.next<'t'>();
        ee_expr_op op;
        op.op = t_op->op;
        op.numop = t_op->numop;
        op.a = a;
        op.b = b;
        op.sym = r;
        out_assigns.emplace_back(op);
        return r;
      }
    }
  }
  else {
    egerror("Unhandled exptype: a bug within the compiler.");
  }
}

void eeyore_cond_goto(const ast_exp &cond, bool inv, 
                      decl_symbol_manager &declman,
                      layered_store<g_def> &defs,
                      std::vector<ee_expr_types> &exprs,
                      int lbl, int lbl_fl = -1) {
  // the adjacent two ops are inverse of each other.
  // the first two must be eq, neq.
  const static int lops[6] = {OP_EQ, OP_NEQ, OP_LT, OP_GE, OP_LE, OP_GT};

  if(auto t_int = dynamic_cast<const ast_int_literal *>(&cond); t_int) {
    if(inv ^ !!t_int->val) {
      // uncond goto
      exprs.push_back(ee_expr_goto(lbl));
      return;
    }
    else return;
  }
  if(auto t_op = dynamic_cast<const ast_exp_op *>(&cond); t_op) {
    // logic 2-op.
    if(t_op->op == OP_LOR || t_op->op == OP_LAND) {
      bool gen_lbl_fl = (lbl_fl == -1);
      if(gen_lbl_fl) lbl_fl = ++global_label_id;
      if(inv ^ (t_op->op == OP_LOR)) {   // de morgan's law here
        eeyore_cond_goto(*t_op->a, inv, declman, defs, exprs, lbl, lbl_fl);
        eeyore_cond_goto(*t_op->b, inv, declman, defs, exprs, lbl, lbl_fl);
      }
      else {
        eeyore_cond_goto(*t_op->a, !inv, declman, defs, exprs, lbl_fl);
        eeyore_cond_goto(*t_op->b, inv, declman, defs, exprs, lbl, lbl_fl);
      }
      if(gen_lbl_fl) exprs.push_back(ee_expr_label(lbl_fl));
      return;
    }

    // logic 1-op.
    if(t_op->numop == 1 && t_op->op == OP_NEG) {
      eeyore_cond_goto(*t_op->a, !inv, declman, defs, exprs, lbl, lbl_fl);
      return;
    }

    // relation op
    int t = -1;
    for(int i = 0; i < 6; ++i) if(t_op->op == lops[i]) t = i;
    if(t != -1) {
      ee_rval a = eval_exp(*t_op->a, defs, exprs, declman);
      ee_rval b = eval_exp(*t_op->b, defs, exprs, declman);
      if(std::get_if<int>(&a) && std::get_if<int>(&b)) {
        ee_rval v = eval_exp(cond, defs, exprs, declman);
        if(inv ^ !!std::get<int>(v)) {
          // uncond goto
          exprs.push_back(ee_expr_goto(lbl));
          return;
        }
        else return;
      }
      
      ee_expr_cond_goto cgo;
      cgo.label_id = lbl;
      cgo.a = a;
      cgo.b = b;
      cgo.lop = lops[t ^ inv];
      cgo.label_id = lbl;
      exprs.push_back(cgo);
      return;
    }
  }
  
  ee_rval v = eval_exp(cond, defs, exprs, declman);
  if(std::get_if<int>(&v)) {
    if(inv ^ !!std::get<int>(v)) {
      // uncond goto
      exprs.push_back(ee_expr_goto(lbl));
      return;
    }
    else return;
  }
  ee_expr_cond_goto cgo;
  cgo.a = v;
  cgo.b = 0;
  cgo.lop = lops[inv ^ 1];
  cgo.label_id = lbl;
  exprs.push_back(cgo);
}

inline void process_initlist(int *dims, int sz_dims,
                             const ast_initval &init,
                             std::shared_ptr<ast_exp> *store) {
  std::visit(overloaded{
      [&] (std::shared_ptr<ast_exp> exp) {
        if(sz_dims) {
          egerror("Initializing an array with a single number.");
        }
        store[0] = exp;
      },
      [&] (const std::vector<std::shared_ptr<ast_initval>> &v) {
        if(!sz_dims) {
          // if(v.size() > 1) egerror("Initializing a single number with an array.");
          // caveat: did not check multiple braces here.
          if(!v.empty()) process_initlist(dims, sz_dims, *v[0], store);
          return;
        }
        std::vector<int> i_dec(sz_dims, 0);
        int i = 0;
        for(auto pv: v) {
          if(i_dec[0] >= dims[0]) {
            egerror("Too many init values.");
          }
          std::visit(overloaded{
              [&] (std::shared_ptr<ast_exp> subexp) {
                store[i] = subexp;
                ++i;
                ++i_dec[sz_dims - 1];
                for(int j = sz_dims - 1; j >= 1; --j) {
                  if(i_dec[j] >= dims[j]) {
                    i_dec[j] -= dims[j];
                    ++i_dec[j - 1];
                  }
                  else break;
                }
              },
              [&] (std::vector<std::shared_ptr<ast_initval>> &) {
                // locate the last complete subarray.
                if(i_dec[sz_dims - 1]) {
                  egerror("Initializing a single number with a subarray.");
                }
                int t = sz_dims - 1;
                while(t - 1 > 0 && !i_dec[t - 1]) --t;
                process_initlist(dims + t, sz_dims - t, *pv, store + i);
                
                ++i_dec[t - 1];
                int prod = 1;
                for(int j = t; j < sz_dims; ++j) prod *= dims[j];
                i += prod;
                for(int j = t - 1; j >= 1; --i) {
                  if(i_dec[j] >= dims[j]) {
                    i_dec[j] -= dims[j];
                    ++i_dec[j - 1];
                  }
                  else break;
                }
              }
            },
            pv->content);
        }
      }
    },
    init.content);
}

template<char def_type_c, bool global>
inline void push_def(layered_store<g_def> &defs,
                     std::shared_ptr<ast_def> cdef,
                     std::vector<ee_expr_types> &out_assigns,
                     decl_symbol_manager &out_decls) {
  if(defs.m.find(cdef->name) != defs.m.end()) {
    egerror("Redeclared symbol in current context.");
  }
  g_def &d = defs.m[cdef->name];
  d.dims.resize(cdef->dims.size());
  int size = 1;
  for(int i = (def_type_c == 'p' ? 1 : 0); i < (int)cdef->dims.size(); ++i) {
    ee_rval r = eval_exp(*cdef->dims[i], defs, out_assigns, out_decls);
    std::visit(overloaded{
        [&] (int a) {
          d.dims[i] = a;
          size *= a;
        },
        [&] (ee_symbol) {
          egerror("Non-constant expression used to define array dims.");
        }
      }, r);
  }
  if constexpr (def_type_c == 'p') {
    d.sym = out_decls.next<def_type_c>();
  }
  else {
    d.sym = cdef->dims.size() ? out_decls.next<def_type_c>(size) : out_decls.next<def_type_c>();
    if(cdef->init) {
      std::vector<std::shared_ptr<ast_exp>> store(size);
      process_initlist(d.dims.data(), d.dims.size(), *cdef->init, store.data());
      auto constdef = dcast<ast_constdef>(cdef);
      if(constdef) d.vals.emplace(size);
      const auto genzeroseq = [&] (int l, int r) {
        // generate a small loop to fill zeros
        // caveat: not SSA
        ee_symbol i = out_decls.next<'t'>();
        int lbl_st = ++global_label_id;
        ee_expr_assign ea_i0;
        ea_i0.lval.sym = i;
        ea_i0.a = l * 4;
        out_assigns.push_back(ea_i0);
        out_assigns.push_back(ee_expr_label(lbl_st));
        ee_expr_assign ea_zero;
        ea_zero.lval.sym = d.sym;
        ea_zero.lval.sym_idx = i;
        ea_zero.a = 0;
        out_assigns.push_back(ea_zero);
        ee_expr_op ea_ipp;
        ea_ipp.sym = i;
        ea_ipp.a = i;
        ea_ipp.b = 4;
        ea_ipp.op = OP_ADD;
        ea_ipp.numop = 2;
        out_assigns.push_back(ea_ipp);
        ee_expr_cond_goto ea_back;
        ea_back.a = i;
        ea_back.b = r * 4;
        ea_back.lop = OP_LT;
        ea_back.label_id = lbl_st;
        out_assigns.push_back(ea_back);
      };
      for(int i = 0; i < size; ++i) {
        ee_rval r;
        if(store[i]) r = eval_exp(*store[i], defs, out_assigns, out_decls);
        else {
          int j = i + 1;
          while(j < size && !store[j]) ++j;
          if(j - i > 3) {
            if constexpr (!global) genzeroseq(i, j);
            i = j - 1;
            continue;
          }
          r = 0;
        }
        if(constdef) {
          std::visit(overloaded{
              [&] (int v) {
                d.vals->operator[](i) = v;
              },
              [&] (ee_symbol) {
                egerror("Non-constant expression used to initialize const definition.");
              }
            }, r);
        }
        ee_expr_assign ea;
        ea.lval.sym = d.sym;
        if(d.dims.size()) ea.lval.sym_idx.emplace(i * 4);
        ea.a = r;
        out_assigns.emplace_back(ea);
      }
    }
    else if(dcast<ast_constdef>(cdef)) {
      egerror("Const declaration without init");
    }
  }
}

void eeyore_gen_block(ast_block &block,
                      decl_symbol_manager &declman,
                      layered_store<g_def> &last_defs,
                      std::vector<ee_expr_types> &exprs,
                      int lbl_loop_st, int lbl_loop_ed);

void eeyore_gen_stmt(std::shared_ptr<ast_stmt> stmt,
                     decl_symbol_manager &declman,
                     layered_store<g_def> &defs,
                     std::vector<ee_expr_types> &exprs,
                     int lbl_loop_st, int lbl_loop_ed) {
  if(auto it = dcast<ast_stmt_assign>(stmt); it) {
    ev_lval_ret lvret = eval_lval(*it->l, defs, exprs, declman);
    ee_rval rval = eval_exp(*it->r, defs, exprs, declman);
    ee_lval lv;
    std::visit(overloaded{
        [&] (int) {
          egerror("Cannot assign value to constant symbols.");
        },
        [&] (ee_symbol) {
          egerror("Cannot assign value to a partial array pointer.");
        },
        [&] (ee_lval _lv) {
          lv = _lv;
        }
      }, lvret);
    ee_expr_assign op;
    op.lval = lv;
    op.a = rval;
    exprs.push_back(op);
  }
  else if(auto it = dcast<ast_stmt_eval>(stmt); it) {
    eval_exp(*it->v, defs, exprs, declman);
    if(exprs.size()) {
      auto it = std::get_if<ee_expr_call>(&*exprs.rbegin());
      if(it) {
        it->store.reset();
        // caveat: unused var.
      }
    }
  }
  else if(auto it = dcast<ast_stmt_subblock>(stmt); it) {
    eeyore_gen_block(*it->b, declman, defs, exprs,
                     lbl_loop_st, lbl_loop_ed);
  }
  else if(auto it = dcast<ast_stmt_if>(stmt); it) {
    int lbl_jo_then = ++global_label_id;
    int lbl_jo_else = ++global_label_id;
    eeyore_cond_goto(*it->cond, true, declman, defs, exprs, lbl_jo_then);
    eeyore_gen_stmt(it->exec, declman, defs, exprs,
                    lbl_loop_st, lbl_loop_ed);
    if(it->exec_else) exprs.push_back(ee_expr_goto(lbl_jo_else));
    exprs.push_back(ee_expr_label(lbl_jo_then));
    if(it->exec_else) {
      eeyore_gen_stmt(it->exec_else, declman, defs, exprs,
                      lbl_loop_st, lbl_loop_ed);
      exprs.push_back(ee_expr_label(lbl_jo_else));
    }
  }
  else if(auto it = dcast<ast_stmt_while>(stmt); it) {
    int lbl_st = ++global_label_id, lbl_ed = ++global_label_id;
    exprs.push_back(ee_expr_label(lbl_st));
    eeyore_cond_goto(*it->cond, true, declman, defs, exprs, lbl_ed);
    eeyore_gen_stmt(it->exec, declman, defs, exprs,
                    lbl_st, lbl_ed);
    exprs.push_back(ee_expr_goto(lbl_st));
    exprs.push_back(ee_expr_label(lbl_ed));
  }
  else if(auto it = dcast<ast_stmt_break>(stmt); it) {
    if(lbl_loop_ed < 0) {
      egerror("break outside a loop.");
    }
    exprs.push_back(ee_expr_goto(lbl_loop_ed));
  }
  else if(auto it = dcast<ast_stmt_continue>(stmt); it) {
    if(lbl_loop_st < 0) {
      egerror("continue outside a loop.");
    }
    exprs.push_back(ee_expr_goto(lbl_loop_st));
  }
  else if(auto it = dcast<ast_stmt_return>(stmt); it) {
    ee_expr_ret ret;
    if(it->val) ret.val = eval_exp(*it->val, defs, exprs, declman);
    exprs.push_back(ret);
  }
  else {
    egerror("Unexpected block statement. "
            "This is a bug within the compiler.");
  }
}

void eeyore_gen_block(ast_block &block,
                      decl_symbol_manager &declman,
                      layered_store<g_def> &last_defs,
                      std::vector<ee_expr_types> &exprs,
                      int lbl_loop_st, int lbl_loop_ed) {
  layered_store<g_def> defs(&last_defs);
  for(auto bi: block.items) {
    if(auto it = dcast<ast_def>(bi); it) {
      push_def<'T', false>(defs, it, exprs, declman);
    }
    else {
      eeyore_gen_stmt(dcast<ast_stmt>(bi), declman, defs, exprs,
                      lbl_loop_st, lbl_loop_ed);
    }
  }
}

std::shared_ptr<ee_program> eeyore_gen(std::shared_ptr<ast_compunit> sysy) {
  auto ret = std::make_shared<ee_program>();

  layered_store<g_def> defs(nullptr);
  decl_symbol_manager declman(ret->decls);
  std::vector<ee_expr_types> t_definits;
  for(auto def: sysy->defs) {
    push_def<'T', true>(defs, def, t_definits, declman);
  }
  
  ret->funcdefs.reserve(sysy->funcdefs.size());
  for(auto sysy_fdef: sysy->funcdefs) {
    auto &ee_f = ret->funcdefs.emplace_back();
    ee_f.name = sysy_fdef->name;
    ee_f.num_params = sysy_fdef->params.size();
    
    decl_symbol_manager func_declman(ee_f.decls, declman);
    layered_store<g_def> defs_with_params(&defs);
    
    for(int i = 0; i < ee_f.num_params; ++i) {
      push_def<'p', false>(defs_with_params, sysy_fdef->params[i], ee_f.exprs, func_declman);
    }
    if(sysy_fdef->name == "main") {
      ee_f.exprs = std::move(t_definits);
      // move all temp declarations to main function, instead of
      // keeping them globally.
      std::vector<ee_decl> old_global_decls = std::move(ret->decls);
      for(auto &ed: old_global_decls) {
        if(ed.sym.type == 't') ee_f.decls.push_back(ed);
        else ret->decls.push_back(ed);
      }
    }
    eeyore_gen_block(*sysy_fdef->block, func_declman, defs_with_params, ee_f.exprs,
                     -1, -1);
    if(sysy_fdef->type == K_VOID) {
      ee_f.exprs.push_back(ee_expr_ret());
    }
  }
  return ret;
}
