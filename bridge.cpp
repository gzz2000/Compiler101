#include <cstdio>
#include "ast.hpp"

extern std::shared_ptr<ast_nodebase> yyparse(void);
extern FILE *yyin;

void yyerror(char const *err) {
  printf("Parser error: %s\n", err);
  exit(1);
}

inline std::shared_ptr<ast_nodebase> read_source_ast(const char *fname) {
  yyin = fopen(fname, "r");
  if(!yyin) {
    perror("Error opening source file");
    exit(1);
  }
  std::shared_ptr<ast_nodebase> ret = yyparse();
  fclose(yyin);
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
