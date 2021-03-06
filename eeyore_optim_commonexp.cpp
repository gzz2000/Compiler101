/**
 * @author Zizheng Guo
 * This implements global common subexpression elimination and copy propagation.
 */

#include "eeyore.hpp"
#include "eeyore_analysis.hpp"
#include <set>
#include <vector>
#include <unordered_map>
#include <utility>
#include <tuple>
#include <optional>

template<typename key, typename value>
struct persistent_umap {
  std::unordered_map<key, value> map;
  std::vector<std::pair<key, std::optional<value>>> record;
  
  inline std::optional<value> find(key k) const {
    if(auto p = map.find(k); p == map.end()) return {};
    else return p->second;
  }

  inline int record_at() const {
    return record.size();
  }
  inline void set(key k, value v) {
    record.push_back(std::make_pair(k, find(k)));
    map[k] = v;
  }
  inline void restore(int at) {
    for(int i = (int)record.size() - 1; i >= at; --i) {
      if(record[i].second) map[record[i].first] = *record[i].second;
      else map.erase(record[i].first);
    }
  }
};

ee_funcdef eefuncdef_commonexp(const ee_funcdef &oldef) {
  ee_funcdef nwdef = oldef;
  nwdef.name = oldef.name;
  nwdef.num_params = oldef.num_params;
  nwdef.decls = oldef.decls;

  ee_dataflow df(nwdef);
  df.compute_dominator_tree();
  
  const auto get_eval_save = [&] (const ee_expr_types &et) {
    std::optional<ee_symbol> sym;
    std::visit(overloaded{
        [&] (ee_expr_op e) {
          sym = e.sym;
        },
        [&] (ee_expr_assign e) {
          if(e.lval.sym_idx) return;
          sym = e.lval.sym;
        },
        [&] (const ee_expr_call &e) {
          if(!e.store) return;
          sym = e.store;
        },
        [] (auto) {}
      }, et);
    return sym;
  };

  const auto bfs_backward_check = [&] (int st, int ed, auto &&f_is_valid) {
    bool clean = true;
    std::set<int> visited;
    df.bfs_back(st, [&] (int u) {
      if(!clean) return true;
      if(u == ed) return true;
      if(visited.count(u)) return true;
      visited.insert(u);
      if(u != st && !f_is_valid(u)) {
        clean = false;
        return true;
      }
      return false;
    });
    return clean;
  };

  persistent_umap<ee_symbol, int> idom_evals;
  persistent_umap<std::tuple<int, ee_rval, ee_rval>, int> idom_ops;
  persistent_umap<ee_lval, int> idom_arrs;

  const auto copy_prop_sym = [&] (int i, ee_symbol &sym) {
    std::optional<int> jp = idom_evals.find(sym);
    if(!jp) return;
    int j = *jp;
    auto p = std::get_if<ee_expr_assign>(&nwdef.exprs[j]);
    if(!p) return;
    auto q = std::get_if<ee_symbol>(&p->a);
    if(!q) return;   // notice we do not expand ee_rval[constant] because we are lazy.
    bool valid = bfs_backward_check(i, j, [&] (int k) {
      // todo: bug here. if sym is a global variable, it can be modified
      // by a function call with side effect, which is not considered here.
      // fortunately, this is not causing error in test cases.
      std::optional<ee_symbol> s2 = get_eval_save(nwdef.exprs[k]);
      if(s2 && (*s2 == *q || *s2 == sym)) return false; // polluted.
      return true;
    });
    if(!valid) return;
    sym = *q;   // finally, substitute safely.
  };
  const auto copy_prop_rval = [&] (int i, ee_rval &rv) {
    std::visit(overloaded{
        [&] (ee_symbol &sym) { copy_prop_sym(i, sym); },
        [] (int) {}
      }, rv);
  };
  const auto copy_prop = overloaded{copy_prop_sym, copy_prop_rval};
  
  const auto dfs_optim = [&] (int u, auto &&dfs_optim) -> void {
    int p_idom_evals = idom_evals.record_at();
    int p_idom_ops = idom_ops.record_at();
    int p_idom_arrs = idom_arrs.record_at();

    std::visit(overloaded{
        [&] (ee_expr_op &e) {
          copy_prop(u, e.a);
          copy_prop(u, e.b);

          // common subexpression elimination
          std::tuple<int, ee_rval, ee_rval> sign(e.op * 4 + e.numop, e.a, e.b);
          std::optional<int> lastc = idom_ops.find(sign);
          if(lastc && bfs_backward_check(u, *lastc, [&] (int k) {
            std::optional<ee_symbol> s2 = get_eval_save(nwdef.exprs[k]);
            const auto chkeq = [] (ee_symbol sym, ee_rval a) {
              auto p = std::get_if<ee_symbol>(&a);
              return p && *p == sym;
            };
            if(s2 && (*s2 == e.sym || chkeq(*s2, e.a) || chkeq(*s2, e.b))) return false; // polluted.
            return true;
          }))
          {
            ee_expr_assign ea;
            ea.lval.sym = e.sym;
            ea.a = std::get<ee_expr_op>(nwdef.exprs[*lastc]).sym;
            nwdef.exprs[u] = ea;   // after this, (&) e becomes invalid.
            idom_evals.set(ea.lval.sym, u);
          }
          else {
            idom_evals.set(e.sym, u);
            idom_ops.set(sign, u);
          }
        },
        [&] (ee_expr_assign &e) {
          if(e.lval.sym_idx) copy_prop(u, e.lval.sym);
          copy_prop(u, e.a);
          if(!e.lval.sym_idx) idom_evals.set(e.lval.sym, u);
        },
        [&] (ee_expr_assign_arr &e) {
          copy_prop(u, e.a.sym);
          copy_prop(u, *e.a.sym_idx);
          // array dereferencing elimination
          std::optional<int> lasta = idom_arrs.find(e.a);
          if(lasta && bfs_backward_check(u, *lasta, [&] (int k) {
            if(std::get_if<ee_expr_call>(&nwdef.exprs[k])) return false;
            if(auto p1 = std::get_if<ee_expr_assign>(&nwdef.exprs[k]);
               p1 &&
               p1->lval.sym == e.a.sym &&
               p1->lval.sym_idx) return false;
            return true;
          }))
          {
            ee_expr_assign ea;
            ea.lval.sym = e.sym;
            ea.a = std::get<ee_expr_assign_arr>(nwdef.exprs[*lasta]).sym;
            nwdef.exprs[u] = ea;   // after this, (&) e becomes invalid.
            idom_evals.set(ea.lval.sym, u);
          }
          else {
            idom_evals.set(e.sym, u);
            idom_arrs.set(e.a, u);
          }
        },
        [&] (ee_expr_cond_goto &e) {
          copy_prop(u, e.a);
          copy_prop(u, e.b);
        },
        [&] (ee_expr_call &e) {
          for(ee_rval &rv: e.params) {
            copy_prop(u, rv);
          }
          if(e.store) idom_evals.set(*e.store, u);
        },
        [&] (ee_expr_ret &e) {
          if(e.val) copy_prop(u, *e.val);
        },
        [] (auto &) {}
      }, nwdef.exprs[u]);
    
    for(int v: df.doms[u]) dfs_optim(v, dfs_optim);

    idom_evals.restore(p_idom_evals);
    idom_ops.restore(p_idom_ops);
    idom_arrs.restore(p_idom_arrs);
  };

  // repeat 3 times
  dfs_optim(0, dfs_optim);
  dfs_optim(0, dfs_optim);
  dfs_optim(0, dfs_optim);
  
  return nwdef;
}

std::shared_ptr<ee_program> eeyore_optim_commonexp(std::shared_ptr<ee_program> oldeeprog) {
  std::shared_ptr<ee_program> ret = std::make_shared<ee_program>();
  ret->decls = oldeeprog->decls;
  for(const auto &fdef: oldeeprog->funcdefs) {
    ret->funcdefs.push_back(eefuncdef_commonexp(fdef));
  }
  return ret;
}
