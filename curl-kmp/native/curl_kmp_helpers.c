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

#include "curl_kmp_helpers.h"

CURLcode curl_kmp_easy_setopt_string(CURL *easy, CURLoption opt, const char *value) {
    return curl_easy_setopt(easy, opt, value);
}

CURLcode curl_kmp_easy_setopt_long(CURL *easy, CURLoption opt, int64_t value) {
    return curl_easy_setopt(easy, opt, (long)value);
}

CURLcode curl_kmp_easy_setopt_pointer(CURL *easy, CURLoption opt, void *value) {
    return curl_easy_setopt(easy, opt, value);
}

CURLcode curl_kmp_easy_setopt_writefunction(CURL *easy, curl_write_callback cb, void *userdata) {
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, cb);
    if (rc != CURLE_OK) {
        return rc;
    }
    return curl_easy_setopt(easy, CURLOPT_WRITEDATA, userdata);
}

CURLcode curl_kmp_easy_setopt_headerfunction(CURL *easy, curl_write_callback cb, void *userdata) {
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, cb);
    if (rc != CURLE_OK) {
        return rc;
    }
    return curl_easy_setopt(easy, CURLOPT_HEADERDATA, userdata);
}

/* CURLOPT_POSTFIELDS only references the buffer, which must stay valid until
 * the transfer runs. The native side keeps the ByteArray pinned; the JNI
 * bridge keeps an owned copy per handle. A NULL data resets the method back
 * to GET explicitly (curl keeps the POST flag otherwise). */
CURLcode curl_kmp_easy_setopt_postfields(CURL *easy, const void *data, size_t len) {
    if (!data) {
        CURLcode rc = curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
        if (rc != CURLE_OK) {
            return rc;
        }
        return curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, 0L);
    }
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_POSTFIELDS, data);
    if (rc != CURLE_OK) {
        return rc;
    }
    return curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)len);
}

CURLcode curl_kmp_easy_getinfo_long(CURL *easy, CURLINFO info, int64_t *value) {
    long tmp = 0;
    CURLcode rc = curl_easy_getinfo(easy, info, &tmp);
    if (rc == CURLE_OK) {
        *value = (int64_t)tmp;
    }
    return rc;
}

CURLcode curl_kmp_global_init(int flags) {
    return curl_global_init((long)flags);
}

CURLcode curl_kmp_easy_getinfo_string(CURL *easy, CURLINFO info, char **value) {
    return curl_easy_getinfo(easy, info, value);
}

const char *curl_kmp_strerror(int code) {
    return curl_easy_strerror((CURLcode)code);
}