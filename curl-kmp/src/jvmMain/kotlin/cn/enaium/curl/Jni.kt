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

package cn.enaium.curl

import java.util.concurrent.ConcurrentHashMap

/**
 * JNI bridge for the JVM target.
 *
 * Every `external fun` maps 1:1 to a `Java_cn_enaium_curl_Jni_<name>`
 * function in the C++ sources under `jni/` (see jni_bridge.h for the naming
 * convention). All members are public (no `internal` modifier) so their JVM
 * names are not mangled by the Kotlin compiler.
 *
 * Body/header callbacks are dispatched by id (the handle address) through the
 * static bridge methods [onWrite]/[onHeader], which the C write/header
 * callbacks invoke; the Kotlin side keeps the actual lambdas in
 * [writeCallbacks]/[headerCallbacks].
 */
internal object Jni {

    init {
        NativeLoader.load()
        initCallbackBridge()
    }

    internal val writeCallbacks = ConcurrentHashMap<Long, (ByteArray) -> Unit>()
    internal val headerCallbacks = ConcurrentHashMap<Long, (ByteArray) -> Unit>()

    @JvmStatic
    fun onWrite(id: Long, chunk: ByteArray) {
        writeCallbacks[id]?.invoke(chunk)
    }

    @JvmStatic
    fun onHeader(id: Long, chunk: ByteArray) {
        headerCallbacks[id]?.invoke(chunk)
    }

    external fun initCallbackBridge()

    // =========================================================================
    // Core
    // =========================================================================

    external fun globalInit(flags: Int): Int
    external fun globalCleanup()
    external fun version(): String
    external fun strError(code: Int): String

    // =========================================================================
    // Easy handle
    // =========================================================================

    external fun easyInit(): Long
    external fun easyCleanup(handle: Long)
    external fun easySetUrl(handle: Long, url: String): Int
    external fun easySetWriteFunction(handle: Long): Int
    external fun easySetHeaderFunction(handle: Long): Int
    external fun easySetPostFields(handle: Long, data: ByteArray?): Int
    external fun easySetHttpHeaders(handle: Long, headers: Array<String>?): Int
    external fun easySetFollowLocation(handle: Long, follow: Boolean): Int
    external fun easySetConnectTimeoutMs(handle: Long, ms: Long): Int
    external fun easySetTimeoutMs(handle: Long, ms: Long): Int
    external fun easySetUserAgent(handle: Long, ua: String?): Int
    external fun easySetAcceptEncoding(handle: Long, enc: String?): Int
    external fun easySetCaInfo(handle: Long, path: String?): Int
    external fun easySetSslVerifyPeer(handle: Long, verify: Boolean): Int
    external fun easyPerform(handle: Long): Int
    external fun easyResponseCode(handle: Long): Long
    external fun easyEffectiveUrl(handle: Long): String?
    external fun easyContentType(handle: Long): String?
}
