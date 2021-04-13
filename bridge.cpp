#include <cstdio>

extern int yyparse(void);
extern FILE *yyin;

void yyerror(char const *err) {
  printf("Parser error: %s\n", err);
}

// inline void 

int main() {
  printf("Hello, World!\n");
  return 0;
}
