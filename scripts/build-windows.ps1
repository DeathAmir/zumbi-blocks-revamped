$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Push-Location $root
try {
    New-Item -ItemType Directory -Force build/prebuilt | Out-Null
    if (!(Test-Path build/prebuilt/openssl-native.zip)) {
        Invoke-WebRequest -Uri "https://www.nuget.org/api/v2/package/openssl-native/3.5.9" -OutFile build/prebuilt/openssl-native.zip -MaximumRedirection 10 -TimeoutSec 300
    }
    if (!(Test-Path build/prebuilt/zlib-windows-x64.zip)) {
        Invoke-WebRequest -Uri "https://www.nuget.org/api/v2/package/zlib-windows-x64/1.3.1.1000" -OutFile build/prebuilt/zlib-windows-x64.zip -MaximumRedirection 10 -TimeoutSec 180
    }
    if (!(Test-Path build/prebuilt/openssl/include/openssl/evp.h)) {
        Expand-Archive build/prebuilt/openssl-native.zip build/prebuilt/openssl -Force
    }
    if (!(Get-ChildItem build/prebuilt/zlib -Recurse -File -Filter zlib.h -ErrorAction SilentlyContinue)) {
        Expand-Archive build/prebuilt/zlib-windows-x64.zip build/prebuilt/zlib -Force
    }
    $opensslHeader = Get-ChildItem build/prebuilt/openssl -Recurse -File -Filter evp.h | Where-Object { $_.FullName -match '[\\/]include[\\/]openssl[\\/]evp.h$' } | Select-Object -First 1
    $opensslLibrary = Get-ChildItem build/prebuilt/openssl -Recurse -File -Filter libcrypto.lib | Where-Object { $_.FullName -match '[\\/]lib[\\/]win-x64[\\/]native[\\/]' } | Select-Object -First 1
    $zlibHeader = Get-ChildItem build/prebuilt/zlib -Recurse -File -Filter zlib.h | Select-Object -First 1
    $zlibLibrary = Get-ChildItem build/prebuilt/zlib -Recurse -File -Filter *.lib | Where-Object { $_.Name -match 'zlib' } | Select-Object -First 1
    if (!$opensslHeader -or !$opensslLibrary -or !$zlibHeader -or !$zlibLibrary) {
        throw "Precompiled OpenSSL or zlib headers/libraries are missing."
    }
    $opensslDllDir = Join-Path $root "build/prebuilt/openssl/runtimes/win-x64/native"
    if (!(Test-Path $opensslDllDir)) { throw "OpenSSL x64 runtime DLL directory is missing." }
    cmake -S NativeEngine -B build -A x64 -DCMAKE_BUILD_TYPE=Release -DOPENSSL_INCLUDE_DIR="$($opensslHeader.Directory.Parent.FullName)" -DOPENSSL_CRYPTO_LIBRARY="$($opensslLibrary.FullName)" -DZLIB_INCLUDE_DIR="$($zlibHeader.DirectoryName)" -DZLIB_LIBRARY_RELEASE="$($zlibLibrary.FullName)" -DZLIB_LIBRARY="$($zlibLibrary.FullName)"
    if ($LASTEXITCODE -ne 0) { throw "CMake configure failed." }
    cmake --build build --config Release --parallel
    if ($LASTEXITCODE -ne 0) { throw "Native engine build failed." }
    New-Item -ItemType Directory -Force build/Release | Out-Null
    Get-ChildItem $opensslDllDir -Filter *.dll | Copy-Item -Destination build/Release -Force
    Get-ChildItem build/prebuilt/zlib -Recurse -File -Filter *.dll | Copy-Item -Destination build/Release -Force
    .\build\Release\zumbi_native.exe compile NativeEngine/scripts/player_system.zs NativeEngine/scripts/player_system.zsc
    if ($LASTEXITCODE -ne 0) { throw "Player system ZS compilation failed." }
    .\build\Release\zumbi_native.exe game-selftest
    if ($LASTEXITCODE -ne 0) { throw "Player and UI runtime tests failed." }
    .\build\Release\zumbi_native.exe game-loop NativeEngine/scripts/player_system.zsc 10
    if ($LASTEXITCODE -ne 0) { throw "Ten-frame runtime test failed." }
    .\build\Release\zumbi_native.exe asset-selftest
    if ($LASTEXITCODE -ne 0) { throw "Signed Zlib asset package self-test failed." }
    .\build\Release\zumbi_native.exe selftest
    if ($LASTEXITCODE -ne 0) { throw "Native engine core self-test failed." }
    Write-Host "Windows x64 build and runtime tests passed."
    Write-Host "Next: run scripts/setup-asset-signing.ps1 to provision the private signing key."
}
finally {
    Pop-Location
}
