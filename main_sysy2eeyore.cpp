#include <iostream>
#include <fstream>
#include "sysy.hpp"
#include "eeyore.hpp"

extern std::shared_ptr<ast_compunit> read_source_ast(const char *fname);

extern std::shared_ptr<ee_program> eeyore_gen(std::shared_ptr<ast_compunit> sysy);

extern void dump_eeyore(std::shared_ptr<ee_program> eeprog, std::ostream &out);

int main(int argc, char **argv) {
  if(argc != 6) {
    printf("Usage: %s -S -e <source.sy> -o <output.eeyore>\n", argv[0]);
    return 255;
  }
  std::shared_ptr<ast_compunit> sysy = read_source_ast(argv[3]);
  std::shared_ptr<ee_program> eeyore = eeyore_gen(sysy);
  std::ofstream fout(argv[5]);
  dump_eeyore(eeyore, fout);
  return 0;
}
