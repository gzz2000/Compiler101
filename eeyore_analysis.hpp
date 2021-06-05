#pragma once

#include "eeyore.hpp"
#include <vector>
#include <unordered_map>
#include <functional>

struct ee_dataflow {
  // per instruction
  int n_exprs = 0;
  std::unordered_map<int, int> label2pos;
  std::vector<std::vector<int>> e_in, e_out;
  std::vector<int> loopcnt;

  // per symbol
  int n_decls;
  std::unordered_map<ee_symbol, int> sym2id;

  inline int s2i(ee_symbol sym) {
    if(auto it = sym2id.find(sym); it != sym2id.end()) return it->second;
    else return -1;
  }

  ee_dataflow(const ee_funcdef &eef);
  
  // BFS on the reverse graph.
  // bool foo(int): returns true if the bfs should stop here.
  // caveat: foo actually handles the "visit?" process in BFS.
  // make sure it does this or the time complexity would become exponential.
  // does NOT preserve topological order.
  void bfs_back(int start, std::function<bool(int)> foo);
};
