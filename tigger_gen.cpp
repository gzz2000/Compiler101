/**
 * @author Zizheng Guo
 * This implements global register allocation based on graph coloring.
 */

#include "tigger.hpp"
#include "sysy.hpp"
#include "sysy.tab.hpp"
#include "eeyore.hpp"
#include "eeyore_analysis.hpp"
#include "utils.hpp"
#include <cassert>
#include <algorithm>
#include <stack>
#include <vector>
#include <set>

struct cstat_type {
  int stackpos = -1;
  int size;
  int color = -1;    // -1 means {uncolored, spilled}
  bool is_array = false;
};

tg_funcdef tigger_func_gen(
  const ee_funcdef &eef,
  const std::unordered_map<int, std::optional<int>> &global_decl_map)
{
  tg_funcdef tgf;
  tgf.name = eef.name;
  tgf.num_params = eef.num_params;

  // build dataflow
  ee_dataflow df(eef);
  
  // drawback:
  //   once a var is put into the stack, it will stay there forever
  //   there is no "splitting" available.
  //   this is for easy implementation.

  std::vector<cstat_type> cstats(df.n_decls);

  // tag the arrays, so that they are not colored
  for(const auto &decl: eef.decls) {
    int t = df.s2i(decl.sym);
    if(decl.size) cstats[t].is_array = true;
  }

  // build interference graph
  // compute load-use relations
  std::vector<std::vector<int>> active_vars(df.n_exprs);
  std::vector<bool> expr_used(df.n_exprs);
  
  for(int i = 0; i < df.n_exprs; ++i) {
    const auto proc_use_sym = [&] (ee_symbol sym) {
      int symid = df.s2i(sym);
      if(symid == -1) return;
      df.bfs_back(i, [&] (int j) -> bool {
        int assign = -1;
        std::visit(overloaded{
            [&] (ee_expr_op e) {
              assign = df.s2i(e.sym);
            },
            [&] (ee_expr_assign e) {
              if(!e.lval.sym_idx) assign = df.s2i(e.lval.sym);
            },
            [&] (ee_expr_assign_arr e) {
              assign = df.s2i(e.sym);
            },
            [] (ee_expr_cond_goto) {},
            [] (ee_expr_goto) {},
            [] (ee_expr_label) {},
            [&] (const ee_expr_call &e) {
              if(e.store) assign = df.s2i(*e.store);
            },
            [] (ee_expr_ret) {}
          }, eef.exprs[j]);
        if(assign == symid) {
          expr_used[j] = true;
          if(i != j) return true;
        }
        for(int oid: active_vars[j]) if(oid == symid) return true;
        active_vars[j].push_back(symid);
        return false;
      });
    };
    const auto proc_use_rval = [&] (ee_rval rval) {
      if(auto p = std::get_if<ee_symbol>(&rval); p) {
        proc_use_sym(*p);
      }
    };
    const auto pu = overloaded{proc_use_sym, proc_use_rval};
    std::visit(overloaded{
        [&] (ee_expr_op e) {
          pu(e.a);
          pu(e.b);
        },
        [&] (ee_expr_assign e) {
          if(e.lval.sym_idx) {
            pu(e.lval.sym);
            pu(*e.lval.sym_idx);
          }
          pu(e.a);
        },
        [&] (ee_expr_assign_arr e) {
          pu(e.a.sym);
          pu(*e.a.sym_idx);
        },
        [&] (ee_expr_cond_goto e) {
          pu(e.a);
          pu(e.b);
        },
        [] (ee_expr_goto) {},
        [] (ee_expr_label) {},
        [&] (const ee_expr_call &e) {
          for(ee_rval param: e.params) pu(param);
        },
        [&] (ee_expr_ret e) {
          if(e.val) pu(*e.val);
        }
      }, eef.exprs[i]);
  }

  // materialize the relation
  std::vector<std::set<int>> interf(df.n_decls), interf_tmp;
  for(int i = 0; i < df.n_exprs; ++i) {
    for(int j = 0; j < (int)active_vars[i].size(); ++j) {
      for(int k = j + 1; k < (int)active_vars[i].size(); ++k) {
        int a = active_vars[i][j], b = active_vars[i][k];
        interf[a].insert(b);
        interf[b].insert(a);
      }
    }
  }
  interf_tmp = interf;

  // initialize coloring heuristics
  constexpr int max_colors = 25;   // 27 - 2
  int palette[max_colors] = {};
  int reg_to_color[max_colors + 5] = {};
  std::unordered_map<int, int> suggest_reg;

  for(int i = 0; i < max_colors; ++i) palette[i] = -2;
  for(int i = 0; i < max_colors + 5; ++i) reg_to_color[i] = -1;
  
  const auto match_palette = [&] (ee_symbol sym, int reg) {
    int t = df.s2i(sym);
    if(t == -1) return;
    suggest_reg[t] = reg;
  };
  // for function calls (parameter, return value) and ret
  int cnt_fcalls = 0;
  for(const auto &expr: eef.exprs) {
    if(auto p = std::get_if<ee_expr_call>(&expr); p) {
      ++cnt_fcalls;
      if(p->store) {
        match_palette(*p->store, 20);
      }
      assert(p->params.size() <= 8);
      for(int i = 0; i < (int)p->params.size(); ++i) {
        if(auto q = std::get_if<ee_symbol>(&p->params[i]); q) {
          match_palette(*q, 20 + i);
        }
      }
    }
    else if(auto p = std::get_if<ee_expr_ret>(&expr); p) {
      if(p->val) {
        if(auto q = std::get_if<ee_symbol>(&*p->val); q) {
          match_palette(*q, 20);
        }
      }
    }
  }
  // for input parameters, if there is no function call.
  // (if there is function call, this will cause redundant save/restore)
  if(!cnt_fcalls) {
    for(int i = 0; i < tgf.num_params; ++i) {
      match_palette(ee_symbol{'p', i}, 20 + i /* ai */);
    }
  }
  
  // color the graph according to heuristics, and tag all spills
  const auto comp_by_loopcnt = [&] (int a, int b) {
    if(df.loopcnt[a] != df.loopcnt[b]) return df.loopcnt[a] < df.loopcnt[b];
    else return a < b;
  };
  std::set<int, decltype(comp_by_loopcnt)> remaining(comp_by_loopcnt);
  std::stack<int> pend;
  
  for(int i = 0; i < df.n_decls; ++i) {
    remaining.insert(i);
  }
  const auto &popremaining = [&] (decltype(remaining)::iterator it) {
    pend.push(*it);
    for(int j: interf_tmp[*it]) interf_tmp[j].erase(*it);
    return remaining.erase(it);
  };
  while(!remaining.empty()) {
    bool met = false;
    for(auto it = remaining.begin(); it != remaining.end(); ) {
      if(interf_tmp[*it].size() < max_colors) {
        it = popremaining(it);
        met = true;
      }
      else ++it;
    }
    if(met) continue;
    popremaining(remaining.begin());   // risk spilling.
  }
  while(!pend.empty()) {
    int u = pend.top(); pend.pop();
    if(cstats[u].is_array) continue;  // do not assign register to an array
    // todo: need to optimize: loadaddr can be reused.
    bool adj[max_colors] = {};
    for(int v: interf[u]) {
      if(cstats[v].color != -1) adj[cstats[v].color] = true;
    }
    for(int i = 0; i < max_colors; ++i) {
      if(!adj[i]) {
        cstats[u].color = i;
        break;
      }
    }
    if(cstats[u].color == -1) continue; // spilled
    if(auto p = suggest_reg.find(u); p != suggest_reg.end()) {
      int sr = p->second;
      if(reg_to_color[sr] == -1) {
        while(adj[cstats[u].color] || palette[cstats[u].color] != -2)
          ++cstats[u].color;
        assert(cstats[u].color < max_colors);
        reg_to_color[sr] = cstats[u].color;
        palette[cstats[u].color] = sr;
      }
      else if(!adj[reg_to_color[sr]]) {
        cstats[u].color = reg_to_color[sr];
      }
    }
  }

  // match all other registers arbitrarilly.
  // available:
  //   s0 s1 s2 s3 s4 s5 s6 s7 s8 s9 s10 s11;
  //   t2 t3 t4 t5 t6;   // t0, t1 are reserved.
  //   a0 a1 a2 a3 a4 a5 a6 a7;
  // if #fcalls is 0, we would like to use t2--t6, a0--a7 more frequently.
  // else, we would use s0--s11 more frequently.
  for(int i = 0; i < df.n_decls; ++i) {
    if(cstats[i].color != -1 && palette[cstats[i].color] == -2)
      palette[cstats[i].color] = -1;
  }
  const auto match_rest = [&] (int regl, int regr) {
    for(int r = regl; r <= regr; ++r) if(reg_to_color[r] == -1) {
        for(int i = 0; i < max_colors; ++i) if(palette[i] == -1) {
            palette[i] = r;
            reg_to_color[r] = i;
            break;
          }
      }
  };
  if(cnt_fcalls == 0) {
    match_rest(15, 27);   // t2--t6, a0--a7
    match_rest(1, 12);    // s0--s11
  }
  else {
    match_rest(1, 12);    // s0--s11
    match_rest(15, 27);   // t2--t6, a0--a7
  }

  // generate code
  // for t2--t6 and a0--a7, they need to be stored upon each function call.
  //   range: active_vars \ params (to check?)
  // for s0--s11, they should be stored on enter and on exit
  //   range: used reg: reg_to_color[i] != -1.

  // allocate space on stack
  // for parameters
  for(int i = 0; i < tgf.num_params; ++i) {
    int t = df.s2i(ee_symbol{'p', i});
    cstats[t].stackpos = tgf.size_stack++;
    cstats[t].size = 1;
  }
  // for registers
  int reg_stackpos[max_colors + 5] = {};
  for(int i = 1; i <= 12; ++i) {   // s0--s11
    if(reg_to_color[i] != -1) reg_stackpos[i] = tgf.size_stack++;
  }
  if(cnt_fcalls) for(int i = 15; i <= 27; ++i) {  // t2--t6, a0--a7
      if(reg_to_color[i] != -1) reg_stackpos[i] = tgf.size_stack++;
    }
  // for variables
  for(const auto &decl: eef.decls) {
    int t = df.s2i(decl.sym);
    int size = decl.size ? *decl.size : 1;
    cstats[t].size = size;
    if(cstats[t].color == -1) {
      cstats[t].stackpos = tgf.size_stack;
      tgf.size_stack += size;
    }
  }

  // ----- HELPER FUNCTIONS -----

  // load_val: load @sym to a register, suggest temp reg @tmp
  // @return: register that @sym is now loaded into
  const auto load_val = [&] (ee_symbol sym, tg_reg tmp) {
    int t = df.s2i(sym);
    if(t == -1) {   // global var
      auto p = global_decl_map.find(sym.id);
      assert(p != global_decl_map.end());
      if(p->second) tgf.exprs.push_back(tg_expr_global_loadaddr{sym.id, tmp});
      else tgf.exprs.push_back(tg_expr_global_load{sym.id, tmp});
      return tmp;
    }
    if(cstats[t].is_array) { // stack pointer
      tgf.exprs.push_back(tg_expr_stack_loadaddr{cstats[t].stackpos, tmp});
      return tmp;
    }
    if(cstats[t].color != -1) return tg_reg{palette[cstats[t].color]};
    tgf.exprs.push_back(tg_expr_stack_load{cstats[t].stackpos, tmp});
    return tmp;
  };
  
  // load_rval: load rvalue @rval into a register. suggest @tmp.
  const auto load_rval = [&] (ee_rval rval, tg_reg tmp) {
    tg_reg ret;
    std::visit(overloaded{
        [&] (ee_symbol sym) {
          ret = load_val(sym, tmp);
        },
        [&] (int v) {
          if(v == 0) {
            ret = tg_reg{0};  // x0
          }
          else {
            tgf.exprs.push_back(tg_expr_assign_c{tmp, v});
            ret = tmp;
          }
        }
      }, rval);
    return ret;
  };

  // load_rval_to: force loading @rval to register @to
  const auto load_rval_to = [&] (ee_rval rval, tg_reg to) {
    tg_reg rto = load_rval(rval, to);
    if(rto.id != to.id)
      tgf.exprs.push_back(tg_expr_assign_c{to, rto});
  };
  
  // save_val: find a register for storing @sym, suggest @tmp
  // @return: the register to store @sym
  const auto save_val = [&] (ee_symbol sym, tg_reg tmp) {
    int t = df.s2i(sym);
    if(t == -1) return tmp;
    assert(cstats[t].size == 1);
    if(cstats[t].color != -1) return tg_reg{palette[cstats[t].color]};
    return tmp;
  };
  
  // save_val_apply: assume already saved to the allocated register,
  //   now save the value to the right place (maybe memory).
  // @caveat: will destroy both 2 tmp regs.
  const auto save_val_apply = [&] (ee_symbol sym, tg_reg tmp) {
    int t = df.s2i(sym);
    if(t == -1) {
      // caveat: here we find another tmp reg.
      tg_reg tmp2{tmp.id == 13 ? 14 : 13};
      tgf.exprs.push_back(tg_expr_global_loadaddr{sym.id, tmp2});
      tgf.exprs.push_back(tg_expr_assign_la{tmp2, 0, tmp});
      return;
    }
    if(cstats[t].color == -1) {
      tgf.exprs.push_back(tg_expr_stack_store{tmp, cstats[t].stackpos});
    }
  };

  // save_val_from: force saving @val to @sym
  const auto save_val_from = [&] (ee_symbol sym, tg_reg val) {
    tg_reg rto = save_val(sym, val);
    if(rto.id != val.id)
      tgf.exprs.push_back(tg_expr_assign_c{rto, val});
    save_val_apply(sym, val);
  };
  
  // load_addr: load the address of lvalue @lv into suggested @to,
  //   with scratch space suggested @tmp.
  // @return [reg, NUM], used as: reg[NUM] in tigger exprs.
  // @caveat: will destroy both 2 tmp regs.
  const auto load_addr = [&] (ee_lval lv, tg_reg to, tg_reg tmp) {
    assert(lv.sym_idx);
    int arrt = df.s2i(lv.sym);
    tg_reg ret_reg;
    int ret_num;
    if(arrt == -1) {   // global var
      tgf.exprs.push_back(tg_expr_global_loadaddr{lv.sym.id, to});
      ret_reg = to;
    }
    else if(!cstats[arrt].is_array) {  // pointer passed as parameter
      assert(lv.sym.type == 'p');
      ret_reg = load_val(lv.sym, to);
    }
    else {     // on stack
      assert(lv.sym.type != 'p');
      assert(cstats[arrt].stackpos != -1);
      tgf.exprs.push_back(tg_expr_stack_loadaddr{cstats[arrt].stackpos, to});
      ret_reg = to;
    }
    std::visit(overloaded{
        [&] (ee_symbol sym) {
          tg_expr_op add;
          add.op = OP_ADD; add.numop = 2;
          add.lval = to;
          add.a = ret_reg;
          add.b = load_val(sym, tmp);
          tgf.exprs.push_back(add);
          
          ret_reg = to;
          ret_num = 0;
        },
        [&] (int v) {
          ret_num = v;
        }
      }, *lv.sym_idx);
    return std::make_pair(ret_reg, ret_num);
  };

  // cycle_param_rearrange: general abstraction of parameter arrangement.
  // on entering the function and storing the parameters to right place,
  // and on calling a next function with parameters to be filled,
  // we need to rearrange the parameters from/to a0--a7 to/from
  // their allocated palette[color[]].
  // this is nontrivial because we may need to permute some of them in-situ,
  // and this is implemented below as a cycle-checking subroutine.
  const auto cycle_param_rearrange = [&] (
    int n, auto &&f_pos, auto &&f_brk, auto &&f_nrm, auto &&f_rst)
  {
    int storing[10] = {};
    int status[10] = {};
    bool broke_here[10] = {};
    for(int i = 0; i < 10; ++i) storing[i] = -1;
    for(int i = 0; i < n; ++i) {
      f_pos(storing, i);
    }
    const auto put = [&] (int i, auto &&put) {
      status[i] = 1;
      if(storing[i] == i) {
        status[i] = 2;
        return;
      }
      int coinc = storing[i];
      if(coinc != -1 && status[coinc] == 0) {
        put(coinc, put);
      }
      if(coinc != -1 && status[coinc] == 1) {
        // break cycle.
        f_brk(coinc, i);
        broke_here[coinc] = true;
        coinc = -1;
      }
      assert(coinc == -1 || status[coinc] == 2);
      if(broke_here[i]) f_rst(i);
      else f_nrm(i);
      status[i] = 2;
    };
    for(int i = 0; i < n; ++i) {
      if(!status[i]) put(i, put);
    }
  };

  // code generation

  // store used s0--s11
  for(int i = 1; i <= 12; ++i) {
    if(reg_to_color[i] != -1)
      tgf.exprs.push_back(tg_expr_stack_store{{i}, reg_stackpos[i]});
  }

  // store spilled params: a0--a7
  cycle_param_rearrange(
    tgf.num_params,
    [&] (int *storing, int i) {
      int t = df.s2i(ee_symbol{'p', i});
      if(cstats[t].color != -1) {
        int r = palette[cstats[t].color];
        if(r >= 20 && r < 20 + tgf.num_params) storing[i] = r - 20;
      }
    },
    [&] (int coinc, int i) {
      (void)i;
      tgf.exprs.push_back(
        tg_expr_assign_c{tg_reg{13}, tg_reg{20 + coinc}});
    },
    [&] (int i) {
      save_val_from(ee_symbol{'p', i}, tg_reg{20 + i});
    },
    [&] (int i) {
      save_val_from(ee_symbol{'p', i}, tg_reg{13});
    });
  // for(int i = 0; i < tgf.num_params; ++i) {
  //   save_val_from(ee_symbol{'p', i}, tg_reg{20 + i});
  // }

  // translate expressions one by one
  for(int i = 0; i < df.n_exprs; ++i) {
    std::visit(overloaded{
        [&] (ee_expr_op e) {
          assert(!(std::get_if<int>(&e.a) && std::get_if<int>(&e.b)));
          if(df.s2i(e.sym) != -1 && !expr_used[i]) return;
          
          // single operand, no optimization available
          if(e.numop == 1) {
            tg_reg ra = load_val(std::get<ee_symbol>(e.a), tg_reg{13});
            if(e.op != OP_ADD) {
              tg_expr_op op;
              op.op = e.op; op.numop = 1;
              op.lval = save_val(e.sym, tg_reg{13});
              op.a = ra;
              tgf.exprs.push_back(op);
              save_val_apply(e.sym, tg_reg{13});
            }
            else {
              save_val_from(e.sym, ra);
            }
            return;
          }
          
          // commutative transform for convenient immediate value
          if(std::get_if<int>(&e.a) && (
               e.op == OP_ADD || e.op == OP_MUL || e.op == OP_LOR ||
               e.op == OP_LAND || e.op == OP_EQ || e.op == OP_NEQ ||
               // below: simple op reverse will be ok
               e.op == OP_LT || e.op == OP_GT ||
               e.op == OP_LE || e.op == OP_GE))
          {
            std::swap(e.a, e.b);
            if(e.op == OP_LT) e.op = OP_GT;
            else if(e.op == OP_GT) e.op = OP_LT;
            else if(e.op == OP_LE) e.op = OP_GE;
            else if(e.op == OP_GE) e.op = OP_LE;
          }

          tg_expr_op op;
          op.op = e.op; op.numop = 2;
          op.lval = save_val(e.sym, tg_reg{13});
          op.a = load_rval(e.a, tg_reg{13});
          std::visit(overloaded{
              [&] (ee_symbol sym) {
                op.b = load_val(sym, tg_reg{14});
              },
              [&] (int v) {
                op.b = v;
              }
            }, e.b);
          tgf.exprs.push_back(op);
          save_val_apply(e.sym, tg_reg{13});
        },

        [&] (ee_expr_assign e) {
          
          if(e.lval.sym_idx) {
            auto [reg, num] = load_addr(e.lval, tg_reg{13}, tg_reg{14});
            tg_expr_assign_la as;
            as.lreg = reg;
            as.lidx = num;
            as.a = load_rval(e.a, tg_reg{14});
            tgf.exprs.push_back(as);
          }
          else {
            if(df.s2i(e.lval.sym) != -1 && !expr_used[i]) return;
            std::visit(overloaded{
                [&] (ee_symbol sym) {
                  tg_reg t = load_val(sym, tg_reg{13});
                  save_val_from(e.lval.sym, t);
                },
                [&] (int v) {
                  tg_expr_assign_c as;
                  as.lval = save_val(e.lval.sym, tg_reg{13});
                  as.a = v;
                  tgf.exprs.push_back(as);
                  save_val_apply(e.lval.sym, tg_reg{13});
                }
              }, e.a);
          }
        },

        [&] (ee_expr_assign_arr e) {
          auto [reg, num] = load_addr(e.a, tg_reg{13}, tg_reg{14});
          tg_expr_assign_ra as;
          as.lval = save_val(e.sym, tg_reg{14});
          as.areg = reg;
          as.aidx = num;
          tgf.exprs.push_back(as);
          save_val_apply(e.sym, tg_reg{14});
        },

        [&] (ee_expr_cond_goto e) {
          tgf.exprs.push_back(tg_expr_cond_goto{
              load_rval(e.a, tg_reg{13}),
              load_rval(e.b, tg_reg{14}),
              e.lop,
              e.label_id
            });
        },

        [&] (ee_expr_goto e) {
          tgf.exprs.push_back(tg_expr_goto{e.label_id});
        },

        [&] (ee_expr_label e) {
          tgf.exprs.push_back(tg_expr_label{e.label_id});
        },

        [&] (const ee_expr_call &e) {
          // each active var in t2--t6, a0--a7
          // after this call need to be saved and restored,
          // except for the return value.
          bool nxt_inuse[max_colors + 5] = {};
          int rett = e.store ? df.s2i(*e.store) : -1;
          if(i + 1 < df.n_exprs) for(int i: active_vars[i + 1]) {
              if(i != rett && cstats[i].color != -1)
                nxt_inuse[palette[cstats[i].color]] = true;
            }
          // save
          for(int i = 15; i <= 27; ++i) if(nxt_inuse[i]) {
              tgf.exprs.push_back(tg_expr_stack_store{{i}, reg_stackpos[i]});
            }
          
          // arrange params
          // recursive, loop-breaking
          assert(e.params.size() <= 8);
          cycle_param_rearrange(
            (int)e.params.size(),
            [&] (int *storing, int i) {
              if(auto p = std::get_if<ee_symbol>(&e.params[i]); p) {
                int t = df.s2i(*p);
                if(t != -1 && cstats[t].color != -1) {
                  int color = palette[cstats[t].color];
                  if(color >= 20 && color < 20 + (int)e.params.size())
                    storing[color - 20] = i;
                }
              }
            },
            [&] (int coinc, int i) {
              (void)coinc;
              tgf.exprs.push_back(
                tg_expr_assign_c{tg_reg{13}, tg_reg{20 + i}});
            },
            [&] (int i) {
              load_rval_to(e.params[i], tg_reg{20 + i});
            },
            [&] (int i) {
              tgf.exprs.push_back(
                tg_expr_assign_c{tg_reg{20 + i}, tg_reg{13}});
            });
          
          // call and save return value
          tgf.exprs.push_back(tg_expr_call{e.func});
          if(e.store) {
            save_val_from(*e.store, tg_reg{20});
          }
          // restore
          for(int i = 15; i <= 27; ++i) if(nxt_inuse[i]) {
              tgf.exprs.push_back(tg_expr_stack_load{reg_stackpos[i], {i}});
            }
        },

        [&] (ee_expr_ret e) {
          // set return value
          if(e.val) {
            load_rval_to(*e.val, tg_reg{20});
          }
          // restore s0--s11
          for(int i = 1; i <= 12; ++i) {
            if(reg_to_color[i] != -1)
              tgf.exprs.push_back(tg_expr_stack_load{reg_stackpos[i], {i}});
          }
          // give out control
          tgf.exprs.push_back(tg_expr_ret{});
        }
      }, eef.exprs[i]);
  }
  return tgf;
}

std::shared_ptr<tg_program> tigger_gen(std::shared_ptr<ee_program> eeprog) {
  std::shared_ptr<tg_program> ret = std::make_shared<tg_program>();
  std::unordered_map<int, std::optional<int>> global_decl_map;
  // decl
  for(const auto &decl: eeprog->decls) {
    assert(decl.sym.type == 'T');
    ret->decls.push_back(tg_global_decl{decl.sym.id, decl.size});
    global_decl_map[decl.sym.id] = decl.size;
  }
  // funcdefs
  for(const auto &funcdef: eeprog->funcdefs) {
    ret->funcdefs.push_back(tigger_func_gen(funcdef, global_decl_map));
  }
  return ret;
}
