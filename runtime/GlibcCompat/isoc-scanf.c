//===-- isoc-scanf.c ------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

/* glibc's stdio.h __REDIRECTs the scanf family to a versioned symbol —
 * __isoc99_* for a translation unit that predates C23, __isoc23_* from
 * _GNU_SOURCE or -std=c2x onwards — that klee-uclibc does not define. Without
 * these forwarders the call leaves the interpreter and concretises whatever it
 * parses. */

#include <stdarg.h>

struct FILE;

int vscanf(const char *format, va_list ap);
int vfscanf(struct FILE *stream, const char *format, va_list ap);
int vsscanf(const char *s, const char *format, va_list ap);

#define KLEE_SCANF_ALIAS(name, target)                                         \
  int name(const char *format, ...) {                                          \
    va_list ap;                                                                \
    int result;                                                                \
    va_start(ap, format);                                                      \
    result = target(format, ap);                                               \
    va_end(ap);                                                                \
    return result;                                                             \
  }

#define KLEE_FSCANF_ALIAS(name, target)                                        \
  int name(struct FILE *stream, const char *format, ...) {                     \
    va_list ap;                                                                \
    int result;                                                                \
    va_start(ap, format);                                                      \
    result = target(stream, format, ap);                                       \
    va_end(ap);                                                                \
    return result;                                                             \
  }

#define KLEE_SSCANF_ALIAS(name, target)                                        \
  int name(const char *s, const char *format, ...) {                           \
    va_list ap;                                                                \
    int result;                                                                \
    va_start(ap, format);                                                      \
    result = target(s, format, ap);                                            \
    va_end(ap);                                                                \
    return result;                                                             \
  }

KLEE_SCANF_ALIAS(__isoc99_scanf, vscanf)
KLEE_SCANF_ALIAS(__isoc23_scanf, vscanf)
KLEE_FSCANF_ALIAS(__isoc99_fscanf, vfscanf)
KLEE_FSCANF_ALIAS(__isoc23_fscanf, vfscanf)
KLEE_SSCANF_ALIAS(__isoc99_sscanf, vsscanf)
KLEE_SSCANF_ALIAS(__isoc23_sscanf, vsscanf)

int __isoc99_vscanf(const char *format, va_list ap) {
  return vscanf(format, ap);
}

int __isoc23_vscanf(const char *format, va_list ap) {
  return vscanf(format, ap);
}

int __isoc99_vfscanf(struct FILE *stream, const char *format, va_list ap) {
  return vfscanf(stream, format, ap);
}

int __isoc23_vfscanf(struct FILE *stream, const char *format, va_list ap) {
  return vfscanf(stream, format, ap);
}

int __isoc99_vsscanf(const char *s, const char *format, va_list ap) {
  return vsscanf(s, format, ap);
}

int __isoc23_vsscanf(const char *s, const char *format, va_list ap) {
  return vsscanf(s, format, ap);
}
