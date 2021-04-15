#include <iostream>
#include "sysy.hpp"
#include "eeyore.hpp"

extern std::shared_ptr<ast_compunit> read_source_ast(const char *fname);

extern std::shared_ptr<ee_program> eeyore_gen(std::shared_ptr<ast_compunit> sysy);

extern void dump_eeyore(std::shared_ptr<ee_program> eeprog, std::ostream &out);

int main(int argc, char **argv) {
  if(argc != 2) {
    printf("Usage: %s <source_file>\n", argv[0]);
    return 255;
  }
  std::shared_ptr<ast_compunit> sysy = read_source_ast(argv[1]);
  std::shared_ptr<ee_program> eeyore = eeyore_gen(sysy);
  dump_eeyore(eeyore, std::cout);
  return 0;
}
