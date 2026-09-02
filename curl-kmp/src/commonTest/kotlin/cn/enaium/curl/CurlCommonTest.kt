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

import kotlin.test.Test
import kotlin.test.assertEquals
import kotlin.test.assertFalse
import kotlin.test.assertTrue

/**
 * Offline lifecycle tests that run on every target (JVM and native): they
 * never touch the network. The real HTTP round-trip is covered by
 * CurlJvmTest (JVM, local HttpServer).
 */
class CurlCommonTest {

    @Test
    fun versionIsNotEmpty() {
        assertTrue(Curl.version().isNotBlank())
    }

    @Test
    fun globalInitAndCleanup() {
        assertTrue(Curl.globalInit().isSuccess)
        Curl.globalCleanup()
    }

    @Test
    fun strErrorOfOk() {
        assertEquals("No error", Curl.strError(CurlCode.OK))
    }

    @Test
    fun easyLifecycle() {
        Curl.easyInit().close()
    }

    @Test
    fun performToClosedPortFails() {
        Curl.easyInit().use { easy ->
            easy.url = "http://127.0.0.1:1/"
            easy.connectTimeoutMs = 1000
            val code = easy.perform()
            assertFalse(code.isSuccess)
        }
    }
}
