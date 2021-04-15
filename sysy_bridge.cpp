#include <cstdio>
#include "sysy.hpp"

extern int yyparse(std::shared_ptr<ast_compunit> &root_store);
extern FILE *yyin;

void yyerror(std::shared_ptr<ast_nodebase>, char const *err) {
  printf("Parser error: %s\n", err);
  exit(1);
}

inline std::shared_ptr<ast_nodebase> read_source_ast(const char *fname) {
  yyin = fopen(fname, "r");
  if(!yyin) {
    perror("Error opening source file");
    exit(1);
  }
  std::shared_ptr<ast_compunit> ret;
  int status = yyparse(ret);
  fclose(yyin);
  if(status) {
    perror("Syntax error returned from yyparse().");
  }
  return ret;
}

int main(int argc, char **argv) {
  if(argc != 2) {
    printf("Usage: %s <source_file>\n", argv[0]);
    return 255;
  }
  auto ret = read_source_ast(argv[1]);
  printf("Hello, World!\n");
  return 0;
}
