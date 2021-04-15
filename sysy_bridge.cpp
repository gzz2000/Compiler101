#include <cstdio>
#include "sysy.hpp"

extern int yyparse(std::shared_ptr<ast_compunit> &root_store);
extern FILE *yyin;

void yyerror(std::shared_ptr<ast_nodebase>, char const *err) {
  printf("Parser error: %s\n", err);
  exit(1);
}

std::shared_ptr<ast_compunit> read_source_ast(const char *fname) {
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
