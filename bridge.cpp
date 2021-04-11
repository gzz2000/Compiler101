#include <cstdio>

void yyerror(char const *err) {
  printf("Parser error: %s\n", err);
}

int main() {
  printf("Hello, World!\n");
  return 0;
}
