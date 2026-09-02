# curl-kmp

Kotlin Multiplatform bindings for [libcurl](https://github.com/curl/curl) (with the mbedTLS TLS backend), with a curated common API backed by two implementations:

- **JVM**: libcurl is compiled from this repository's `curl`/`mbedtls` submodules (under `includes/`) into a JNI shared library (`libcurl_jni`) that is built by CMake (`jni/`) and shipped as per-OS/arch `curl-kmp-jni-jvm-*` artifacts. `NativeLoader` extracts the matching binary at runtime, so consumers need nothing beyond the normal dependencies (no system libcurl, no OpenSSL).
- **Native (Kotlin/Native)**: the libcurl + mbedTLS static libraries from the submodules are compiled per target with CMake and **embedded into the published klib**, so consumers get a fully self-contained binary (no dynamic libcurl dependency). This includes the Android native targets (`androidNative*`), cross-compiled with the Android NDK.

## Supported platforms

| Platform   | Targets                                             | Implementation                     |
|------------|-----------------------------------------------------|------------------------------------|
| JVM        | `jvm` (Linux/macOS/Windows)                         | JNI shared library (`libcurl_jni`), libcurl + mbedTLS compiled from source |
| macOS      | `macosArm64`, `macosX64`                            | cinterop + embedded static libcurl |
| Linux      | `linuxX64`, `linuxArm64`                             | cinterop + embedded static libcurl |
| Windows    | `mingwX64`                                          | cinterop + embedded static libcurl |
| iOS        | `iosArm64`, `iosX64`, `iosSimulatorArm64`           | cinterop + embedded static libcurl |
| tvOS       | `tvosArm64`, `tvosSimulatorArm64`                   | cinterop + embedded static libcurl |
| Android    | `androidNativeArm64`, `androidNativeArm32`, `androidNativeX64`, `androidNativeX86` | cinterop + embedded static libcurl (built with the NDK) |

Not supported: JavaScript and WebAssembly (no `wasmJs`/`js` target).

## Usage

### 1. Add the dependency

`settings.gradle.kts` — make sure Maven Central is available:

```kotlin
dependencyResolutionManagement {
    repositories {
        mavenCentral()
    }
}
```

`build.gradle.kts` — add the dependency to `commonMain` (it works on every
supported target from a single declaration; the matching JVM native library
and native static library are resolved automatically):

```kotlin
kotlin {
    sourceSets {
        commonMain.dependencies {
            implementation("cn.enaium.curl:curl-kmp:1.0.0")
        }
    }
}
```

Or via the version catalog (`gradle/libs.versions.toml`):

```toml
[versions]
curl-kmp = "1.0.0"

[libraries]
curl-kmp = { module = "cn.enaium.curl:curl-kmp", version.ref = "curl-kmp" }
```

```kotlin
commonMain.dependencies {
    implementation(libs.curl.kmp)
}
```

### 2. Import and use

```kotlin
import cn.enaium.curl.Curl
import cn.enaium.curl.CurlCode

fun main() {
    println("libcurl ${Curl.version()}") // e.g. "libcurl/8.22.0-DEV mbedTLS/3.6.7"

    Curl.easyInit().use { easy ->
        easy.url = "https://example.com"
        easy.caInfo = "/etc/ssl/certs/ca-certificates.crt" // TLS verification needs a CA bundle (see notes)
        easy.followLocation = true
        easy.timeoutMs = 30_000

        val body = StringBuilder()
        easy.writeFunction = { chunk -> body.append(chunk.decodeToString()) }
        easy.headerFunction = { chunk -> print(chunk.decodeToString()) }

        val code = easy.perform()
        if (code.isSuccess) {
            println("HTTP ${easy.responseCode()}, ${body.length} bytes, ${easy.contentType()}")
            println("effective url: ${easy.effectiveUrl()}")
        } else {
            println("request failed: ${Curl.strError(code)}")
        }
    }
}
```

No extra setup is needed on the JVM: the matching `curl-kmp-jni-jvm-{os}-{arch}`
artifact is a transitive runtime dependency and `NativeLoader` loads the
bundled `libcurl_jni` automatically. Native targets embed the static
libcurl/mbedTLS library in the klib, so nothing needs to be installed either.

### API surface

- `Curl` — `globalInit(flags)` / `globalCleanup()`, `version()`, `strError(code)`, `easyInit()`.
- `CurlEasy` (an `AutoCloseable` handle) — request configuration (`url`, `postFields`, `httpHeaders`, `followLocation`, `connectTimeoutMs`, `timeoutMs`, `userAgent`, `acceptEncoding`, `caInfo`, `sslVerifyPeer`), streamed callbacks (`writeFunction`, `headerFunction`), and result accessors (`perform()`, `responseCode()`, `effectiveUrl()`, `contentType()`).
- `CurlCode` — a `CURLcode` wrapper with `isSuccess` and common named constants; `Curl.strError` maps it to a description.
- `CurlGlobalInit` — the `CURL_GLOBAL_*` flag values.

### Platform notes

- **TLS certificate verification**: verification is on by default (`sslVerifyPeer = true`), but mbedTLS has no system CA store. For `https://` you must point `caInfo` at a PEM CA bundle (e.g. `/etc/ssl/certs/ca-certificates.crt` on Linux, `/etc/ssl/cert.pem` on macOS). Set `sslVerifyPeer = false` only for testing.
- **JVM native library**: the matching `curl-kmp-jni-jvm-{os}-{arch}` artifact is a transitive runtime dependency of `curl-kmp`; `NativeLoader` extracts the bundled `libcurl_jni` from the classpath and `System.load()`s it, so no `java.library.path` setup is needed.
- **Kotlin version compatibility**: the published klibs are built with Kotlin 2.4.10. Consuming them with a different Kotlin/Native version produces an `IrLinkageError` at the first curl call. Keep the consumer's Kotlin version in sync.
- **Linux arm64**: the `linuxArm64` target is built on Linux aarch64 hosts or cross-compiled from x86_64 with the `aarch64-linux-gnu` toolchain (`gcc-aarch64-linux-gnu g++-aarch64-linux-gnu`); libcurl/mbedTLS are plain C with no system dependencies, so no multiarch sysroot is required.
- **Windows**: `mingwX64` is cross-compiled on Linux hosts with the `x86_64-w64-mingw32` toolchain (Windows hosts default to MSVC, whose archives are incompatible with Kotlin/Native's MinGW linker).
- **Android**: building an `androidNative*` target requires an installed Android NDK (found under `$ANDROID_HOME/ndk`); the libcurl static library is cross-compiled with its CMake toolchain.
- **macOS JVM**: no special JVM flags are required (unlike GUI frameworks, libcurl is a plain C library).

### Native linking

The libcurl + mbedTLS static libraries (merged into a single `libcurl.a`, plus the cinterop helpers and, on Linux, glibc 2.38+ compatibility shims) are embedded in each target's published klib (built per target by the `curl-kmp/native/CMakeLists.txt` wrapper). The required system libraries are recorded in the cinterop klib as `linkerOpts` (see `curl.def`) and are applied automatically when the consumer's final binary is linked.

## Examples

- **`examples/curl_http`** — a runnable demo of the API: version + error strings, GET with body/header callbacks, POST with custom headers, follow-redirects, and error reporting. Runs on JVM, macOS, Linux and Windows (MinGW):

```bash
# JVM (uses the bundled JNI library)
./gradlew :examples:curl_http:jvmRun

# Native
./gradlew :examples:curl_http:runDebugExecutableMacosArm64
./gradlew :examples:curl_http:runDebugExecutableLinuxX64
```

## Development

```bash
# Publish the library to the local Maven repository first (macOS builds all
# Apple targets + JVM + the darwin JNI artifacts; Linux builds the
# linuxX64/linuxArm64/mingwX64 klibs and the linux-x86_64 JNI artifact;
# Windows (or Linux with the MinGW x86_64-w64-mingw32 toolchain) builds the
# windows-x86_64 JNI artifact).
./gradlew :curl-kmp:publishToMavenLocal
./gradlew :jni-jvm-darwin-aarch64:publishToMavenLocal :jni-jvm-darwin-x86_64:publishToMavenLocal   # macOS
./gradlew :jni-jvm-linux-x86_64:publishToMavenLocal                                                 # Linux
./gradlew :jni-jvm-linux-aarch64:publishToMavenLocal                                               # Linux (aarch64 host or cross)
./gradlew :jni-jvm-windows-x86_64:publishToMavenLocal                                               # Windows (MinGW host)

# Unit + integration tests on the host platform
./gradlew :curl-kmp:jvmTest :curl-kmp:macosArm64Test   # macOS
./gradlew :curl-kmp:jvmTest :curl-kmp:linuxX64Test     # Linux
```

The JVM tests run full HTTP round-trips through the JNI bridge against a local `HttpServer` (no external network); the shared tests exercise the lifecycle and error mapping on every target.

## GitHub Actions

- `.github/workflows/test.yml` — manual trigger: macOS builds all Apple klibs and the `darwin` JNI artifacts and runs JVM + native tests; Linux runs `linuxX64Test`, cross-compiles `linuxArm64`/`mingwX64`, builds the `linux-*` JNI artifacts, and runs the example; Windows builds the `windows-x86_64` JNI artifact natively (MinGW) and runs JVM tests; Android installs the NDK and builds the four `androidNative` klibs.
- `.github/workflows/publish.yml` — manual workflow that publishes the metadata + JVM + Apple klibs and the `curl-kmp-jni-jvm-darwin-*` artifacts from `macos-14`, the `linuxX64`/`linuxArm64`/`mingwX64` klibs and the `curl-kmp-jni-jvm-linux-*` artifacts from `ubuntu-latest`, `curl-kmp-jni-jvm-windows-x86_64` from `windows-latest` (native MinGW build), and the four `androidNative` klibs from `ubuntu-latest` (with the NDK) to Maven Central.

Required secrets: `MAVEN_CENTRAL_USERNAME`, `MAVEN_CENTRAL_PASSWORD`, `SIGNING_KEY` (base64 GPG keyring), `SIGNING_KEY_ID`, `SIGNING_PASSWORD`.

## License

MIT. The bundled libcurl submodule is licensed under the [curl license](https://curl.se/docs/copyright.html) (MIT/X derivate); the bundled mbedTLS submodule is licensed under [Apache-2.0](https://github.com/Mbed-TLS/mbedtls/blob/development/LICENSE).
