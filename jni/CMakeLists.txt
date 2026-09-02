# Copyright (c) 2026 Enaium
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Builds the JNI shared library (libcurl_jni) for the JVM target. libcurl and
# mbedTLS are compiled statically from the curl/ and mbedtls/ submodules and
# linked in, so the resulting artifact is self-contained (only the OS system
# libraries are referenced at runtime).
cmake_minimum_required(VERSION 3.16)
project(curl-kmp-jni C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED CURL_SOURCE_DIR)
    set(CURL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../includes/curl")
endif()
if(NOT DEFINED MBEDTLS_SOURCE_DIR)
    set(MBEDTLS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../includes/mbedtls")
endif()

# ---------------------------------------------------------------------------
# mbedTLS (static)
# ---------------------------------------------------------------------------
set(USE_SHARED_MBEDTLS_LIBRARY OFF CACHE BOOL "" FORCE)
set(USE_STATIC_MBEDTLS_LIBRARY ON CACHE BOOL "" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)

add_subdirectory(${MBEDTLS_SOURCE_DIR} mbedtls)

# ---------------------------------------------------------------------------
# libcurl (static, mbedTLS backend)
# ---------------------------------------------------------------------------
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

set(CURL_USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_SCHANNEL OFF CACHE BOOL "" FORCE)
set(CURL_USE_WOLFSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_GNUTLS OFF CACHE BOOL "" FORCE)
set(CURL_USE_RUSTLS OFF CACHE BOOL "" FORCE)
set(CURL_USE_BEARSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBRESSL OFF CACHE BOOL "" FORCE)

set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBSSH OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBUV OFF CACHE BOOL "" FORCE)
set(CURL_USE_GSASL OFF CACHE BOOL "" FORCE)
set(CURL_USE_GSSAPI OFF CACHE BOOL "" FORCE)
set(CURL_USE_LDAP OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
set(ENABLE_ARES OFF CACHE BOOL "" FORCE)
set(USE_NGHTTP2 OFF CACHE BOOL "" FORCE)
set(USE_NGTCP2 OFF CACHE BOOL "" FORCE)
set(USE_QUICHE OFF CACHE BOOL "" FORCE)
set(USE_HTTPSRR OFF CACHE BOOL "" FORCE)
set(CURL_USE_PKGCONFIG OFF CACHE BOOL "" FORCE)
set(CURL_USE_CMAKECONFIG OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_TYPECHECK ON CACHE BOOL "" FORCE)
set(PICKY_COMPILER OFF CACHE BOOL "" FORCE)

set(MBEDTLS_INCLUDE_DIR "${MBEDTLS_SOURCE_DIR}/include" CACHE PATH "" FORCE)
set(MBEDTLS_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/mbedtls/library/libmbedtls.a" CACHE FILEPATH "" FORCE)
set(MBEDX509_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/mbedtls/library/libmbedx509.a" CACHE FILEPATH "" FORCE)
set(MBEDCRYPTO_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/mbedtls/library/libmbedcrypto.a" CACHE FILEPATH "" FORCE)
set(MBEDTLS_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)

add_subdirectory(${CURL_SOURCE_DIR} curl)

# ---------------------------------------------------------------------------
# JNI shared library
# ---------------------------------------------------------------------------
add_library(curl_jni SHARED jni_bridge.cpp)
set_target_properties(curl_jni PROPERTIES OUTPUT_NAME curl_jni)

# The static libs are linked into a shared library: they must be PIC (default
# on Apple; required on Linux).
set_target_properties(libcurl_static mbedtls mbedx509 mbedcrypto PROPERTIES
    POSITION_INDEPENDENT_CODE ON)

target_include_directories(curl_jni PRIVATE
    ${CURL_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(JNI_INCLUDE_DIR)
    target_include_directories(curl_jni PRIVATE ${JNI_INCLUDE_DIR})
else()
    find_package(JNI REQUIRED)
    target_include_directories(curl_jni PRIVATE ${JNI_INCLUDE_DIRS})
endif()
if(JNI_INCLUDE_DIR_PLATFORM)
    target_include_directories(curl_jni PRIVATE ${JNI_INCLUDE_DIR_PLATFORM})
endif()

# curl.h on Windows needs CURL_STATICLIB so the static library is imported.
target_compile_definitions(curl_jni PRIVATE CURL_STATICLIB)

target_link_libraries(curl_jni PRIVATE
    libcurl_static
    mbedtls
    mbedx509
    mbedcrypto
)

if(WIN32)
    set_target_properties(curl_jni PROPERTIES PREFIX "")
    # mbedTLS uses BCryptGenRandom for entropy; libcurl uses Winsock.
    target_link_libraries(curl_jni PRIVATE ws2_32 bcrypt)
else()
    target_link_libraries(curl_jni PRIVATE m)
    if(APPLE AND NOT IOS)
        # libcurl's system-proxy detection (CURL_MACOS_CALL_COPYPROXIES).
        target_link_libraries(curl_jni PRIVATE "-framework SystemConfiguration")
    endif()
endif()