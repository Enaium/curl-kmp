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

#include "jni_bridge.h"

// ---------------------------------------------------------------------------
// Callback bridge (C -> Kotlin)
//
// The JVM object cn.enaium.curl.Jni exposes two static bridge methods that
// the curl write/header callbacks dispatch into:
//
//   public static void onWrite(long id, byte[] chunk);
//   public static void onHeader(long id, byte[] chunk);
//
// `id` is the CURL* handle address (the userdata curl passes back).
// ---------------------------------------------------------------------------

static JavaVM *g_vm = nullptr;
static jclass g_jni_class = nullptr;
static jmethodID g_on_write = nullptr;
static jmethodID g_on_header = nullptr;

// Note: the JavaVM is captured in curl_kmp_jni_init_callback_bridge via
// env->GetJavaVM().
JNIEnv *curl_kmp_jni_get_env() {
    JNIEnv *env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6) != JNI_OK) {
        // curl may invoke callbacks from an internal thread (e.g. the
        // threaded resolver); attach it so the JVM calls below work.
        g_vm->AttachCurrentThread(reinterpret_cast<void **>(&env), nullptr);
    }
    return env;
}

void curl_kmp_jni_init_callback_bridge(JNIEnv *env) {
    env->GetJavaVM(&g_vm);
    jclass cls = env->FindClass("cn/enaium/curl/Jni");
    if (!cls) {
        return; // class not yet loadable (tests without the bridge); callbacks stay inert
    }
    g_jni_class = static_cast<jclass>(env->NewGlobalRef(cls));
    env->DeleteLocalRef(cls);
    g_on_write = env->GetStaticMethodID(g_jni_class, "onWrite", "(J[B)V");
    g_on_header = env->GetStaticMethodID(g_jni_class, "onHeader", "(J[B)V");
}

// ---------------------------------------------------------------------------
// Per-handle state
// ---------------------------------------------------------------------------

static std::unordered_map<CURL *, CurlKmpHandleState> g_states;
static std::mutex g_state_mutex;

std::unordered_map<CURL *, CurlKmpHandleState> &curl_kmp_jni_states() {
    return g_states;
}

std::mutex &curl_kmp_jni_state_mutex() {
    return g_state_mutex;
}

// ---------------------------------------------------------------------------
// curl callbacks
// ---------------------------------------------------------------------------

static size_t curl_kmp_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    JNIEnv *env = curl_kmp_jni_get_env();
    if (!env || !g_jni_class || !g_on_write) {
        return 0;
    }
    jlong id = reinterpret_cast<jlong>(userdata);
    jsize len = static_cast<jsize>(size * nmemb);
    jbyteArray chunk = env->NewByteArray(len);
    if (chunk) {
        env->SetByteArrayRegion(chunk, 0, len, reinterpret_cast<const jbyte *>(ptr));
        env->CallStaticVoidMethod(g_jni_class, g_on_write, id, chunk);
        env->DeleteLocalRef(chunk);
    }
    return size * nmemb;
}

static size_t curl_kmp_header_cb(char *ptr, size_t size, size_t nmemb, void *userdata) {
    JNIEnv *env = curl_kmp_jni_get_env();
    if (!env || !g_jni_class || !g_on_header) {
        return 0;
    }
    jlong id = reinterpret_cast<jlong>(userdata);
    jsize len = static_cast<jsize>(size * nmemb);
    jbyteArray chunk = env->NewByteArray(len);
    if (chunk) {
        env->SetByteArrayRegion(chunk, 0, len, reinterpret_cast<const jbyte *>(ptr));
        env->CallStaticVoidMethod(g_jni_class, g_on_header, id, chunk);
        env->DeleteLocalRef(chunk);
    }
    return size * nmemb;
}

// ---------------------------------------------------------------------------
// Core
// ---------------------------------------------------------------------------

CURLJNI_FUNC(void) CURLJNI_NAME(initCallbackBridge)(JNIEnv *env, jobject) {
    curl_kmp_jni_init_callback_bridge(env);
}

CURLJNI_FUNC(jint) CURLJNI_NAME(globalInit)(JNIEnv *, jobject, jint flags) {
    return static_cast<jint>(curl_global_init(static_cast<long>(flags)));
}

CURLJNI_FUNC(void) CURLJNI_NAME(globalCleanup)(JNIEnv *, jobject) {
    curl_global_cleanup();
}

CURLJNI_FUNC(jstring) CURLJNI_NAME(version)(JNIEnv *env, jobject) {
    return env->NewStringUTF(curl_version());
}

CURLJNI_FUNC(jstring) CURLJNI_NAME(strError)(JNIEnv *env, jobject, jint code) {
    return env->NewStringUTF(curl_easy_strerror(static_cast<CURLcode>(code)));
}

// ---------------------------------------------------------------------------
// Easy handle
// ---------------------------------------------------------------------------

CURLJNI_FUNC(jlong) CURLJNI_NAME(easyInit)(JNIEnv *, jobject) {
    return reinterpret_cast<jlong>(curl_easy_init());
}

CURLJNI_FUNC(void) CURLJNI_NAME(easyCleanup)(JNIEnv *, jobject, jlong handle) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    {
        std::lock_guard<std::mutex> lock(curl_kmp_jni_state_mutex());
        auto &states = curl_kmp_jni_states();
        auto it = states.find(easy);
        if (it != states.end()) {
            if (it->second.headers) {
                curl_slist_free_all(it->second.headers);
            }
            states.erase(it);
        }
    }
    curl_easy_cleanup(easy);
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetUrl)(JNIEnv *env, jobject, jlong handle, jstring url) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    const char *cUrl = env->GetStringUTFChars(url, nullptr);
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_URL, cUrl);
    env->ReleaseStringUTFChars(url, cUrl);
    return static_cast<jint>(rc);
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetWriteFunction)(JNIEnv *, jobject, jlong handle) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, curl_kmp_write_cb);
    if (rc != CURLE_OK) {
        return static_cast<jint>(rc);
    }
    // The handle address doubles as the callback id.
    return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_WRITEDATA, easy));
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetHeaderFunction)(JNIEnv *, jobject, jlong handle) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, curl_kmp_header_cb);
    if (rc != CURLE_OK) {
        return static_cast<jint>(rc);
    }
    return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_HEADERDATA, easy));
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetPostFields)(JNIEnv *env, jobject, jlong handle, jbyteArray data) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    {
        std::lock_guard<std::mutex> lock(curl_kmp_jni_state_mutex());
        CurlKmpHandleState &state = curl_kmp_jni_states()[easy];
        if (data) {
            jsize len = env->GetArrayLength(data);
            state.postfields.assign(static_cast<size_t>(len), '\0');
            if (len > 0) {
                env->GetByteArrayRegion(data, 0, len,
                                        reinterpret_cast<jbyte *>(&state.postfields[0]));
            }
        } else {
            state.postfields.clear();
        }
    }
    std::lock_guard<std::mutex> lock(curl_kmp_jni_state_mutex());
    CurlKmpHandleState &state = curl_kmp_jni_states()[easy];
    if (state.postfields.empty()) {
        // Reset the request method back to GET explicitly: curl keeps the
        // POST flag even when CURLOPT_POSTFIELDS is NULL.
        CURLcode rc = curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
        if (rc != CURLE_OK) {
            return static_cast<jint>(rc);
        }
        return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, 0L));
    }
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_POSTFIELDS, state.postfields.data());
    if (rc != CURLE_OK) {
        return static_cast<jint>(rc);
    }
    return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE,
                                              static_cast<long>(state.postfields.size())));
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetHttpHeaders)(JNIEnv *env, jobject, jlong handle, jobjectArray headers) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    {
        std::lock_guard<std::mutex> lock(curl_kmp_jni_state_mutex());
        CurlKmpHandleState &state = curl_kmp_jni_states()[easy];
        if (state.headers) {
            curl_slist_free_all(state.headers);
            state.headers = nullptr;
        }
        if (headers) {
            jsize count = env->GetArrayLength(headers);
            for (jsize i = 0; i < count; i++) {
                jstring jh = static_cast<jstring>(env->GetObjectArrayElement(headers, i));
                const char *cHeader = env->GetStringUTFChars(jh, nullptr);
                state.headers = curl_slist_append(state.headers, cHeader);
                env->ReleaseStringUTFChars(jh, cHeader);
                env->DeleteLocalRef(jh);
            }
        }
        return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_HTTPHEADER, state.headers));
    }
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetFollowLocation)(JNIEnv *, jobject, jlong handle, jboolean follow) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, follow ? 1L : 0L));
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetConnectTimeoutMs)(JNIEnv *, jobject, jlong handle, jlong ms) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, static_cast<long>(ms)));
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetTimeoutMs)(JNIEnv *, jobject, jlong handle, jlong ms) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, static_cast<long>(ms)));
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetUserAgent)(JNIEnv *env, jobject, jlong handle, jstring ua) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    if (!ua) {
        return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_USERAGENT, nullptr));
    }
    const char *cUa = env->GetStringUTFChars(ua, nullptr);
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_USERAGENT, cUa);
    env->ReleaseStringUTFChars(ua, cUa);
    return static_cast<jint>(rc);
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetAcceptEncoding)(JNIEnv *env, jobject, jlong handle, jstring enc) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    if (!enc) {
        return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, nullptr));
    }
    const char *cEnc = env->GetStringUTFChars(enc, nullptr);
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, cEnc);
    env->ReleaseStringUTFChars(enc, cEnc);
    return static_cast<jint>(rc);
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetCaInfo)(JNIEnv *env, jobject, jlong handle, jstring path) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    if (!path) {
        return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_CAINFO, nullptr));
    }
    const char *cPath = env->GetStringUTFChars(path, nullptr);
    CURLcode rc = curl_easy_setopt(easy, CURLOPT_CAINFO, cPath);
    env->ReleaseStringUTFChars(path, cPath);
    return static_cast<jint>(rc);
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easySetSslVerifyPeer)(JNIEnv *, jobject, jlong handle, jboolean verify) {
    CURL *easy = reinterpret_cast<CURL *>(handle);
    return static_cast<jint>(curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, verify ? 1L : 0L));
}

CURLJNI_FUNC(jint) CURLJNI_NAME(easyPerform)(JNIEnv *, jobject, jlong handle) {
    return static_cast<jint>(curl_easy_perform(reinterpret_cast<CURL *>(handle)));
}

CURLJNI_FUNC(jlong) CURLJNI_NAME(easyResponseCode)(JNIEnv *, jobject, jlong handle) {
    long code = 0;
    curl_easy_getinfo(reinterpret_cast<CURL *>(handle), CURLINFO_RESPONSE_CODE, &code);
    return static_cast<jlong>(code);
}

CURLJNI_FUNC(jstring) CURLJNI_NAME(easyEffectiveUrl)(JNIEnv *env, jobject, jlong handle) {
    char *url = nullptr;
    CURLcode rc = curl_easy_getinfo(reinterpret_cast<CURL *>(handle), CURLINFO_EFFECTIVE_URL, &url);
    if (rc != CURLE_OK || !url) {
        return nullptr;
    }
    return env->NewStringUTF(url);
}

CURLJNI_FUNC(jstring) CURLJNI_NAME(easyContentType)(JNIEnv *env, jobject, jlong handle) {
    char *type = nullptr;
    CURLcode rc = curl_easy_getinfo(reinterpret_cast<CURL *>(handle), CURLINFO_CONTENT_TYPE, &type);
    if (rc != CURLE_OK || !type) {
        return nullptr;
    }
    return env->NewStringUTF(type);
}