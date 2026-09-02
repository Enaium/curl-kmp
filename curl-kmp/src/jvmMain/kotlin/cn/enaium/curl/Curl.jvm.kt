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

// =========================================================================
// JVM (JNI) easy handle
// =========================================================================

internal class JvmCurlEasy internal constructor(handle: Long) : CurlEasy {

    private var handle = handle
    private var closed = false

    private fun check(): Long {
        if (closed) throw IllegalStateException("curl easy handle is closed")
        return handle
    }

    private fun checkCode(code: Int) {
        if (code != 0) {
            throw IllegalStateException("curl error ${Curl.strError(CurlCode(code))}")
        }
    }

    override var url: String? = null
        set(value) {
            field = value
            checkCode(Jni.easySetUrl(check(), value ?: ""))
        }

    override var writeFunction: ((ByteArray) -> Unit)? = null
        set(value) {
            field = value
            if (value == null) {
                Jni.writeCallbacks.remove(check())
            } else {
                Jni.writeCallbacks[check()] = value
                checkCode(Jni.easySetWriteFunction(check()))
            }
        }

    override var headerFunction: ((ByteArray) -> Unit)? = null
        set(value) {
            field = value
            if (value == null) {
                Jni.headerCallbacks.remove(check())
            } else {
                Jni.headerCallbacks[check()] = value
                checkCode(Jni.easySetHeaderFunction(check()))
            }
        }

    override var postFields: ByteArray? = null
        set(value) {
            field = value
            checkCode(Jni.easySetPostFields(check(), value))
        }

    override var httpHeaders: List<String>? = null
        set(value) {
            field = value
            checkCode(Jni.easySetHttpHeaders(check(), value?.toTypedArray()))
        }

    override var followLocation: Boolean = false
        set(value) {
            field = value
            checkCode(Jni.easySetFollowLocation(check(), value))
        }

    override var connectTimeoutMs: Long = 0
        set(value) {
            field = value
            checkCode(Jni.easySetConnectTimeoutMs(check(), value))
        }

    override var timeoutMs: Long = 0
        set(value) {
            field = value
            checkCode(Jni.easySetTimeoutMs(check(), value))
        }

    override var userAgent: String? = null
        set(value) {
            field = value
            checkCode(Jni.easySetUserAgent(check(), value))
        }

    override var acceptEncoding: String? = null
        set(value) {
            field = value
            checkCode(Jni.easySetAcceptEncoding(check(), value))
        }

    override var caInfo: String? = null
        set(value) {
            field = value
            checkCode(Jni.easySetCaInfo(check(), value))
        }

    override var sslVerifyPeer: Boolean = true
        set(value) {
            field = value
            checkCode(Jni.easySetSslVerifyPeer(check(), value))
        }

    override fun perform(): CurlCode = CurlCode(Jni.easyPerform(check()))

    override fun responseCode(): Long = Jni.easyResponseCode(check())

    override fun effectiveUrl(): String? = Jni.easyEffectiveUrl(check())

    override fun contentType(): String? = Jni.easyContentType(check())

    override fun close() {
        if (closed) return
        closed = true
        Jni.writeCallbacks.remove(handle)
        Jni.headerCallbacks.remove(handle)
        Jni.easyCleanup(handle)
    }
}

// =========================================================================
// JVM (JNI) implementation
// =========================================================================

actual object Curl {

    actual fun globalInit(flags: Int): CurlCode = CurlCode(Jni.globalInit(flags))

    actual fun globalCleanup() {
        Jni.globalCleanup()
    }

    actual fun version(): String = Jni.version()

    actual fun strError(code: CurlCode): String = Jni.strError(code.value)

    actual fun easyInit(): CurlEasy {
        val handle = Jni.easyInit()
        if (handle == 0L) throw IllegalStateException("curl_easy_init failed")
        return JvmCurlEasy(handle)
    }
}
