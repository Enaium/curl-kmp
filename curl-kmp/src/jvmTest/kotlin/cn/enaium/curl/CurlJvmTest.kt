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

import com.sun.net.httpserver.HttpExchange
import com.sun.net.httpserver.HttpServer
import java.net.InetSocketAddress
import org.junit.jupiter.api.AfterEach
import org.junit.jupiter.api.BeforeEach
import org.junit.jupiter.api.Test
import kotlin.test.assertEquals
import kotlin.test.assertNotNull
import kotlin.test.assertTrue

/**
 * Full HTTP round-trips through the JNI bridge against a local HttpServer
 * (no external network).
 */
class CurlJvmTest {

    private lateinit var server: HttpServer

    @BeforeEach
    fun setUp() {
        server = HttpServer.create(InetSocketAddress("127.0.0.1", 0), 0)
        server.createContext("/hello") { exchange ->
            respond(exchange, 200, "hello world", "text/plain")
        }
        server.createContext("/echo") { exchange ->
            val body = exchange.requestBody.readBytes().decodeToString()
            respond(exchange, 200, "echo:$body", "text/plain")
        }
        server.createContext("/header") { exchange ->
            val header = exchange.requestHeaders.getFirst("X-Test") ?: "missing"
            respond(exchange, 200, "header:$header", "text/plain")
        }
        server.createContext("/method") { exchange ->
            val method = exchange.requestMethod
            respond(exchange, 200, "method:$method", "text/plain")
        }
        server.createContext("/redirect") { exchange ->
            exchange.responseHeaders.add("Location", "/hello")
            exchange.sendResponseHeaders(302, -1)
            exchange.close()
        }
        server.start()
    }

    @AfterEach
    fun tearDown() {
        server.stop(0)
    }

    private fun port(): Int = server.address.port

    private fun respond(exchange: HttpExchange, code: Int, body: String, contentType: String) {
        exchange.responseHeaders.add("Content-Type", contentType)
        val bytes = body.toByteArray()
        exchange.sendResponseHeaders(code, bytes.size.toLong())
        exchange.responseBody.use { it.write(bytes) }
    }

    private fun fetch(
        path: String,
        configure: CurlEasy.() -> Unit = {},
        body: StringBuilder = StringBuilder(),
    ): Pair<CurlEasy, StringBuilder> {
        val easy = Curl.easyInit()
        easy.url = "http://127.0.0.1:${port()}$path"
        easy.writeFunction = { chunk -> body.append(chunk.decodeToString()) }
        easy.configure()
        easy.perform()
        return easy to body
    }

    @Test
    fun getWithCallbacks() {
        fetch("/hello", body = StringBuilder()).first.use { easy ->
            assertEquals(200L, easy.responseCode())
            assertEquals("text/plain", easy.contentType())
            assertTrue(easy.effectiveUrl()!!.endsWith("/hello"))
        }
        val headers = StringBuilder()
        Curl.easyInit().use { easy ->
            easy.url = "http://127.0.0.1:${port()}/hello"
            easy.headerFunction = { chunk -> headers.append(chunk.decodeToString()) }
            assertTrue(easy.perform().isSuccess)
            assertTrue(headers.toString().contains("HTTP/1.1 200"))
        }
    }

    @Test
    fun getBodyIsDelivered() {
        val body = StringBuilder()
        fetch("/hello", body = body).first.use {
            assertEquals("hello world", body.toString())
        }
    }

    @Test
    fun postEcho() {
        val body = StringBuilder()
        fetch("/echo", configure = { postFields = "ping".encodeToByteArray() }, body = body).first.use {
            assertEquals(200L, it.responseCode())
            assertEquals("echo:ping", body.toString())
        }
    }

    @Test
    fun postClearsBackToGet() {
        // Setting postFields switches to POST; clearing it must return to GET.
        val postBody = StringBuilder()
        fetch("/method", configure = { postFields = "x".encodeToByteArray() }, body = postBody).first.use {
            assertEquals("method:POST", postBody.toString())
        }
        val getBody = StringBuilder()
        Curl.easyInit().use { easy ->
            easy.url = "http://127.0.0.1:${port()}/method"
            easy.postFields = "x".encodeToByteArray()
            easy.perform()
            easy.postFields = null
            easy.writeFunction = { chunk -> getBody.append(chunk.decodeToString()) }
            assertTrue(easy.perform().isSuccess)
            assertEquals("method:GET", getBody.toString())
        }
    }

    @Test
    fun customHeaders() {
        val body = StringBuilder()
        fetch("/header", configure = { httpHeaders = listOf("X-Test: abc") }, body = body).first.use {
            assertEquals("header:abc", body.toString())
        }
    }

    @Test
    fun followRedirect() {
        // Without followLocation the 302 is returned as-is.
        Curl.easyInit().use { easy ->
            easy.url = "http://127.0.0.1:${port()}/redirect"
            assertTrue(easy.perform().isSuccess)
            assertEquals(302L, easy.responseCode())
        }
        // With followLocation the redirect is resolved.
        val body = StringBuilder()
        Curl.easyInit().use { easy ->
            easy.url = "http://127.0.0.1:${port()}/redirect"
            easy.followLocation = true
            easy.writeFunction = { chunk -> body.append(chunk.decodeToString()) }

            assertTrue(easy.perform().isSuccess)
            assertEquals(200L, easy.responseCode())
            assertEquals("hello world", body.toString())
            assertNotNull(easy.effectiveUrl())
        }
    }

    @Test
    fun sslVerifyPeerToggle() {
        Curl.easyInit().use { easy ->
            easy.sslVerifyPeer = false
            easy.url = "http://127.0.0.1:${port()}/hello"
            assertTrue(easy.perform().isSuccess)
        }
    }
}