# Native game directory layout

The asset packer creates a Windows-friendly game tree under `Game/`:

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

Run `scripts/setup-asset-signing.ps1` from PowerShell after building the native executable. If GitHub CLI is authenticated, the script creates the repository secret itself. Otherwise it copies the private key to the clipboard and opens the exact GitHub secret setup URL. Never commit the private key.

Zlib and OpenSSL are linked from prebuilt vcpkg packages on Windows and system development packages on Linux.
