# Native engine migration

## Current state

This branch contains a C++20 runtime foundation, a ZS compiler/VM, a native player/inventory/UI host API, a SHA-256/Zlib asset packer, and Ed25519 manifest signing.

The connected runtime currently supports persistent player entities, spawn points, player state, movement through a kinematic transform bridge, inventory slots and item records, input injection, retained UI nodes, menu navigation, key rebinding, options values, game events, and repeated ZS VM ticks. The Unity Player0 defaults and 39 default controls are represented in ZS. The source inventory is tracked in [UNITY_SCRIPT_INVENTORY.md](UNITY_SCRIPT_INVENTORY.md), and the current native UI layout reference is [ui_layout.json](../NativeEngine/ui/ui_layout.json).

This is still a staged migration, not a complete Unity replacement. It does not yet have a native renderer or visible UI, a collision solver/CharacterController, a real desktop input backend, camera/head rig, navigation, animation, spatial audio, world item raycasts, gun/melee/usable item behavior, imported Unity scenes/prefabs, or multiplayer. The UI tree is retained runtime data and is not yet drawn on screen.

The source project is Unity 2020.1.14f1 and its original gameplay uses `MonoBehaviour`, `CharacterController`, `Physics`, `NavMeshAgent`, `Animator`, `AudioSource`, `ScriptableObject`, and Unity scene/prefab serialization.

## Target architecture

- **Native C++ engine:** window/device lifecycle, renderer, input, math, ECS/scene graph, resource streaming, physics, collision, navigation, audio, animation runtime, networking, and performance-critical simulation.
- **Zumbi Scripts (ZS):** UI layouts and behavior, menus, HUD, event handlers, gameplay rules, configuration, and mod-facing logic.
- **ZSC compiler/VM:** compiles ZS into binary VM bytecode. The native host registers game APIs for player, inventory, input, physics bridge, UI, options, events, and spawn management.
- **ZSA:** readable stripped disassembly/debug listing. It is not the source language and is not currently a standalone assembler.
- **ZSC:** binary VM module. The source-level function names are stripped and function symbols are randomized.
- **ZBR Engine.zbp:** Windows CI creates a ZIP-based game package containing the native executable, runtime DLLs, content-addressed asset blobs, signed manifest, public verification key, and compiled player module when the signing secret is configured.

## ZS runtime interface

The VM executes `Main()` once per tick. The same `NativeGameRuntime` instance retains entity state, input state, inventory, UI nodes, options and script state across calls. A native application can call `NativeGameRuntime::run(module, error)` each frame; the CLI `game-loop <module.zsc> <frames>` command exercises repeated ticks in CI.

The current movement API is deliberately kinematic: the host updates transforms and receives grounded/ceiling input from an external adapter. It is not a collision solver. The retained UI API stores layout, text, values, visibility, actions and click state, but it is not a renderer.

## Asset packaging and trust

- Each source asset is SHA-256 hashed before compression.
- Physical object names are `<sha256>.zba`; duplicate content is stored once.
- Each object contains a Zlib stream with bounded original/compressed sizes.
- The manifest maps original relative paths to SHA-256 object names and original sizes.
- The manifest is signed using Ed25519. Runtime verification compares the package public key against a public key pinned into the executable at CMake configure time, verifies the signature, validates paths and bounds, decompresses objects, and recomputes SHA-256.
- The private signing seed is never stored in the repository or shipped with the game. Run `scripts/setup-asset-signing.ps1` on the owner's Windows machine after building. It creates/reuses a 32-byte key under the user profile and provisions the GitHub Actions secret through authenticated GitHub CLI. The repository public-key file stays blank until setup; CI derives the public key from the secret and rebuilds the executable with that key pinned.
- The signed content pack is not encrypted. Signing proves integrity/authenticity; Zlib is compression, not encryption. The separate AES-256-GCM single-file archive command is a different feature and is not the ZBR content pack format.

## Migration order

1. Complete the native player manager, loadout, camera, item interaction, and UI event/input integration.
2. Implement a native renderer and retained-UI drawing/input backend, then validate the menu/HUD against the original prefabs.
3. Implement collision/character-controller physics and connect grounded, ceiling, movement, and camera transforms.
4. Port firearms, melee, usable items, zombie AI, navigation, loot and wave orchestration.
5. Import and validate scenes, prefabs, models, textures, audio and GUID/reference relationships.
6. Add save/configuration persistence and multiplayer only after authoritative simulation and replication are implemented and tested.

## Build

Windows: CMake 3.20+, Visual Studio 2022/MSVC, vcpkg packages for OpenSSL and zlib.

Linux: CMake 3.20+, a C++20 compiler, OpenSSL and zlib development packages.

```sh
cmake -S NativeEngine -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/zumbi_native compile NativeEngine/scripts/player_system.zs NativeEngine/scripts/player_system.zsc
./build/zumbi_native game-selftest
./build/zumbi_native game-loop NativeEngine/scripts/player_system.zsc 10
```
