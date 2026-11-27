# Third-party binary notices

The Windows native build uses pinned precompiled dependency packages:

- OpenSSL 3.5.9, distributed by the NuGet package `openssl-native` 3.5.9. OpenSSL is licensed under Apache License 2.0. Package page: https://www.nuget.org/packages/openssl-native/3.5.9
- zlib 1.3.1, distributed by the NuGet package `zlib-windows-x64` 1.3.1.1000. zlib is distributed under the zlib license. Package page: https://www.nuget.org/packages/zlib-windows-x64/1.3.1.1000

These are precompiled Windows x64 development/runtime dependencies; the build does not compile OpenSSL or zlib from source. The original package license files remain in the build dependency cache and should be retained with any redistribution that requires them.

The native asset package itself uses SHA-256 content addressing, Zlib compression, and an Ed25519 signature. Signing is not encryption.
