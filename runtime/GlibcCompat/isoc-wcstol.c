//===-- isoc-wcstol.c -----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/* The wide-character half of the strtol redirects described in isoc-strtol.c,
 * kept in its own translation unit so that a program using only the narrow
 * functions does not drag klee-uclibc's wide-character support in with it. */

#include <stddef.h>

typedef void *locale_t;

long wcstol(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long wcstoul(const wchar_t *nptr, wchar_t **endptr, int base);
long long wcstoll(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long long wcstoull(const wchar_t *nptr, wchar_t **endptr, int base);
long wcstol_l(const wchar_t *nptr, wchar_t **endptr, int base, locale_t loc);
unsigned long wcstoul_l(const wchar_t *nptr, wchar_t **endptr, int base,
                        locale_t loc);
long long wcstoll_l(const wchar_t *nptr, wchar_t **endptr, int base,
                    locale_t loc);
unsigned long long wcstoull_l(const wchar_t *nptr, wchar_t **endptr, int base,
                              locale_t loc);
long wcstoimax(const wchar_t *nptr, wchar_t **endptr, int base);
unsigned long wcstoumax(const wchar_t *nptr, wchar_t **endptr, int base);

#define KLEE_WCSTO_ALIAS(type, name, target)                                   \
  type name(const wchar_t *nptr, wchar_t **endptr, int base) {                 \
    return target(nptr, endptr, base);                                         \
  }

#define KLEE_WCSTO_L_ALIAS(type, name, target)                                 \
  type name(const wchar_t *nptr, wchar_t **endptr, int base, locale_t loc) {   \
    return target(nptr, endptr, base, loc);                                    \
  }

KLEE_WCSTO_ALIAS(long, __isoc23_wcstol, wcstol)
KLEE_WCSTO_ALIAS(unsigned long, __isoc23_wcstoul, wcstoul)
KLEE_WCSTO_ALIAS(long long, __isoc23_wcstoll, wcstoll)
KLEE_WCSTO_ALIAS(unsigned long long, __isoc23_wcstoull, wcstoull)
KLEE_WCSTO_L_ALIAS(long, __isoc23_wcstol_l, wcstol_l)
KLEE_WCSTO_L_ALIAS(unsigned long, __isoc23_wcstoul_l, wcstoul_l)
KLEE_WCSTO_L_ALIAS(long long, __isoc23_wcstoll_l, wcstoll_l)
KLEE_WCSTO_L_ALIAS(unsigned long long, __isoc23_wcstoull_l, wcstoull_l)
KLEE_WCSTO_ALIAS(long, __isoc23_wcstoimax, wcstoimax)
KLEE_WCSTO_ALIAS(unsigned long, __isoc23_wcstoumax, wcstoumax)
