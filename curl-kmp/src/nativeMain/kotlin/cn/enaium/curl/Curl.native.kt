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

@file:OptIn(kotlinx.cinterop.ExperimentalForeignApi::class)

package cn.enaium.curl

import kotlinx.cinterop.*
import curl.*

// =========================================================================
// Native (cinterop) easy handle
// =========================================================================

/**
 * Callbacks are dispatched through a global map keyed by the CURL* address:
 * the C write/header callbacks receive the handle pointer as userdata, and
 * look the Kotlin instance up here. Single-threaded usage (the callbacks only
 * fire from the thread that called [perform]) so a plain map is enough.
 */
private val nativeEasies = mutableMapOf<Long, NativeCurlEasy>()

private val nativeWriteCallback: curl_write_callback = staticCFunction {
        ptr: CPointer<ByteVar>?, size: ULong, nmemb: ULong, userdata: COpaquePointer? ->
    val easy = userdata?.rawValue?.toLong()?.let { nativeEasies[it] }
    if (easy != null && ptr != null) {
        val bytes = ptr.readBytes((size * nmemb).toInt())
        easy.writeFunction?.invoke(bytes)
    }
    size * nmemb
}

private val nativeHeaderCallback: curl_write_callback = staticCFunction {
        ptr: CPointer<ByteVar>?, size: ULong, nmemb: ULong, userdata: COpaquePointer? ->
    val easy = userdata?.rawValue?.toLong()?.let { nativeEasies[it] }
    if (easy != null && ptr != null) {
        val bytes = ptr.readBytes((size * nmemb).toInt())
        easy.headerFunction?.invoke(bytes)
    }
    size * nmemb
}

internal class NativeCurlEasy internal constructor(raw: COpaquePointer?) : CurlEasy {

    internal var raw: COpaquePointer? = raw
    private var headerList: CPointer<curl_slist>? = null
    // CURLOPT_POSTFIELDS references the buffer; keep it pinned until the
    // handle closes or the fields are replaced.
    private var pinnedPostFields: Pinned<ByteArray>? = null
    private var closed = false

    private fun check(): COpaquePointer =
        raw ?: throw IllegalStateException("curl easy handle is closed")

    override var url: String? = null
        set(value) {
            field = value
            checkCode(curl_kmp_easy_setopt_string(check(), CURLOPT_URL, value))
        }

    override var writeFunction: ((ByteArray) -> Unit)? = null

    override var headerFunction: ((ByteArray) -> Unit)? = null

    override var postFields: ByteArray? = null
        set(value) {
            field = value
            pinnedPostFields?.unpin()
            pinnedPostFields = null
            if (value == null) {
                checkCode(curl_kmp_easy_setopt_postfields(check(), null, 0uL))
            } else {
                val pinned = value.pin()
                pinnedPostFields = pinned
                checkCode(curl_kmp_easy_setopt_postfields(check(), pinned.addressOf(0), value.size.toULong()))
            }
        }

    override var httpHeaders: List<String>? = null
        set(value) {
            field = value
            headerList?.let { curl_slist_free_all(it) }
            headerList = null
            var list: CPointer<curl_slist>? = null
            value?.forEach { header ->
                list = curl_slist_append(list, header)
            }
            headerList = list
            checkCode(curl_kmp_easy_setopt_pointer(check(), CURLOPT_HTTPHEADER, list))
        }

    override var followLocation: Boolean = false
        set(value) {
            field = value
            checkCode(curl_kmp_easy_setopt_long(check(), CURLOPT_FOLLOWLOCATION, if (value) 1L else 0L))
        }

    override var connectTimeoutMs: Long = 0
        set(value) {
            field = value
            checkCode(curl_kmp_easy_setopt_long(check(), CURLOPT_CONNECTTIMEOUT_MS, value))
        }

    override var timeoutMs: Long = 0
        set(value) {
            field = value
            checkCode(curl_kmp_easy_setopt_long(check(), CURLOPT_TIMEOUT_MS, value))
        }

    override var userAgent: String? = null
        set(value) {
            field = value
            checkCode(curl_kmp_easy_setopt_string(check(), CURLOPT_USERAGENT, value))
        }

    override var acceptEncoding: String? = null
        set(value) {
            field = value
            checkCode(curl_kmp_easy_setopt_string(check(), CURLOPT_ACCEPT_ENCODING, value))
        }

    override var caInfo: String? = null
        set(value) {
            field = value
            checkCode(curl_kmp_easy_setopt_string(check(), CURLOPT_CAINFO, value))
        }

    override var sslVerifyPeer: Boolean = true
        set(value) {
            field = value
            checkCode(curl_kmp_easy_setopt_long(check(), CURLOPT_SSL_VERIFYPEER, if (value) 1L else 0L))
        }

    private fun checkCode(code: UInt) {
        if (code != 0u) {
            throw IllegalStateException("curl error ${Curl.strError(CurlCode(code.toInt()))}")
        }
    }

    override fun perform(): CurlCode = CurlCode(curl_easy_perform(check()).toInt())

    override fun responseCode(): Long = memScoped {
        val out = alloc<LongVar>()
        checkCode(curl_kmp_easy_getinfo_long(check(), CURLINFO_RESPONSE_CODE, out.ptr))
        out.value
    }

    override fun effectiveUrl(): String? = memScoped {
        val out = alloc<CPointerVar<ByteVar>>()
        checkCode(curl_kmp_easy_getinfo_string(check(), CURLINFO_EFFECTIVE_URL, out.ptr))
        out.value?.toKString()
    }

    override fun contentType(): String? = memScoped {
        val out = alloc<CPointerVar<ByteVar>>()
        checkCode(curl_kmp_easy_getinfo_string(check(), CURLINFO_CONTENT_TYPE, out.ptr))
        out.value?.toKString()
    }

    override fun close() {
        if (closed) return
        closed = true
        pinnedPostFields?.unpin()
        pinnedPostFields = null
        raw?.let { handle ->
            headerList?.let { curl_slist_free_all(it) }
            nativeEasies.remove(handle.rawValue.toLong())
            curl_easy_cleanup(handle)
        }
        raw = null
        headerList = null
    }
}

// =========================================================================
// Native (cinterop) implementation
// =========================================================================

actual object Curl {

    actual fun globalInit(flags: Int): CurlCode =
        CurlCode(curl_kmp_global_init(flags).toInt())

    actual fun globalCleanup() {
        curl_global_cleanup()
    }

    actual fun version(): String = curl_version()?.toKString() ?: ""

    actual fun strError(code: CurlCode): String = curl_kmp_strerror(code.value)?.toKString() ?: ""

    actual fun easyInit(): CurlEasy {
        val easy = curl_easy_init() ?: throw IllegalStateException("curl_easy_init failed")
        val wrapped = NativeCurlEasy(easy)
        nativeEasies[easy.rawValue.toLong()] = wrapped
        // Register the static callbacks once per handle; the WRITEFUNCTION /
        // HEADERFUNCTION userdata is the handle pointer itself.
        checkCode(curl_kmp_easy_setopt_writefunction(easy, nativeWriteCallback, easy))
        checkCode(curl_kmp_easy_setopt_headerfunction(easy, nativeHeaderCallback, easy))
        return wrapped
    }

    private fun checkCode(code: UInt) {
        if (code != 0u) {
            throw IllegalStateException("curl error ${strError(CurlCode(code.toInt()))}")
        }
    }
}
