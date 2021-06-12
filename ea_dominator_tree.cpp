/**
 * @author Zizheng Guo
 * This implements Lengauer-Tarjan algorithm for dominator tree computation.
 */

#include "eeyore.hpp"
#include "eeyore_analysis.hpp"
#include <vector>
#include <algorithm>
#include <cassert>

void ee_dataflow::compute_dominator_tree() {
  int ndfn = 0;
  std::vector<int> dfn(n_exprs, -1), seq(n_exprs), semi(n_exprs), fa(n_exprs), inset(n_exprs), upset_minsemi(n_exprs);
  std::vector<std::vector<int>> dom(n_exprs);
  idom.resize(n_exprs);

  // compute dfs sequence
  const auto dfs_dfn = [&] (int u, auto &&dfs_dfn) -> void {
    dfn[u] = ndfn++;
    seq[dfn[u]] = u;
    for(int v: e_out[u]) if(dfn[v] == -1) {
        fa[v] = u;
        dfs_dfn(v, dfs_dfn);
      }
  };
  fa[0] = -1;
  dfs_dfn(0, dfs_dfn);
  for(int i = 1; i < n_exprs; ++i) assert(dfn[i] != -1);

  // compute semi dominator
  for(int i = 0; i < n_exprs; ++i) {
    semi[i] = i;
    inset[i] = i;
    upset_minsemi[i] = i;
  }
  const auto link = [&] (int v, int w) {
    assert(inset[w] == w);
    inset[w] = v;
  };
  const auto find = [&] (int v, auto &&find) {
    if(inset[v] == v) return v;
    int p = find(inset[v], find);
    if(dfn[semi[upset_minsemi[p]]] < dfn[semi[upset_minsemi[v]]])
      upset_minsemi[v] = upset_minsemi[p];
    return inset[v] = p;
  };
  const auto eval = [&] (int v) {
    if(inset[v] == v) return v;
    else {
      find(v, find);
      return upset_minsemi[v];
    }
  };

  return;
  
  for(int i = n_exprs - 1; i >= 1; --i) {
    int w = seq[i];
    for(int v: e_in[w]) {
      int u = eval(v);
      if(dfn[semi[u]] < dfn[semi[w]]) semi[w] = semi[u];
    }
    link(fa[w], w);
    dom[semi[w]].push_back(w);
    for(int v: dom[fa[w]]) {
      int u = eval(v);
      idom[v] = (dfn[semi[u]] < dfn[semi[v]]) ? u : fa[w];
    }
    dom[fa[w]].clear();
  }

  // finalize immediate dominator
  idom[0] = -1;
  for(int i = 1; i < n_exprs; ++i) {
    int w = seq[i];
    if(idom[w] != semi[w]) idom[w] = idom[idom[w]];
  }
  doms.resize(n_exprs);
  for(int i = 1; i < n_exprs; ++i) {
    doms[idom[i]].push_back(i);
  }
}
