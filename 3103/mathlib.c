#include "mathlib.h"

int findFibonacci(int n){
  if(n<=1)
    return n;
  return findFibonacci(n-1) + findFibonacci(n-2);
}

int add1K(int n){
  return n+1000;
}
