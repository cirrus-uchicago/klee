//===-- isoc-strtol.c -----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/* C23 gave the strtol family a 0b prefix, so glibc's stdlib.h and inttypes.h
 * __REDIRECT them to __isoc23_* whenever _GNU_SOURCE or -std=c2x is in effect.
 * klee-uclibc predates that and only defines the unversioned names. */

typedef void *locale_t;

long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
long long strtoll(const char *nptr, char **endptr, int base);
unsigned long long strtoull(const char *nptr, char **endptr, int base);
long strtol_l(const char *nptr, char **endptr, int base, locale_t loc);
unsigned long strtoul_l(const char *nptr, char **endptr, int base,
                        locale_t loc);
long long strtoll_l(const char *nptr, char **endptr, int base, locale_t loc);
unsigned long long strtoull_l(const char *nptr, char **endptr, int base,
                              locale_t loc);
long strtoimax(const char *nptr, char **endptr, int base);
unsigned long strtoumax(const char *nptr, char **endptr, int base);

#define KLEE_STRTO_ALIAS(type, name, target)                                   \
  type name(const char *nptr, char **endptr, int base) {                       \
    return target(nptr, endptr, base);                                         \
  }

#define KLEE_STRTO_L_ALIAS(type, name, target)                                 \
  type name(const char *nptr, char **endptr, int base, locale_t loc) {         \
    return target(nptr, endptr, base, loc);                                    \
  }

KLEE_STRTO_ALIAS(long, __isoc23_strtol, strtol)
KLEE_STRTO_ALIAS(unsigned long, __isoc23_strtoul, strtoul)
KLEE_STRTO_ALIAS(long long, __isoc23_strtoll, strtoll)
KLEE_STRTO_ALIAS(unsigned long long, __isoc23_strtoull, strtoull)
KLEE_STRTO_L_ALIAS(long, __isoc23_strtol_l, strtol_l)
KLEE_STRTO_L_ALIAS(unsigned long, __isoc23_strtoul_l, strtoul_l)
KLEE_STRTO_L_ALIAS(long long, __isoc23_strtoll_l, strtoll_l)
KLEE_STRTO_L_ALIAS(unsigned long long, __isoc23_strtoull_l, strtoull_l)
KLEE_STRTO_ALIAS(long, __isoc23_strtoimax, strtoimax)
KLEE_STRTO_ALIAS(unsigned long, __isoc23_strtoumax, strtoumax)
