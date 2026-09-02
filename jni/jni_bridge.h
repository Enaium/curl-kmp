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

#ifndef CURL_KMP_JNI_BRIDGE_H
#define CURL_KMP_JNI_BRIDGE_H

#include <jni.h>
#include <stdint.h>
#include <mutex>
#include <string>
#include <unordered_map>

#include <curl/curl.h>

// JNI entry-point naming macro: every external fun on the Kotlin
// `cn.enaium.curl.Jni` object maps to Java_cn_enaium_curl_Jni_<name>.
#define CURLJNI_FUNC(ret) extern "C" JNIEXPORT ret JNICALL
#define CURLJNI_NAME(name) Java_cn_enaium_curl_Jni_##name

// ---------------------------------------------------------------------------
// Callback bridge (C -> Kotlin)
//
// The curl write/header callbacks need to reach back into the JVM. They
// receive the CURL* handle as userdata (set via CURLOPT_WRITEDATA /
// CURLOPT_HEADERDATA); the bridge resolves the cached JavaVM, attaches the
// current thread if needed and dispatches to the static bridge methods on
// cn.enaium.curl.Jni (onWrite / onHeader), which look the handle up in the
// Kotlin callback maps.
// ---------------------------------------------------------------------------
void curl_kmp_jni_init_callback_bridge(JNIEnv *env);
JNIEnv *curl_kmp_jni_get_env();

// ---------------------------------------------------------------------------
// Per-handle state
// ---------------------------------------------------------------------------

struct CurlKmpHandleState {
    std::string postfields; // owned copy: CURLOPT_POSTFIELDS only references
    struct curl_slist *headers = nullptr;
};

// Guarded by curl_kmp_jni_state_mutex(); keyed by the CURL* handle.
std::unordered_map<CURL *, CurlKmpHandleState> &curl_kmp_jni_states();
std::mutex &curl_kmp_jni_state_mutex();

// ---------------------------------------------------------------------------
// Marshaling helpers
// ---------------------------------------------------------------------------

inline jstring curl_kmp_jni_to_string(JNIEnv *env, const char *s) {
    return s ? env->NewStringUTF(s) : nullptr;
}

#endif // CURL_KMP_JNI_BRIDGE_H