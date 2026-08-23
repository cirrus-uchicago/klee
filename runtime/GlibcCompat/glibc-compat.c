//===-- glibc-compat.c ----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/* Functions that bitcode compiled against glibc's headers refers to but that
 * klee-uclibc does not define. Each one would otherwise become an external
 * call, concretising its arguments. */

#include <stddef.h>

/* MB_CUR_MAX expands to this call in glibc's stdlib.h; klee-uclibc spells the
 * same quantity _stdlib_mb_cur_max. */
size_t _stdlib_mb_cur_max(void);

size_t __ctype_get_mb_cur_max(void) { return _stdlib_mb_cur_max(); }

int memcmp(const void *s1, const void *s2, size_t n);

int bcmp(const void *s1, const void *s2, size_t n) { return memcmp(s1, s2, n); }

/* klee-uclibc is built without message catalogues, so report what a system
 * with no catalogue installed reports. */
typedef void *nl_catd;

nl_catd catopen(const char *name, int flag) { return (nl_catd)-1L; }

char *catgets(nl_catd catalog, int set, int number, const char *string) {
  return (char *)string;
}

int catclose(nl_catd catalog) { return -1; }
