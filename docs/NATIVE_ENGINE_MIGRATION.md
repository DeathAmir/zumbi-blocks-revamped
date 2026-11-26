# Native engine migration

## Current state

This branch adds a C++20 native-engine bootstrap and an authenticated asset-file container. It does **not** yet replace the Unity game or convert the Unity scenes, prefabs, models, textures, animations, or audio.

The source project is Unity 2020.1.14f1. Its gameplay code is coupled to Unity APIs including `MonoBehaviour`, `CharacterController`, `Physics`, `NavMeshAgent`, `Animator`, `AudioSource`, `ScriptableObject`, and Unity scene/prefab serialization.

## Migration order

1. Inventory and export every source asset and record GUID/reference relationships.
2. Implement the native runtime foundations: window/rendering, input, math, scene graph, resource loading, physics, audio, animation, navigation, and UI.
3. Port player movement/stats/inventory and combat.
4. Port enemy AI, navigation, ragdolls, spawning, waves, and loot.
5. Rebuild menu, HUD, settings, save/configuration, and scene flow.
6. Convert and validate every asset; retain source provenance and licenses.
7. Add multiplayer only after authoritative gameplay simulation and replication are implemented and tested.

## Encrypted asset file

The bootstrap uses AES-256-GCM for individual files. The 32-byte key is supplied as 64 hexadecimal characters. Authentication is verified before decrypted bytes are written. This is a file container primitive, not yet a complete directory/package format or a key-management system.

Never commit production encryption keys. A key embedded in a distributed client can be extracted; encryption is not a substitute for anti-tamper security.

## Build

Requires CMake 3.20+, a C++20 compiler, and OpenSSL development libraries.

```sh
cmake -S NativeEngine -B build
cmake --build build --config Release
```
