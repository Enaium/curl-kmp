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

package cn.enaium.curl.example

import cn.enaium.curl.Curl
import cn.enaium.curl.CurlCode

/**
 * Demo of the curl-kmp API:
 *
 *  1. version + error strings
 *  2. GET with a write callback (streamed body) and a header callback
 *  3. POST with post fields and custom headers
 *  4. follow-redirects
 *  5. connection failure + strError
 *
 * Pass a URL as the first argument to use something other than the default
 * (http://example.com; note: TLS verification needs [cn.enaium.curl.CurlEasy.caInfo]
 * pointing at a CA bundle when using https).
 */
object CurlHttpDemo {

    fun run(args: Array<String>) {
        val url = args.firstOrNull() ?: "http://example.com"

        println("libcurl version: ${Curl.version()}")

        // 1. GET with callbacks
        println("\n=== GET $url ===")
        Curl.easyInit().use { easy ->
            easy.url = url
            val body = StringBuilder()
            easy.writeFunction = { chunk -> body.append(chunk.decodeToString()) }
            easy.headerFunction = { chunk -> print(chunk.decodeToString()) }
            easy.connectTimeoutMs = 10_000
            easy.timeoutMs = 30_000

            val code = easy.perform()
            println("perform -> ${Curl.strError(code)} (${code.value})")
            println("response code: ${easy.responseCode()}")
            println("content type: ${easy.contentType()}")
            println("effective url: ${easy.effectiveUrl()}")
            println("body (${body.length} chars): ${body.take(120)}")
        }

        // 2. POST with custom headers
        println("\n=== POST $url ===")
        Curl.easyInit().use { easy ->
            easy.url = url
            easy.postFields = "hello=world".encodeToByteArray()
            easy.httpHeaders = listOf("Content-Type: application/x-www-form-urlencoded")
            easy.followLocation = true
            easy.timeoutMs = 30_000

            val code = easy.perform()
            println("perform -> ${Curl.strError(code)} (${code.value})")
            println("response code: ${easy.responseCode()}")
        }

        // 3. Follow redirects (example.com serves a 200 directly; pass a
        // redirecting URL to see it followed).
        println("\n=== FOLLOW REDIRECTS ===")
        Curl.easyInit().use { easy ->
            easy.url = url
            easy.followLocation = true
            easy.timeoutMs = 30_000
            easy.perform()
            println("after follow: response=${easy.responseCode()}, effective=${easy.effectiveUrl()}")
        }

        // 4. Error reporting for an unreachable host.
        println("\n=== ERROR REPORTING ===")
        Curl.easyInit().use { easy ->
            easy.url = "http://127.0.0.1:1/"
            easy.connectTimeoutMs = 2_000
            val code = easy.perform()
            check(code != CurlCode.OK) { "expected a connection error" }
            println("connect to closed port -> ${Curl.strError(code)} (${code.value})")
        }

        println("\ndone.")
    }
}
