# Zumbi Scripts

Zumbi Scripts (ZS) is the game-facing high-level language for ZBR Engine. It is a custom language and does not embed or depend on Lua.

## Runtime boundary

The C++ engine owns rendering, physics, collision, input devices, audio backends, resource streaming, navigation primitives, networking transport, memory management, and low-level platform services.

Game behavior is authored in ZS: player and enemy state machines, weapons, inventory, waves, missions, interaction rules, menus, HUD, UI behavior, progression, save-game rules, and event responses. ZS reaches engine features through a registered native API; scripts do not receive arbitrary C++ pointers or unrestricted native calls.

## Pipeline

- `.zs`: editable high-level source.
- ZSC compiler: lexer, parser, semantic checks, bytecode generation, and per-build three-character function symbols.
- `.zsc`: binary VM bytecode module with size-checked RLE compression when it reduces the payload. It contains constants, local slots, function metadata, and VM instructions; source-level function names are not stored in the runtime function table.
- ZSA: readable stripped assembler/debug listing generated with `zumbi_native disasm module.zsc module.zsa`. It shows VM instructions and randomized function symbols, not the original source function names. It is not shipped in release bundles.
- ZSC VM: bounded stack interpreter with function calls, local variables, arithmetic, comparisons, branches, loops, native API calls, and a per-run instruction budget.
- `ZBR Engine.zbp`: current Windows package is a ZIP container with SHA-256-addressed Zlib-compressed content objects, an Ed25519-signed manifest, a pinned public key, the native executable/runtime DLLs, and the compiled player module. This package is signed and compressed, not encrypted. The separate AES-256-GCM file archive is a different command.

The current ZSC format is binary, not source text. The current implementation does not claim that bytecode is impossible to reverse engineer. Native code, symbols, and runtime behavior can always be analyzed; encrypted ZBP distribution, randomized symbols, stripped debug metadata, and later opcode obfuscation raise the cost but are not an absolute guarantee.

## Current ZS syntax

```zs
fn Main() {
    let health = 100;
    let wave = 1;
    let damage = calculateDamage(45, 12);

    while (wave < 4) {
        log("Wave", wave);
        wave = wave + 1;
    }

    if (health > 0 && damage == 33) {
        log("Player can continue");
    } else {
        log("Player is down");
    }
}

fn calculateDamage(base, armor) {
    let result = base - armor;
    if (result < 0) {
        return 0;
    }
    return result;
}
```

Implemented language features:

- Functions and parameters; functions may call other functions.
- `let` locals and reassignment.
- `return`.
- `if / else` and `while`.
- Number, string, boolean, and null literals.
- `+`, `-`, `*`, `/`, `%`, unary `-` and `!`.
- Comparisons and short-circuit `&&` / `||`.
- Native engine function calls such as `physics.setVelocity(...)`, `player.setHealth(...)`, `zombie.spawn(...)`, and `ui.text(...)` once those APIs are registered by the engine runtime.

The standalone `run` CLI host registers `log`, `print`, `sqrt`, arrays, maps and run-scoped state. The connected `NativeGameRuntime` additionally registers these game APIs:

- Player: `player.create/destroy/get/set/position`.
- Player manager: `player_manager.add_spawn/clear_spawns/spawn/count/at/dead_count/death`.
- Inventory and items: `inventory.init/get/set/select/selected/primary/secondary/melee/has_gun/target_item`; `item.create/destroy/get/set`.
- Input and controls: `input.get/set/consume/key_down/key_pressed`; `controls.get/set`.
- Simulation: `game.active`, `time.delta`, `camera.yaw`, `physics.grounded/ceiling/gravity_y/move`.
- UI and options: `ui.create/action/click/text/visible/enabled/alpha/value/get_text/get_visible/get_value/change_value/node_count`; `options.get/set/save`.
- Events and feedback: `event.emit`, `audio.play`, `particles.set`.

Arrays and maps are reference values in the VM and can hold other ZS values. In the connected runtime, player, inventory, UI, options and script state persist across repeated VM ticks in the same runtime instance. The options manager also saves and reloads volume, fullscreen, resolution, graphics quality and mouse sensitivity from `Game/Config/settings.ini`; player and inventory saves are still pending. The current physics calls only update kinematic transforms and accept grounded/ceiling input from a future collision adapter. The UI calls maintain a retained node tree but do not draw a native window yet.

## Build and execute

```sh
zumbi_native compile gameplay.zs gameplay.zsc
zumbi_native disasm gameplay.zsc gameplay.zsa\nzumbi_native run gameplay.zsc\nzumbi_native game-loop NativeEngine/scripts/player_system.zsc 10
```

The VM starts the first function in the module; place `fn Main()` first. The VM enforces call-depth, stack, and instruction-count limits. The compiler and VM are an initial general-purpose foundation, not yet the complete language/runtime needed to port every original Unity gameplay system. Nested arrays/maps, native player/inventory/UI/options APIs, event dispatch, spawn selection and repeated VM ticks now exist. Modules/imports, coroutines, debugger/source maps, typed capability-checked native API registration, collision physics, visible UI rendering, persistent save serialization and complete Unity gameplay parity remain planned work.
