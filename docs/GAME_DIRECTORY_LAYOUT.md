# Native game directory layout

The Windows packaging workflow stages the original `Assets/` tree plus the compiled `player_system.zsc`, then creates a Windows-friendly game tree under `Game/`. The package archive is `ZBR Engine.zbp` (ZIP container with signed content blobs):

```text
Game/
  Bin/                  Native executable and runtime DLLs
  ZBR/
    Content/
      Objects/          <sha256>.zba, Zlib-compressed content-addressed blobs
      manifest.zbm      Original relative paths, SHA-256 and uncompressed sizes
      manifest.sig      Ed25519 signature of the complete manifest
    Keys/
      asset_public_key.hex
    Scripts/            Compiled Zumbi Scripts modules (.zsc)
    Shaders/
    Cache/
  Saves/
  Config/
  Logs/
  UserData/
  README.txt
```

Each asset is hashed before compression. Identical bytes share one object file. The manifest maps the original relative asset path to the SHA-256 object name, so the engine can resolve Unity references without retaining the original filename as the physical blob name. `.cs`, `.dll`, and `.pdb` files are excluded from the content pack; Unity `.meta`, prefab, scene, texture, model, audio, font, and shader files are retained.

## Signing

- Algorithm: Ed25519.
- Private key: 32-byte random seed encoded as 64 hex characters, stored only in GitHub Actions secret `ZBR_ASSET_SIGNING_PRIVATE_KEY` and a restricted local user-profile file.
- Public key: committed at `NativeEngine/keys/asset_signing_public_key.hex` and embedded into the native executable at CMake configure time.
- Signature: covers the complete manifest; the manifest covers every asset hash and path.
- Verification: checks the pinned public key, signature, decompression bounds, path traversal, and SHA-256 for every object.

Build locally with `powershell -ExecutionPolicy Bypass -File scripts/build-windows.ps1`. This downloads the pinned precompiled dependencies, configures CMake, builds the x64 executable, copies runtime DLLs, and runs the player/UI/asset tests. Then run `powershell -ExecutionPolicy Bypass -File scripts/setup-asset-signing.ps1` from PowerShell. The script generates the key once under `%USERPROFILE%\\.zbr\\asset-signing-private.hex`, reuses it on later runs, restricts its file ACL, derives the public key, and sends the private key to `gh secret set` through stdin when GitHub CLI is authenticated. If `gh` is unavailable, it prints instructions for adding the local file contents manually. Never commit the private key. The repository public-key file is intentionally blank until the owner runs this setup; the Windows signing job derives the pinned public key from the Actions secret before its final rebuild.

Windows uses pinned precompiled NuGet binaries: OpenSSL 3.5.9 (`openssl-native`) and zlib 1.3.1.1000 (`zlib-windows-x64`). The build copies required runtime DLLs into `Game/Bin` beside `zumbi_native.exe`. Linux uses system OpenSSL and zlib development packages. See `NativeEngine/THIRD_PARTY_NOTICES.md`.
