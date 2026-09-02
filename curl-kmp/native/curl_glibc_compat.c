/*
 * Copyright (c) 2026 Enaium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * glibc compatibility shims, Linux only.
 *
 * SDL3 built on a modern distro (glibc >= 2.38) may reference symbols that
 * the Kotlin/Native bundled Linux sysroot (glibc 2.19) does not provide:
 *
 *  - strlcpy/strlcat: added to glibc in 2.38; SDL detects them and calls
 *    them directly (SDL_strlcpy/SDL_strlcat delegate).
 *  - __isoc23_strtol/strtoul/strtoll/strtoull/strtof/strtod/strtold and
 *    __isoc23_fscanf/vfscanf/sscanf/vsscanf: glibc 2.38+ compiles calls to
 *    the C23 semantics variants when _GNU_SOURCE is defined (which SDL
 *    defines in SDL_internal.h).
 *
 * These definitions are only pulled from the archive if referenced, so they
 * are harmless on systems where the real symbols exist.
 */

#define _GNU_SOURCE 1
#include <errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <spawn.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <wchar.h>

/*
 * The __isoc23_* functions are glibc 2.38+ C23-semantics versions of strtol
 * & friends. SDL compiled against a modern glibc references them because
 * glibc redirects the calls at the asm-name level when _GNU_SOURCE is
 * defined, but the Kotlin/Native bundled sysroot (glibc 2.19) does not
 * provide them.
 *
 * That redirect is a compiler attribute (the symbols get renamed, e.g.
 * strtol -> __isoc23_strtol), NOT a preprocessor macro, so #undef does not
 * help: a call to strtol() from inside these wrappers would be redirected
 * straight back into __isoc23_strtol, recursing forever. Instead, call the
 * real libc symbols through explicit asm labels below.
 */
extern long int __sdl_glibc_strtol(const char *nptr, char **endptr, int base) __asm__("strtol");
extern unsigned long int __sdl_glibc_strtoul(const char *nptr, char **endptr, int base) __asm__("strtoul");
extern long long int __sdl_glibc_strtoll(const char *nptr, char **endptr, int base) __asm__("strtoll");
extern unsigned long long int __sdl_glibc_strtoull(const char *nptr, char **endptr, int base) __asm__("strtoull");
extern float __sdl_glibc_strtof(const char *nptr, char **endptr) __asm__("strtof");
extern double __sdl_glibc_strtod(const char *nptr, char **endptr) __asm__("strtod");
extern long double __sdl_glibc_strtold(const char *nptr, char **endptr) __asm__("strtold");
extern int __sdl_glibc_vfscanf(FILE *stream, const char *format, va_list ap) __asm__("vfscanf");
extern int __sdl_glibc_vsscanf(const char *s, const char *format, va_list ap) __asm__("vsscanf");
extern long int __sdl_glibc_wcstol(const wchar_t *nptr, wchar_t **endptr, int base) __asm__("wcstol");
extern unsigned long int __sdl_glibc_wcstoul(const wchar_t *nptr, wchar_t **endptr, int base) __asm__("wcstoul");
extern long long int __sdl_glibc_wcstoll(const wchar_t *nptr, wchar_t **endptr, int base) __asm__("wcstoll");
extern unsigned long long int __sdl_glibc_wcstoull(const wchar_t *nptr, wchar_t **endptr, int base) __asm__("wcstoull");

long int __isoc23_strtol(const char *nptr, char **endptr, int base)
{
    return __sdl_glibc_strtol(nptr, endptr, base);
}
unsigned long int __isoc23_strtoul(const char *nptr, char **endptr, int base)
{
    return __sdl_glibc_strtoul(nptr, endptr, base);
}
long long int __isoc23_strtoll(const char *nptr, char **endptr, int base)
{
    return __sdl_glibc_strtoll(nptr, endptr, base);
}
unsigned long long int __isoc23_strtoull(const char *nptr, char **endptr, int base)
{
    return __sdl_glibc_strtoull(nptr, endptr, base);
}
float __isoc23_strtof(const char *nptr, char **endptr)
{
    return __sdl_glibc_strtof(nptr, endptr);
}
double __isoc23_strtod(const char *nptr, char **endptr)
{
    return __sdl_glibc_strtod(nptr, endptr);
}
long double __isoc23_strtold(const char *nptr, char **endptr)
{
    return __sdl_glibc_strtold(nptr, endptr);
}
int __isoc23_fscanf(FILE *stream, const char *format, ...)
{
    va_list ap;
    int result;
    va_start(ap, format);
    result = __sdl_glibc_vfscanf(stream, format, ap);
    va_end(ap);
    return result;
}
int __isoc23_vfscanf(FILE *stream, const char *format, va_list ap)
{
    return __sdl_glibc_vfscanf(stream, format, ap);
}
int __isoc23_sscanf(const char *s, const char *format, ...)
{
    va_list ap;
    int result;
    va_start(ap, format);
    result = __sdl_glibc_vsscanf(s, format, ap);
    va_end(ap);
    return result;
}
int __isoc23_vsscanf(const char *s, const char *format, va_list ap)
{
    return __sdl_glibc_vsscanf(s, format, ap);
}
long int __isoc23_wcstol(const wchar_t *nptr, wchar_t **endptr, int base)
{
    return __sdl_glibc_wcstol(nptr, endptr, base);
}
unsigned long int __isoc23_wcstoul(const wchar_t *nptr, wchar_t **endptr, int base)
{
    return __sdl_glibc_wcstoul(nptr, endptr, base);
}
long long int __isoc23_wcstoll(const wchar_t *nptr, wchar_t **endptr, int base)
{
    return __sdl_glibc_wcstoll(nptr, endptr, base);
}
unsigned long long int __isoc23_wcstoull(const wchar_t *nptr, wchar_t **endptr, int base)
{
    return __sdl_glibc_wcstoull(nptr, endptr, base);
}

/* BSD-style strlcpy/strlcat (glibc >= 2.38 provides them; older glibc and
 * the Kotlin/Native bundled sysroot do not). */
size_t strlcpy(char *dst, const char *src, size_t size)
{
    size_t slen = strlen(src);
    if (size > 0) {
        size_t n = slen < size - 1 ? slen : size - 1;
        memcpy(dst, src, n);
        dst[n] = '\0';
    }
    return slen;
}

size_t strlcat(char *dst, const char *src, size_t size)
{
    size_t dlen = strnlen(dst, size);
    size_t slen = strlen(src);
    if (dlen < size) {
        size_t n = slen < size - dlen - 1 ? slen : size - dlen - 1;
        memcpy(dst + dlen, src, n);
        dst[dlen + n] = '\0';
    }
    return dlen + slen;
}

size_t wcslcpy(wchar_t *dst, const wchar_t *src, size_t size)
{
    size_t slen = wcslen(src);
    if (size > 0) {
        size_t n = slen < size - 1 ? slen : size - 1;
        wmemcpy(dst, src, n);
        dst[n] = L'\0';
    }
    return slen;
}

size_t wcslcat(wchar_t *dst, const wchar_t *src, size_t size)
{
    size_t dlen = wcsnlen(dst, size);
    size_t slen = wcslen(src);
    if (dlen < size) {
        size_t n = slen < size - dlen - 1 ? slen : size - dlen - 1;
        wmemcpy(dst + dlen, src, n);
        dst[dlen + n] = L'\0';
    }
    return dlen + slen;
}

/* posix_spawn_file_actions_addchdir_np was added to glibc in 2.29. On older
 * glibc (e.g. the Kotlin/Native bundled sysroot) SDL's process API simply
 * cannot honor a working directory; report ENOSYS so SDL degrades like the
 * !HAVE_POSIX_SPAWN_FILE_ACTIONS_ADDCHDIR path (the stub is only pulled
 * from the archive where the real symbol does not exist). */
int posix_spawn_file_actions_addchdir_np(posix_spawn_file_actions_t *actions, const char *path)
{
    (void)actions;
    (void)path;
    return ENOSYS;
}

/* memfd_create(2) was added to glibc in 2.27, but the Kotlin/Native bundled
 * Linux sysroot (glibc 2.19) does not provide it, so SDL's direct calls
 * (DBus portal notifications, Wayland shared-memory buffers) fail to link.
 * Route through the raw syscall (kernel >= 3.17); it is only pulled from the
 * archive where the real symbol does not exist. */
int memfd_create(const char *name, unsigned int flags)
{
    return (int)syscall(SYS_memfd_create, name, flags);
}
