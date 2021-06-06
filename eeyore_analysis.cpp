#include "eeyore.hpp"
#include "eeyore_analysis.hpp"
#include <queue>

ee_dataflow::ee_dataflow(const ee_funcdef &eef)
  : n_exprs((int)eef.exprs.size()),
    e_in(n_exprs), e_out(n_exprs), loopcnt(n_exprs + 1, 0),
    n_decls(0)
{
  // build map label_id -> pos
  for(int i = 0; i < n_exprs; ++i) {
    if(auto c = std::get_if<ee_expr_label>(&eef.exprs[i]); c) {
      label2pos[c->label_id] = i;
    }
  }
  
  // build e_in and e_out
  for(int i = 0; i < n_exprs; ++i) {
    if(auto c = std::get_if<ee_expr_goto>(&eef.exprs[i]); c) {
      e_in[label2pos[c->label_id]].push_back(i);
      e_out[i].push_back(label2pos[c->label_id]);
      continue;
    }
    if(auto c = std::get_if<ee_expr_cond_goto>(&eef.exprs[i]); c) {
      e_in[label2pos[c->label_id]].push_back(i);
      e_out[i].push_back(label2pos[c->label_id]);
    }
    if(i + 1 < (int)eef.exprs.size()) {
      e_in[i + 1].push_back(i);
      e_out[i].push_back(i + 1);
    }
  }

  // count loop for heuristics
  for(int i = 0; i < n_exprs; ++i) {
    for(int j: e_out[i]) {
      if(j < i) {
        ++loopcnt[j];
        --loopcnt[i + 1];
      }
    }
  }
  for(int i = 1; i <= n_exprs; ++i) loopcnt[i] += loopcnt[i - 1];

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
  while(!q.empty()) {
    int u = q.front(); q.pop();
    for(int v: e_in[u]) {
      if(foo(v)) continue;
      q.push(v);
    }
  }
}
