# Native engine migration

## Current state

This branch contains a C++20 native-engine bootstrap, an authenticated single-file asset primitive, a Zumbi Scripts compiler/VM, and CLI-hosted arrays, maps, and run-scoped state APIs. Four gameplay logic slices now have ZS prototypes and automated VM tests: player stats, player movement, enemy combat, and wave progression. This is a partial code migration; it does **not** yet replace the Unity game or convert the Unity scenes, prefabs, models, textures, animations, or audio.

The source inventory and per-script status are tracked in [UNITY_SCRIPT_INVENTORY.md](UNITY_SCRIPT_INVENTORY.md). Prototype scripts are deliberately marked as logic-only until they are integrated with a live native entity, physics, input, UI, and asset runtime.

The source project is Unity 2020.1.14f1. Its gameplay code is coupled to Unity APIs including `MonoBehaviour`, `CharacterController`, `Physics`, `NavMeshAgent`, `Animator`, `AudioSource`, `ScriptableObject`, and Unity scene/prefab serialization.

## Target architecture

- **Native C++ engine:** window/device lifecycle, renderer, input, math, ECS/scene graph, resource streaming, physics, collision, navigation, audio, animation runtime, networking, and performance-critical simulation.
- **Zumbi Scripts (ZS):** UI layouts and behavior, menus, HUD, event handlers, gameplay rules, configuration, and mod-facing logic.
- **ZSC compiler:** parses ZS, validates it, lowers it to ZSA, and eventually emits compact ZSC bytecode.
- **ZSA:** readable low-level assembly/debug representation with three-character per-build function symbols.
- **ZSC:** planned compact bytecode module, with validated opcodes and compression.
- **ZBP:** final encrypted bundle/container. The target main package is `ZBR Engine.zbp`; it will hold compiled ZSC modules, asset index/metadata, and encrypted asset chunks. ZSA listings can be included in development/debug packages.

The native engine exposes a versioned, capability-checked API to scripts. ZS never receives arbitrary native pointers or unrestricted C++ calls. UI rendering is submitted to the native UI subsystem; physics and authoritative simulation remain in C++.

## Migration order

1. Inventory and export every source asset and record GUID/reference relationships.
2. Implement native runtime foundations: window/rendering, input, math, scene graph, resource loading, physics, audio, animation, navigation, and UI.
3. Expand ZS/ZSA/ZSC: nested data collections and run-scoped state are prototyped; next add modules/imports, events, coroutines, typed capability-checked native bindings, validation, and compression hardening.
4. Port player movement/stats/inventory and combat.
5. Port enemy AI, navigation, ragdolls, spawning, waves, and loot.
6. Rebuild menu, HUD, settings, save/configuration, and scene flow in ZS over native UI.
7. Convert and validate every asset; retain source provenance and licenses.
8. Add multiplayer only after authoritative gameplay simulation and replication are implemented and tested.

## Encrypted asset file

The archive primitive uses AES-256-GCM. On Windows it uses the built-in CNG/BCrypt API, so the Windows build does not need OpenSSL, vcpkg, or an extra crypto DLL. Non-Windows builds currently use OpenSSL. Authentication is verified before decrypted bytes are written. This is currently a single-file archive primitive, not yet the final multi-entry ZBP package format or a key-management system.

Never commit production encryption keys. A key embedded in a distributed client can be extracted; encryption is not a substitute for anti-tamper security.

## Build

Windows: CMake 3.20+ and Visual Studio 2022/MSVC; crypto uses the Windows SDK BCrypt library.

Linux: CMake 3.20+, a C++20 compiler, and OpenSSL development libraries.

```sh
cmake -S NativeEngine -B build
cmake --build build --config Release
./build/zumbi_native compile samples/ui/main.zs main.zsa
```
