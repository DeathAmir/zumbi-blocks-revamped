# Unity gameplay migration inventory

Source baseline: Unity 2020.1.14f1, 46 C# scripts under Assets/Scripts and 2 scripts under Standard Assets. This inventory tracks the original source; a script is not considered migrated merely because a similarly named ZS sample exists.

## Migration status

| Unity source | Responsibility | Status |
|---|---|---|
| AmbientEntity.cs | Ambient entity lifecycle | Pending |
| AmbientEntityLight.cs | Ambient lighting behavior | Pending |
| AudioManager.cs | Audio routing and playback | Pending |
| Control.cs | Input control data | ZS binding data (runtime model) |
| Controllers/EnemyMovement.cs | Zombie steering, target selection, navigation | Pending |
| Controllers/MouseController.cs | Mouse look and weapon aim | Pending |
| Controllers/SpectatorMovement.cs | Spectator camera movement | Pending |
| ControlsCollection.cs | Control presets | ZS defaults and 39-entry control map |
| ControlsManager.cs | Bindings and input configuration | ZS runtime port (39 defaults and rebind capture); OS input adapter pending |
| Destructible.cs | Damageable world objects | Pending |
| EnemyInventory.cs | Zombie equipment and item use | Pending |
| EnemySpawn.cs | Spawn point behavior | Pending |
| EnemyStats.cs | Zombie health, armor, attacks, death and ragdoll state | Logic prototype |
| Explosive.cs | Explosive weapons and damage | Pending |
| GameManager.cs | Scene flow, pause, game clock, game over | Pending |
| Guns.cs | Firearms, reload, recoil, ammunition and firing modes | Pending |
| ImpactEffectCollection.cs | Surface impact effect lookup | Pending |
| Item.cs | Item metadata and interaction | Pending |
| ItemCollection.cs | Item definitions | Pending |
| LayerMaskCollisions.cs | Collision layer masks | Pending |
| LightItem.cs | Portable light item | Pending |
| Loot.cs | Loot data | Pending |
| LootSpawn.cs | Loot spawn point | Pending |
| LootSpawner.cs | Loot distribution | Pending |
| Melee.cs | Melee weapon behavior | Pending |
| NavMeshManager.cs | Navigation mesh lifecycle | Pending |
| Options.cs | Options data | Pending |
| OptionsManager.cs | Settings persistence and application | Pending |
| PhysicMaterialCollection.cs | Physics material lookup | Pending |
| PlayerHUD.cs | Player HUD and feedback | ZS retained-UI logic port |
| PlayerInventory.cs | Inventory slots, pickup, drop, equipment | ZS runtime port (9 slots, selection, pickup, stacking, ammo and drop state); camera/physics presentation pending |
| PlayerManager.cs | Player lifecycle and player list | Native runtime player registry, spawn selection and death tally; prefab/loadout integration pending |
| PlayerMovement.cs | Movement, sprint, crouch, jump and falling | ZS runtime port (kinematic host bridge) |
| PlayerSpawn.cs | Player spawn configuration | Native runtime spawn-point transforms; scene spawn-point import pending |
| PlayerStats.cs | Health, armor, stamina, noise, bleeding and death | ZS runtime port (Player0 defaults) |
| ScriptableObjectManager.cs | Shared data registry | Pending |
| Sound.cs | Sound data | Pending |
| TestMusicPlayer.cs | Test music playback | Pending |
| UIManager.cs | Menus, panels, messages and scene UI | ZS logic port (menus, credits, controls, options, pause, messages, stopwatch); renderer pending |
| UsableItem.cs | Healing, consumables and usable equipment | Pending |
| WaveSpawner.cs | Wave timing, selection and spawn orchestration | Logic prototype |
| ZombieTextureCollection.cs | Zombie texture lookup | Pending |
| ZombieTextures.cs | Zombie texture data | Pending |
| ZombieType.cs | Zombie archetype data | Pending |
| Standard Assets/.../MouseLook.cs | Legacy mouse look helper | Pending |
| Standard Assets/.../WaterSimple.cs | Legacy water shader behavior | Pending |

## Implemented native-side logic prototypes

- `NativeEngine/tests/player_stats.zs`: armor-adjusted damage, healing cap, stamina tick and noise decay.
- `NativeEngine/tests/enemy_combat.zs`: armor-adjusted enemy damage and attack cooldown helpers.
- `NativeEngine/tests/player_movement.zs`: sprint/crouch speed selection and jump velocity.
- `NativeEngine/tests/wave_director.zs`: kill threshold, wave completion, wave numbering and countdown.

PlayerStats, PlayerMovement, PlayerInventory, PlayerHUD, ControlsManager, PlayerManager, PlayerSpawn and UIManager now have partial ZS/native-runtime ports. The Player0 defaults and 39 default control bindings are represented in ZS. NativeGameRuntime owns persistent player entities, inventory/item records, spawn-point transforms, input injection, options state, retained UI nodes, event dispatch and per-frame ZS execution. Automated runtime tests cover movement, damage/healing/stamina, inventory pickup/stacking/ammo/drop, menu navigation, key rebinding and options values.

This is not yet full visual or physical parity with Unity. The physics bridge is kinematic and still lacks a collision solver, CharacterController capsule, camera/head rig and actual OS input adapter. The retained UI tree is not yet drawn by a renderer. World item raycasts, weapon/melee/usable-item behaviors, item prefab presentation, PlayerManager loadout cloning, imported scene spawn points, spatial audio playback and animation remain pending. The source C# components remain as behavioral references until full scene and interaction parity is verified.

## Required next integrations

1. Expand the current run-scoped state and array/map support into persistent, serializable entity/component storage.
2. Replace the current string-dispatched host calls with typed, versioned native API registration for entities, transforms, input, physics, navigation, audio, animation, UI, inventory, items and spawning.
3. Port the remaining gameplay methods, preserving serialized defaults and prefab references; the firearm/melee/usable item and player manager paths are still pending.
4. Implement the retained UI renderer and desktop input backend, then import and validate scene/prefab/asset references; do not discard Unity GUIDs before the replacement asset database is complete.
5. Add integration tests that compare the original C# behavior with the native runtime on the same fixtures.
