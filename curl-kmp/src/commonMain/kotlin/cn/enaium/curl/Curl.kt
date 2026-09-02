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

/**
 * A libcurl CURLcode. [isSuccess] is true for CURLE_OK (0); every other value
 * is an error code whose description is available via [Curl.strError].
 */
data class CurlCode(val value: Int) {
    val isSuccess: Boolean get() = value == 0

    companion object {
        val OK = CurlCode(0)
        val UNSUPPORTED_PROTOCOL = CurlCode(1)
        val FAILED_INIT = CurlCode(2)
        val URL_MALFORMAT = CurlCode(3)
        val COULDNT_RESOLVE_PROXY = CurlCode(5)
        val COULDNT_RESOLVE_HOST = CurlCode(6)
        val COULDNT_CONNECT = CurlCode(7)
        val HTTP_RETURNED_ERROR = CurlCode(22)
        val WRITE_ERROR = CurlCode(23)
        val OPERATION_TIMEDOUT = CurlCode(28)
        val SSL_CONNECT_ERROR = CurlCode(35)
        val PEER_FAILED_VERIFICATION = CurlCode(60)

        fun from(value: Int): CurlCode = CurlCode(value)
    }
}

/**
 * Flags for [Curl.globalInit] (values match CURL_GLOBAL_*).
 */
object CurlGlobalInit {
    const val NOTHING = 0
    const val SSL = 1
    const val WIN32 = 2
    const val DEFAULT = 3 // SSL | WIN32
    const val ACK_EINTR = 4
}

/**
 * A libcurl easy handle.
 *
 * The handle is owned by the underlying libcurl instance; call [close] (or
 * [Curl.cleanup] via [AutoCloseable.use]) to release it.
 */
interface CurlEasy : AutoCloseable {
    /** The URL to fetch (e.g. "https://example.com"). */
    var url: String?

    /**
     * Called with each received body chunk (may be empty for non-body
     * responses). When null the body is discarded.
     */
    var writeFunction: ((ByteArray) -> Unit)?

    /** Called with each received header chunk (including the trailing CRLF). */
    var headerFunction: ((ByteArray) -> Unit)?

    /** The bytes to send as the request body (POST). */
    var postFields: ByteArray?

    /** HTTP headers to send, each as "Name: value". */
    var httpHeaders: List<String>?

    /** Follow HTTP redirects (default false). */
    var followLocation: Boolean

    /** Connection timeout in milliseconds (default 0 = no timeout). */
    var connectTimeoutMs: Long

    /** Whole-transfer timeout in milliseconds (default 0 = no timeout). */
    var timeoutMs: Long

    /** The User-Agent header value (default null = libcurl's default). */
    var userAgent: String?

    /** The Accept-Encoding header value ("" enables all built-in encodings). */
    var acceptEncoding: String?

    /** Path to a CA certificate bundle for TLS verification. */
    var caInfo: String?

    /** Verify the TLS peer certificate (default true). */
    var sslVerifyPeer: Boolean

    /** Performs the request; returns the CURLcode of the transfer. */
    fun perform(): CurlCode

    /** The HTTP response code (valid after a successful [perform]). */
    fun responseCode(): Long

    /** The effective URL after redirects (valid after [perform]). */
    fun effectiveUrl(): String?

    /** The response Content-Type (valid after [perform]). */
    fun contentType(): String?

    /** Releases the underlying libcurl handle. */
    override fun close()
}

/**
 * Kotlin Multiplatform bindings for libcurl (with the mbedTLS TLS backend).
 *
 * On the JVM the bindings delegate to a self-contained JNI shared library
 * (libcurl_jni) built from the curl/mbedtls submodules; on native platforms
 * they delegate to the merged static libcurl + mbedTLS archive embedded in
 * the published klib (see the curl.def cinterop file).
 */
expect object Curl {
    /**
     * Initializes libcurl globally. Call once before creating handles (or
     * rely on [easyInit], which initializes with [CurlGlobalInit.DEFAULT]
     * automatically). Must be paired with [globalCleanup].
     */
    fun globalInit(flags: Int = CurlGlobalInit.DEFAULT): CurlCode

    /** Releases libcurl's global state (only after all handles are closed). */
    fun globalCleanup()

    /** libcurl version string (e.g. "libcurl/8.22.0"). */
    fun version(): String

    /** Human-readable description of a [CurlCode]. */
    fun strError(code: CurlCode): String

    /** Creates a new easy handle. */
    fun easyInit(): CurlEasy
}
