# Player and UI migration notes

The connected runtime now executes `NativeEngine/scripts/player_system.zs` through `NativeGameRuntime`. Its host calls are deliberately limited to engine-facing primitives: entity/component access, transform movement, physical-key state, raycast target handoff, item/world state, retained UI nodes, audio/particle events, and event emission. Player rules, inventory decisions, menu routing, HUD updates, and control remapping are in ZS.

## Player0 data source

Defaults were read from the original `Assets/Prefabs/Characters/Player0.prefab`, not guessed: health 100, armor 100, attack damage 14; walking/sprinting/crouching speeds 6/10/2; standing/crouching heights 2.4/1.2; jump height 1.5; stamina 40 and minimum fraction 0.2 (8 after the original initialization calculation); stamina gains 6/4/-7/5/2; jumping cost 10; smelliness 8; noise 100/400/800/150/800 and reduction factor 1000; bleeding duration 60±10, damage 1, rate 0.5.

The original inventory prefab defines nine slots, primary slot 1, secondary slot 2, melee slot 3, interaction reach 4, sway values 0.01/1/5/0.1/8, drop force (0,0,2), drop spin (10,10,10), and random spin disabled.

## Migrated ZS behavior

- PlayerStats: health/armor damage, healing, stamina gains/caps, noise decay and movement noise, bleeding timers/ticks, death guards and death events.
- PlayerMovement: grounded/ceiling state, camera-yaw-relative movement, sprint continuation threshold, crouch, gravity integration and jump impulse.
- PlayerInventory: nine-slot weapon selection, primary/secondary/melee placement, extra-slot fallback, throwable/placeable/single-use stacking, ammo-box distribution, pickup lifecycle, dropping, and death-drop.
- PlayerHUD: health/stamina/noise bars, weapon/ammo text, hitmarker timer, hurt fade and death screen state.
- UIManager: main/play/map/character/options/credits/controls/pause panels, message timers, stopwatch, 39 default bindings and rebinding flow.

Exact extracted panel geometry and prefab action metadata are tracked in `ui_layout.json`. The original prefab contains a mismatch: the Controls button calls `Menu(9)`, while `UIManager.cs` declares `ControlsMenuPanel = 10`. The native port follows the explicit constant and maps controls to panel 10; this discrepancy needs a final scene-level parity decision.

## Remaining engine work

This runtime is connected and its logic is exercised by the VM test harness, but it is not yet a playable replacement for Unity. Physics currently applies kinematic transform deltas; it does not yet resolve capsule collisions, stairs, slopes, or rigid-body contacts. Input is an adapter interface and is not yet bound to a desktop window. Raycast target selection is supplied through `targetItem` and still needs a real scene query. The retained UI model is not yet rendered by a graphics backend. Weapon, melee, usable-item, camera sway, animation, audio playback, persistence, and scene-loading backends remain to be migrated.
