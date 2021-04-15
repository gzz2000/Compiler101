#include "sysy.hpp"
#include "sysy.tab.h"
#include "eeyore.hpp"
#include <vector>
#include <unordered_map>
#include <stack>
#include <variant>
#include <iostream>

// overload op constructor: from https://en.cppreference.com/w/cpp/utility/variant/visit
// helper constant for the visitor #3
template<char> inline constexpr bool always_false_v = false;
// helper type for the visitor #4
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

void egerror(const char *str) {
  printf("Eeyore generation error: %s\n", str);
  exit(3);
}

int global_label_id = 0;

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
// if not, the reverse polish notation is compiled along the way.

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

  std::optional<ee_rval> index;
  if(lval.dims.size()) {
    ast_exp pos_calc;
    for(int j = 0; j < (int)lval.dims.size(); ++j) {
      append_copy(pos_calc.rpn, lval.dims[j]->rpn);
      if(j > 0) {
        pos_calc.rpn.emplace_back(std::make_pair(OP_ADD, 2));
      }
      if(j < (int)lval.dims.size() - 1) {
        pos_calc.rpn.emplace_back(std::make_shared<ast_int_literal>(def->dims[j + 1]));
        pos_calc.rpn.emplace_back(std::make_pair(OP_MUL, 2));
      }
    }
    int mulrem = 4;  // sizeof(int)
    for(int j = lval.dims.size(); j < (int)def->dims.size(); ++j) {
      mulrem *= def->dims[j];
    }
    pos_calc.rpn.emplace_back(std::make_shared<ast_int_literal>(mulrem));
    pos_calc.rpn.emplace_back(std::make_pair(OP_MUL, 2));
    
    index = eval_exp(pos_calc, defs, out_assigns, out_decls);
  }
  
  if(lval.dims.size() < def->dims.size()) {
    // this expression generates a pointer.
    if(!index) return def->sym;
    // add
    ee_symbol r = out_decls.next<'t'>();
    ee_expr_op op;
    op.sym = r;
    op.a = def->sym;
    op.b = *index;
    op.op = OP_ADD;
    op.numop = 2;
    out_assigns.emplace_back(op);
    return r;
  }
  else {
    // this expression generates an int reference.
    if(def->vals && (!index || std::get_if<int>(&index.value()))) {
      // constant. congrats.
      int idx = (index ? std::get<int>(*index) : 0);
      if(idx > (int)def->vals->size()) {
        egerror("Too large index when accessing a const array.");
      }
      return def->vals->operator[](idx);
    }
    else {
      // we need a runtime access.
      ee_lval ret;
      ret.sym = def->sym;
      ret.sym_idx = index;
      return ret;
    }
  }
}

ee_rval eval_exp(const ast_exp &exp,
                 const layered_store<g_def> &defs,
                 std::vector<ee_expr_types> &out_assigns,
                 decl_symbol_manager &out_decls) {
  std::stack<ee_rval> s;
  std::stack<std::optional<int /* label id */>> sc_labels;
  
  for(int i = 0; i < (int)exp.rpn.size(); ++i) {
    std::visit(overloaded{
        [&] (std::shared_ptr<ast_exp_term> term) {
          if(auto t = dcast<ast_int_literal>(term); t) {
            s.emplace(t->val);
          }
          else if(auto t = dcast<ast_funccall>(term); t) {
            // push a function call
            ee_expr_call call;
            call.func = t->name;
            call.params.reserve(t->params.size());
            for(auto expp: t->params) {
              call.params.push_back(
                eval_exp(*expp, defs, out_assigns, out_decls));
            }
            ee_symbol sym = out_decls.next<'t'>();
            call.store.emplace(sym);
            out_assigns.push_back(call);
            s.push(sym);
          }
          else if(auto t = dcast<ast_lval>(term); t) {
            ev_lval_ret lvret = eval_lval(*t, defs, out_assigns, out_decls);
            std::visit(overloaded{
                [&] (ee_symbol sym) {
                  // a pointer.
                  s.push(sym);
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
                    s.push(r);
                  }
                  else s.push(lv.sym);
                },
                [&] (int v) {
                  // a constant
                  s.push(v);
                }
              }, lvret);
          }
          else {
            egerror("Unhandled expterm: a bug within the compiler.");
          }
        },
        
        [&] (std::pair<int, int> ops) {
          if(ops.first == OP_LAND || ops.first == OP_LOR) {
            // special care: short circuit evaluation.
            if(ops.second == 1) {
              // middle tag
              ee_rval a = s.top();
              std::visit(overloaded{
                  [&] (int v) {
                    // the first part evaluated to constant.
                    // we shall check if it determines the whole exp.
                    if((ops.first == OP_LAND && !v) ||
                       (ops.first == OP_LOR && v)) {
                      s.pop();
                      s.push(ee_rval(!!v));
                      // jump over.
                      int c = 1;
                      while(c) {
                        std::visit(overloaded{
                            [&] (std::shared_ptr<ast_exp_term>) {
                              ++c;
                            },
                            [&] (std::pair<int, int> ops) {
                              c = c - ops.second + 1;
                            }
                          },
                          exp.rpn[++i]);
                      }
                    }
                    else {
                      // fallback to the second op when we meet it.
                      s.push(ee_rval(v));
                      sc_labels.emplace();
                    }
                  },
                  [&] (ee_symbol sym) {
                    // the first part is up to runtime input.
                    // generate a conditional jump.
                    ee_expr_cond_goto cgo;
                    cgo.a = sym;
                    cgo.b = 0;
                    cgo.lop = (ops.first == OP_LAND ? OP_EQ : OP_NEQ);
                    cgo.label_id = ++global_label_id;
                    out_assigns.emplace_back(cgo);
                    sc_labels.emplace(cgo.label_id);
                  }
                },
                a);
            }
            else {
              // end tag
              ee_rval a, b;
              b = s.top(); s.pop();
              a = s.top(); s.pop();
              std::optional<int> label_id = sc_labels.top();
              sc_labels.pop();
              // set the value as b.
              auto put_assign = [&] (ee_symbol sym) {
                // put the assign.
                ee_expr_assign ea;
                ea.lval.sym = sym;
                ea.a = b;
                out_assigns.emplace_back(ea);
                // put the label.
                if(label_id) {
                  out_assigns.push_back(ee_expr_label(*label_id));
                }
                s.push(sym);
              };
              std::visit(overloaded{
                  [&] (int) {   // constant, simplified
                    if(auto it = std::get_if<int>(&b); it) {
                      s.push(*it); // constant result
                    }
                    else {
                      put_assign(out_decls.next<'t'>());
                    }
                  },
                  [&] (ee_symbol real_sym) {
                    put_assign(real_sym);
                  }
                },
                a);
            }
            return;
          }
          
          ee_rval a(0), b(0);
          b = s.top(); s.pop();
          if(ops.second == 2) { a = s.top(); s.pop(); }
          if(std::get_if<int>(&a) && std::get_if<int>(&b)) {
            // constant. evaluate the result at compile time.
            int na = std::get<int>(a), nb = std::get<int>(b), ans;
            if(ops.second == 1) {
#define P(name, op)                             \
              case OP_##name:                   \
                ans = (na op nb);               \
                break;
              switch(ops.first) {
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
              case OP_##name:                   \
                ans = (op nb);                  \
                break;
              switch(ops.first) {
                P(ADD, +);
                P(SUB, -);
                P(NEG, !);
              default:
                egerror("unexpected 1-op when doing compile-time computation.");
              }
#undef P
            }
            s.push(ee_rval(ans));
          }
          else {
            // build an op to be executed at runtime.
            ee_symbol r = out_decls.next<'t'>();
            ee_expr_op op;
            op.sym = r;
            op.a = a;
            op.b = b;
            op.op = ops.first;
            op.numop = ops.second;
            s.push(ee_rval(r));
          }
        }
      },
      exp.rpn[i]);
  }
  if(s.size() != 1) {
    egerror("Unexpected stack size after RPN reduction. "
            "This is a bug within the compiler.");
  }
  return s.top();
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
          egerror("Initializing a single number with an array.");
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
                for(int j = sz_dims - 1; j >= 1; --i) {
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

template<char def_type_c>
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
      for(int i = 0; i < size; ++i) {
        ee_rval r;
        if(store[i]) r = eval_exp(*store[i], defs, out_assigns, out_decls);
        else r = 0;
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

// the adjacent two ops are inverse of each other.
// the first two must be eq, neq.
const static int lops[6] = {OP_EQ, OP_NEQ, OP_LT, OP_GE, OP_LE, OP_GT};

void eeyore_gen_stmt(std::shared_ptr<ast_stmt> stmt,
                     decl_symbol_manager &declman,
                     layered_store<g_def> &defs,
                     std::vector<ee_expr_types> &exprs,
                     int lbl_loop_st, int lbl_loop_ed) {
  auto make_cond_goto = [&] (ee_rval cond, bool inv, int lbl) {
    std::visit(overloaded{
        [&] (int c) {
          // degenerated to unconditional
          if((!!c) ^ inv) exprs.push_back(ee_expr_goto(lbl));
        },
        [&] (ee_symbol sym) {
          // try to recover the last logical op (==, !=, >, <, >=, <=)
          // this could help us save one instruction.
          ee_expr_cond_goto cgo;
          cgo.label_id = lbl;
          if(!exprs.empty()) {
            auto it = std::get_if<ee_expr_op>(&*exprs.rbegin());
            if(it && it->sym == sym) {
              int t = -1;
              for(int i = 0; i < 6; ++i) if(it->op == lops[i]) t = i;
              if(t >= 0) {
                cgo.a = it->a;
                cgo.b = it->b;
                cgo.lop = lops[t ^ inv];
                exprs.pop_back();
                exprs.push_back(cgo);
                return;
              }
            }
          }
          cgo.a = sym;
          cgo.b = 0;
          cgo.lop = lops[inv ^ 1];   // eq or neq.
          exprs.push_back(cgo);
        }
      }, cond);
  };
  
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
  }
  else if(auto it = dcast<ast_stmt_subblock>(stmt); it) {
    eeyore_gen_block(*it->b, declman, defs, exprs,
                     lbl_loop_st, lbl_loop_ed);
  }
  else if(auto it = dcast<ast_stmt_if>(stmt); it) {
    int lbl_jo_then = ++global_label_id;
    int lbl_jo_else;
    if(it->exec_else) lbl_jo_else = ++global_label_id;
    ee_rval cond = eval_exp(*it->cond, defs, exprs, declman);
    make_cond_goto(cond, true, lbl_jo_then);
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
    ee_rval cond = eval_exp(*it->cond, defs, exprs, declman);
    make_cond_goto(cond, true, lbl_ed);
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
  layered_store<g_def> defs(last_defs);
  for(auto bi: block.items) {
    if(auto it = dcast<ast_def>(bi); it) {
      push_def<'T'>(defs, it, exprs, declman);
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
    push_def<'T'>(defs, def, t_definits, declman);
  }
  
  ret->funcdefs.reserve(sysy->funcdefs.size());
  for(auto sysy_fdef: sysy->funcdefs) {
    auto &ee_f = ret->funcdefs.emplace_back();
    ee_f.name = sysy_fdef->name;
    ee_f.num_params = sysy_fdef->params.size();
    
    decl_symbol_manager func_declman(ee_f.decls, declman);
    layered_store<g_def> defs_with_params(defs);
    
    for(int i = 0; i < ee_f.num_params; ++i) {
      push_def<'p'>(defs_with_params, sysy_fdef->params[i], ee_f.exprs, func_declman);
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
