//===-- isoc-wscanf.c -----------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/* The wide-character half of the scanf redirects described in isoc-scanf.c,
 * kept in its own translation unit so that a program using only the narrow
 * functions does not drag klee-uclibc's wide-character support in with it. */

#include <stdarg.h>
#include <stddef.h>

struct FILE;

int vwscanf(const wchar_t *format, va_list ap);
int vfwscanf(struct FILE *stream, const wchar_t *format, va_list ap);
int vswscanf(const wchar_t *s, const wchar_t *format, va_list ap);

#define KLEE_WSCANF_ALIAS(name, target)                                        \
  int name(const wchar_t *format, ...) {                                       \
    va_list ap;                                                                \
    int result;                                                                \
    va_start(ap, format);                                                      \
    result = target(format, ap);                                               \
    va_end(ap);                                                                \
    return result;                                                             \
  }

#define KLEE_FWSCANF_ALIAS(name, target)                                       \
  int name(struct FILE *stream, const wchar_t *format, ...) {                  \
    va_list ap;                                                                \
    int result;                                                                \
    va_start(ap, format);                                                      \
    result = target(stream, format, ap);                                       \
    va_end(ap);                                                                \
    return result;                                                             \
  }

#define KLEE_SWSCANF_ALIAS(name, target)                                       \
  int name(const wchar_t *s, const wchar_t *format, ...) {                     \
    va_list ap;                                                                \
    int result;                                                                \
    va_start(ap, format);                                                      \
    result = target(s, format, ap);                                            \
    va_end(ap);                                                                \
    return result;                                                             \
  }

KLEE_WSCANF_ALIAS(__isoc99_wscanf, vwscanf)
KLEE_WSCANF_ALIAS(__isoc23_wscanf, vwscanf)
KLEE_FWSCANF_ALIAS(__isoc99_fwscanf, vfwscanf)
KLEE_FWSCANF_ALIAS(__isoc23_fwscanf, vfwscanf)
KLEE_SWSCANF_ALIAS(__isoc99_swscanf, vswscanf)
KLEE_SWSCANF_ALIAS(__isoc23_swscanf, vswscanf)

int __isoc99_vwscanf(const wchar_t *format, va_list ap) {
  return vwscanf(format, ap);
}

int __isoc23_vwscanf(const wchar_t *format, va_list ap) {
  return vwscanf(format, ap);
}

int __isoc99_vfwscanf(struct FILE *stream, const wchar_t *format, va_list ap) {
  return vfwscanf(stream, format, ap);
}

int __isoc23_vfwscanf(struct FILE *stream, const wchar_t *format, va_list ap) {
  return vfwscanf(stream, format, ap);
}

int __isoc99_vswscanf(const wchar_t *s, const wchar_t *format, va_list ap) {
  return vswscanf(s, format, ap);
}

int __isoc23_vswscanf(const wchar_t *s, const wchar_t *format, va_list ap) {
  return vswscanf(s, format, ap);
}
