#include "eeyore.hpp"
#include "eeyore_analysis.hpp"
#include <queue>

ee_dataflow::ee_dataflow(const ee_funcdef &eef) {
  // build map label_id -> pos
  n_exprs = (int)eef.exprs.size();
  for(int i = 0; i < n_exprs; ++i) {
    if(auto c = std::get_if<ee_expr_label>(&eef.exprs[i]); c) {
      label2pos[c->label_id] = i;
    }
  }
  
  // build e_in and e_out
  e_in.resize(n_exprs);
  e_out.resize(n_exprs);
  for(int i = 0; i < n_exprs; ++i) {
    if(auto c = std::get_if<ee_expr_goto>(&eef.exprs[i]); c) {
      e_in[c->label_id].push_back(i);
      e_out[i].push_back(c->label_id);
      continue;
    }
    if(auto c = std::get_if<ee_expr_cond_goto>(&eef.exprs[i]); c) {
      e_in[c->label_id].push_back(i);
      e_out[i].push_back(c->label_id);
    }
    if(i + 1 < (int)eef.exprs.size()) {
      e_in[i + 1].push_back(i);
      e_out[i].push_back(i + 1);
    }
  }

  // build symbol list
  for(int i = 0; i < eef.num_params; ++i) {
    sym2id[ee_symbol{'p', i}] = n_decls++;
  }
  for(const auto &decl: eef.decls) {
    sym2id[decl.sym] = n_decls++;
  }
}

void ee_dataflow::bfs_back(int start, std::function<bool(int)> foo) {
  if(foo(start)) return;
  std::queue<int> q;
  q.push(start);
  while(true) {
    int u = q.front(); q.pop();
    for(int v: e_in[u]) {
      if(foo(v)) continue;
      q.push(v);
    }
  }
}
