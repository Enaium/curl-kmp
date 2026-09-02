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

# Builds libcurl as a static library (with the mbedTLS TLS backend, both from
# the curl/ and mbedtls/ submodules) and merges the cinterop helper objects
# into a copy of the resulting archive, so a single -staticLibrary flag in the
# cinterop definition embeds everything into the produced klib.
#
# The merged archive is emitted at CURL_KMP_OUTPUT_DIR/libcurl.a; the Gradle
# build passes -DCURL_KMP_OUTPUT_DIR to point at build/native/<target>.
cmake_minimum_required(VERSION 3.21)
project(curl-kmp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT DEFINED CURL_SOURCE_DIR)
    set(CURL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../includes/curl")
endif()
if(NOT DEFINED MBEDTLS_SOURCE_DIR)
    set(MBEDTLS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../includes/mbedtls")
endif()
if(NOT DEFINED CURL_KMP_OUTPUT_DIR)
    set(CURL_KMP_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/out")
endif()

# ---------------------------------------------------------------------------
# mbedTLS (static only; the TLS backend for libcurl)
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
# libcurl is consumed only through the merged static archive.
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_LIBCURL_DOCS OFF CACHE BOOL "" FORCE)
set(BUILD_MISC_DOCS OFF CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

# mbedTLS is the only TLS backend; force everything else off so no system
# dependency is ever picked up (e.g. OpenSSL on Linux CI).
set(CURL_USE_MBEDTLS ON CACHE BOOL "" FORCE)
set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_SCHANNEL OFF CACHE BOOL "" FORCE)
set(CURL_USE_WOLFSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_GNUTLS OFF CACHE BOOL "" FORCE)
set(CURL_USE_RUSTLS OFF CACHE BOOL "" FORCE)
set(CURL_USE_BEARSSL OFF CACHE BOOL "" FORCE)
set(CURL_USE_LIBRESSL OFF CACHE BOOL "" FORCE)

# Disable every optional dependency so the archive is fully self-contained.
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

# Point curl at the mbedTLS built above. find_path/find_library honour
# pre-set cache variables, so the FindMbedTLS module picks these up directly.
set(MBEDTLS_INCLUDE_DIR "${MBEDTLS_SOURCE_DIR}/include" CACHE PATH "" FORCE)
set(MBEDTLS_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/mbedtls/library/libmbedtls.a" CACHE FILEPATH "" FORCE)
set(MBEDX509_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/mbedtls/library/libmbedx509.a" CACHE FILEPATH "" FORCE)
set(MBEDCRYPTO_LIBRARY "${CMAKE_CURRENT_BINARY_DIR}/mbedtls/library/libmbedcrypto.a" CACHE FILEPATH "" FORCE)
set(MBEDTLS_USE_STATIC_LIBS ON CACHE BOOL "" FORCE)

add_subdirectory(${CURL_SOURCE_DIR} curl)

# ---------------------------------------------------------------------------
# cinterop helpers (merged into the archive as their own static library)
# ---------------------------------------------------------------------------
add_library(curl-kmp-helpers STATIC curl_kmp_helpers.c)
target_include_directories(curl-kmp-helpers PRIVATE
    ${CURL_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/nativeInterop/cinterop)

# glibc 2.38+ compatibility shims (strlcpy/strlcat/__isoc23_*) so the static
# library links against the older glibc bundled with Kotlin/Native.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(curl-kmp-helpers PRIVATE curl_glibc_compat.c)
endif()

# The NDK's clang driver normally links the compiler-rt builtins archive
# (e.g. __aarch64_cas8_acq_rel for 64-bit atomics on arm64-v8a without LSE,
# __aeabi_* on armv7, ...) at the final link, but Kotlin/Native's linker does
# not, and its klib archive ordering is unreliable. Merge the per-ABI builtins
# INTO libcurl.a so the app link (and dlopen) always resolves them.
if(ANDROID_NDK)
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(CURL_KMP_BUILTINS_ARCH aarch64)
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(CURL_KMP_BUILTINS_ARCH arm)
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(CURL_KMP_BUILTINS_ARCH x86_64)
    elseif(ANDROID_ABI STREQUAL "x86")
        set(CURL_KMP_BUILTINS_ARCH i686)
    endif()
    if(DEFINED CURL_KMP_BUILTINS_ARCH)
        file(GLOB CURL_KMP_BUILTINS_ARCHIVE
            "${ANDROID_NDK}/toolchains/llvm/prebuilt/*/lib/clang/*/lib/linux/libclang_rt.builtins-${CURL_KMP_BUILTINS_ARCH}-android.a")
        list(LENGTH CURL_KMP_BUILTINS_ARCHIVE CURL_KMP_BUILTINS_COUNT)
        if(CURL_KMP_BUILTINS_COUNT GREATER 0)
            list(GET CURL_KMP_BUILTINS_ARCHIVE 0 CURL_KMP_BUILTINS_ARCHIVE)
            message(STATUS "Merging compiler-rt builtins: ${CURL_KMP_BUILTINS_ARCHIVE}")
        else()
            message(WARNING
                "compiler-rt builtins archive not found for ${ANDROID_ABI}; "
                "the linked library may fail at dlopen with undefined builtin symbols")
            unset(CURL_KMP_BUILTINS_ARCHIVE)
        endif()
    endif()
endif()

# ---------------------------------------------------------------------------
# Merge everything into a single libcurl.a
# ---------------------------------------------------------------------------
# ar x + per-archive directory + prefixed member names: portable across GNU
# ar (Linux/MinGW), llvm-ar (Android NDK) and Apple ar, and it avoids member
# name collisions between the merged archives.
set(CURL_KMP_MERGED "${CURL_KMP_OUTPUT_DIR}/libcurl.a")
set(CURL_KMP_MERGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/merge")
set(CURL_KMP_ARCHIVES_FILE "${CMAKE_CURRENT_BINARY_DIR}/merge-archives.txt")

# The list of archives is emitted through a generated file rather than a
# ;-separated -D argument: custom commands are rendered into a shell script,
# where unquoted semicolons would split the line into multiple commands.
# The builtins archive (Android) joins the same merge so its members end up
# in the single libcurl.a; the merge script extracts each archive into its
# own directory with prefixed member names, so nothing collides.
set(CURL_KMP_ARCHIVES_CONTENT
    "$<TARGET_FILE:libcurl_static>\n$<TARGET_FILE:mbedtls>\n$<TARGET_FILE:mbedx509>\n$<TARGET_FILE:mbedcrypto>\n$<TARGET_FILE:curl-kmp-helpers>")
if(DEFINED CURL_KMP_BUILTINS_ARCHIVE)
    string(APPEND CURL_KMP_ARCHIVES_CONTENT "\n${CURL_KMP_BUILTINS_ARCHIVE}")
endif()
file(GENERATE OUTPUT ${CURL_KMP_ARCHIVES_FILE} CONTENT "${CURL_KMP_ARCHIVES_CONTENT}")

add_custom_command(
    OUTPUT ${CURL_KMP_MERGED}
    COMMAND ${CMAKE_COMMAND} -DAR=${CMAKE_AR}
        -DOUTPUT=${CURL_KMP_MERGED}
        -DWORK=${CURL_KMP_MERGE_DIR}
        "-DARCHIVES_FILE=${CURL_KMP_ARCHIVES_FILE}"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/merge_archives.cmake
    DEPENDS libcurl_static mbedtls mbedx509 mbedcrypto curl-kmp-helpers
    COMMENT "Merging libcurl + mbedTLS into libcurl.a"
)
add_custom_target(curl-kmp-merged ALL DEPENDS ${CURL_KMP_MERGED})
