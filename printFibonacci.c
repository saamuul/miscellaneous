#include <stdio.h>
#include "mathlib.h"

int main(void){
  int n, result;

  if(scanf("%d", &n) != 1){
    fprintf(stderr, "Error: expected an integer on stdin\n");
    return 1;
  }
  
  result = findFibonacci(n);
  printf("%d\n", result);

  return 0;
}
