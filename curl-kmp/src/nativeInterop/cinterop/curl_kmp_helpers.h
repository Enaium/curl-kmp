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

#ifndef CURL_KMP_HELPERS_H
#define CURL_KMP_HELPERS_H

#include <curl/curl.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Typed wrappers around the variadic curl_easy_setopt/curl_easy_getinfo
 * (Kotlin/Native cinterop cannot bind variadic functions, so the JNI bridge
 * and the native bindings both go through these).
 */

/*
 * `long` is 32-bit on Windows (LLP64) and 64-bit on Unix, so cinterop would
 * expose it as Int on Windows and Long elsewhere. Use int64_t in the bridge
 * signatures (and cast to `long` when calling libcurl) so the Kotlin API is
 * identical on every platform.
 */
#include <stdint.h>

CURLcode curl_kmp_easy_setopt_string(CURL *easy, CURLoption opt, const char *value);
CURLcode curl_kmp_easy_setopt_long(CURL *easy, CURLoption opt, int64_t value);
CURLcode curl_kmp_easy_setopt_pointer(CURL *easy, CURLoption opt, void *value);
CURLcode curl_kmp_easy_setopt_writefunction(CURL *easy, curl_write_callback cb, void *userdata);
CURLcode curl_kmp_easy_setopt_headerfunction(CURL *easy, curl_write_callback cb, void *userdata);
CURLcode curl_kmp_easy_setopt_postfields(CURL *easy, const void *data, size_t len);
CURLcode curl_kmp_easy_getinfo_long(CURL *easy, CURLINFO info, int64_t *value);
CURLcode curl_kmp_easy_getinfo_string(CURL *easy, CURLINFO info, char **value);
CURLcode curl_kmp_global_init(int flags);

/* curl_easy_strerror() without the CURLcode enum dance on the Kotlin side. */
const char *curl_kmp_strerror(int code);

#ifdef __cplusplus
}
#endif

#endif /* CURL_KMP_HELPERS_H */