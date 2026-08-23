// Extracting an int from a stream needs both the __isoc23_* parsing symbols
// glibc's headers redirect to and a ctype table that klee-uclibc has already
// initialised by the time libc++'s static constructors run.
// REQUIRES: libcxx
// REQUIRES: uclibc
// REQUIRES: posix-runtime
// RUN: %clangxx %s -emit-llvm %O0opt -c -std=c++11 %libcxx_includes -g -nostdinc++ -o %t1.bc
// RUN: rm -rf %t.klee-out
// RUN: %klee --output-dir=%t.klee-out --posix-runtime --libc=uclibc --libcxx --exit-on-error %t1.bc --sym-stdin 2 >%t.log 2>&1
// RUN: FileCheck --input-file=%t.log %s
// RUN: not grep "undefined reference to function: __isoc" %t.log

#include <iostream>

int main() {
  int x = 0;
  std::cin >> x;
  if (x == 7)
    // CHECK-DAG: seven
    std::cout << "seven\n";
  else
    // CHECK-DAG: other
    std::cout << "other\n";
  return 0;
}
