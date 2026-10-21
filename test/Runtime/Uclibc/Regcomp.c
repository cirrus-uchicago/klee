// KDAlloc places a reallocated regcomp buffer far from the old one, which once
// overflowed regex_old.c's pointer delta and failed any longer pattern.
// RUN: %clang %s -emit-llvm -g -c -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --libc=uclibc --exit-on-error %t1.bc

#include <assert.h>
#include <regex.h>

int main() {
  regex_t re;
  assert(regcomp(&re, "^#line ([[:digit:]]+) \"(.*)\"", REG_EXTENDED) == 0);
  assert(regexec(&re, "#line 12 \"a.l\"", 0, 0, 0) == 0);
  regfree(&re);
  return 0;
}
