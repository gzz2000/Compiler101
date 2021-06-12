#include <iostream>
#include <fstream>
#include "sysy.hpp"
#include "eeyore.hpp"
#include "tigger.hpp"

extern std::shared_ptr<ast_compunit> read_source_ast(const char *fname);

extern std::shared_ptr<ee_program> eeyore_gen(std::shared_ptr<ast_compunit> sysy);
extern void dump_eeyore(std::shared_ptr<ee_program> eeprog, std::ostream &out);
std::shared_ptr<ee_program> eeyore_optim_commonexp(std::shared_ptr<ee_program> oldeeprog);
extern std::shared_ptr<tg_program> tigger_gen(std::shared_ptr<ee_program> eeprog);
extern void dump_tigger(std::shared_ptr<tg_program> tgprog, std::ostream &out);
extern void dump_riscv(std::shared_ptr<tg_program> tgprog, std::ostream &out);

int main(int argc, char **argv) {
  const auto die_args_invalid = [&] () {
    printf("Usage: %s -S [-e/-t] <source.sy> -o <output.eeyore>\n", argv[0]);
    exit(255);
  };
  
  int mode = 2;   // 0: eeyore; 1: tigger; 2: riscv.
  const char *input = NULL, *output = NULL;
  if(argc < 5) die_args_invalid();
  for(int i = 1, nxtoutput = 0; i < argc; ++i) {
    if(argv[i][0] == '-') {
      if(argv[i][2]) die_args_invalid();
      switch(argv[i][1]) {
      case 'S':
        break;
      case 'e':
        mode = 0;
        break;
      case 't':
        mode = 1;
        break;
      case 'o':
        nxtoutput = 1;
        break;
      default:
        die_args_invalid();
      }
    }
    else {
      if(nxtoutput) {
        nxtoutput = 0;
        output = argv[i];
      }
      else {
        input = argv[i];
      }
    }
  }
  if(!output || !input) die_args_invalid();
  // printf("output = %s, input = %s, mode = %d\n", output, input, mode);
  
  std::ofstream fout(output);
  std::shared_ptr<ast_compunit> sysy = read_source_ast(input);
  std::shared_ptr<ee_program> eeyore = eeyore_gen(sysy);
  
  // optimization
  eeyore = eeyore_optim_commonexp(eeyore);
  
  if(mode == 0) { // eeyore
    dump_eeyore(eeyore, fout);
    return 0;
  }
  std::shared_ptr<tg_program> tigger = tigger_gen(eeyore);
  if(mode == 1) {
    dump_tigger(tigger, fout);
  }
  else {
    dump_riscv(tigger, fout);
  }
  return 0;
}
