// __uClibc_main initialises the locale and stdio before it calls the entry
// point, so constructors must run inside the wrapped entry point rather than
// in the wrapper KLEE puts around it.
// RUN: %clang %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --libc=uclibc --exit-on-error %t1.bc 2>&1 | FileCheck %s

#include <ctype.h>
#include <stdio.h>

__attribute__((constructor)) static void probe(void) {
  // CHECK: ctype table ready
  printf("ctype table %s\n", *__ctype_b_loc() ? "ready" : "unset");
}

int main() { return 0; }
