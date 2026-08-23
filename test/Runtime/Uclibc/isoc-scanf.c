// Modern glibc headers __REDIRECT scanf to __isoc99_scanf or __isoc23_scanf.
// Without a definition for those names the call escapes to the host, which
// concretises stdin and leaves a single path.
// RUN: %clang %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --posix-runtime --libc=uclibc --exit-on-error %t1.bc --sym-stdin 4 >%t.log 2>&1
// RUN: FileCheck --input-file=%t.log %s
// RUN: not grep "undefined reference to function: __isoc" %t.log

#include <stdio.h>

int main() {
  int a = 0;
  if (scanf("%d", &a) != 1)
    return 0;
  if (a == 7)
    // CHECK-DAG: seven
    printf("seven\n");
  else
    // CHECK-DAG: other
    printf("other\n");
  return 0;
}
