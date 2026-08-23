//===-- fortify.c ---------------------------------------------------------===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "klee/klee.h"

#include <stdarg.h>

struct sockaddr;
struct FILE;
struct mbstate_t;
typedef intptr_t off_t;
typedef intptr_t ssize_t;
typedef int socklen_t;

char *fgets(char *str, int n, struct FILE *stream);

char *__fgets_chk(char *s, size_t size, int strsize, struct FILE *stream) {
  return fgets(s, size, stream);
}

size_t mbsrtowcs(wchar_t *dst, const char **src, size_t len,
                 struct mbstate_t *ps);

size_t __mbsrtowcs_chk(wchar_t *dst, const char **src, size_t len,
                       struct mbstate_t *ps, size_t dstlen) {
  if (dst && len > dstlen)
    klee_report_error(__FILE__, __LINE__, "mbsrtowcs overflow", "ptr.err");

  return mbsrtowcs(dst, src, len, ps);
}

ssize_t pread(int fd, void *buf, size_t count,
                     off_t offset);

ssize_t __pread_chk(int fd, void *buf, size_t nbytes, off_t offset,
                    size_t buflen) {
  if (nbytes > buflen)
    klee_report_error(__FILE__, __LINE__, "pread overflow", "ptr.err");

  return pread(fd, buf, nbytes, offset);
}

ssize_t read(int fd, void *buf, size_t count);

ssize_t __read_chk(int fd, void *buf, size_t nbytes, size_t buflen) {
  if (nbytes > buflen)
    klee_report_error(__FILE__, __LINE__, "read overflow", "ptr.err");

  return read(fd, buf, nbytes);
}

ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);

ssize_t __readlink_chk(const char *path, char *buf, size_t len, size_t buflen) {
  if (len > buflen)
    klee_report_error(__FILE__, __LINE__, "readlink overflow", "ptr.err");

  return readlink(path, buf, len);
}

char *realpath(const char *path, char *resolved_path);

// Reference: https://pubs.opengroup.org/onlinepubs/009695399/basedefs/limits.h.html
#ifdef PATH_MAX
#undef PATH_MAX
#endif // PATH_MAX

#define PATH_MAX 4096

char *__realpath_chk(const char *path, char *resolved_path,
                     size_t resolved_len) {
  if (resolved_len < PATH_MAX)
    klee_report_error(__FILE__, __LINE__, "realpath overflow", "ptr.err");

  return realpath(path, resolved_path);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags);

ssize_t __recv_chk(int fd, void *buf, size_t len, size_t buflen, int flag) {
  if (len > buflen)
    klee_report_error(__FILE__, __LINE__, "recv overflow", "ptr.err");

  return recv(fd, buf, len, flag);
}

ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                 struct sockaddr *src_addr, socklen_t *addrlen);

ssize_t __recvfrom_chk(int fd, void *buf, size_t len, size_t buflen, int flag,
                       struct sockaddr *from, socklen_t *fromlen) {
  if (len > buflen)
    klee_report_error(__FILE__, __LINE__, "recvfrom overflow", "ptr.err");

  return recvfrom(fd, buf, len, flag, from, fromlen);
}

char *stpncpy(char *dst, const char *src, size_t dsize);

char *__stpncpy_chk(char *dest, const char *src, size_t n, size_t destlen) {
  if (n > destlen)
    klee_report_error(__FILE__, __LINE__, "stpncpy overflow", "ptr.err");
  return stpncpy(dest, src, n);
}

char *strncat(char *destination, const char *source, size_t num);

char *__strncat_chk(char *s1, const char *s2, size_t n, size_t s1len) {
  return strncat(s1, s2, n);
}

int ttyname_r(int fd, char *buf, size_t buflen);

int __ttyname_r_chk(int fd, char *buf, size_t buflen, size_t nreal) {
  if (buflen > nreal)
    klee_report_error(__FILE__, __LINE__, "ttyname_r overflow", "ptr.err");
  return ttyname_r(fd, buf, nreal);
}

int vasprintf(char **strp, const char *format, va_list ap);

int __vasprintf_chk(char **strp, int flag, const char *format, va_list ap) {
  return vasprintf(strp, format, ap);
}

int vfprintf(struct FILE *stream, const char *format, va_list ap);

int __vfprintf_chk(struct FILE *stream, int flag, const char *format,
                   va_list ap) {
  return vfprintf(stream, format, ap);
}

int vsnprintf(char *str, size_t size, const char *format, va_list ap);

int __vsnprintf_chk(char *str, size_t maxlen, int flag, size_t slen,
                    const char *format, va_list ap) {
  if (maxlen > slen)
    klee_report_error(__FILE__, __LINE__, "vsnprintf overflow", "ptr.err");
  return vsnprintf(str, maxlen, format, ap);
}
