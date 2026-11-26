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
- `.zbp`: final encrypted bundle format. The target package name is `ZBR Engine.zbp`; it will hold ZSC modules and encrypted game assets.

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

The engine API names above describe the intended binding style. The standalone CLI host currently registers `log`, `print`, `sqrt`, and these data APIs for deterministic VM tests:

- `array.new()`, `array.push(array, value)`, `array.get(array, index)`, `array.set(array, index, value)`, `array.size(array)`, `array.pop(array)`.
- `map.new()`, `map.set(map, key, value)`, `map.get(map, key)`, `map.has(map, key)`, `map.erase(map, key)`.
- `state.set(key, value)`, `state.get(key)`, `state.has(key)`, `state.erase(key)` for module-run state.

Arrays and maps are reference values in the VM and can hold other ZS values. The CLI state store lives for one VM invocation only. This is not yet a persistent ECS component store, a cross-module global system, or the production engine API registry; those are still required for live gameplay integration.

## Build and execute

```sh
zumbi_native compile gameplay.zs gameplay.zsc
zumbi_native disasm gameplay.zsc gameplay.zsa\nzumbi_native run gameplay.zsc
```

The VM starts the first function in the module; place `fn Main()` first. The VM enforces call-depth, stack, and instruction-count limits. The compiler and VM are an initial general-purpose foundation, not yet the complete language/runtime needed to port every original Unity gameplay system. Nested arrays/maps and the CLI state store now exist. Modules/imports, event declarations and dispatch, coroutines, debugger/source maps, typed capability-checked production engine bindings, and persistent entity/component state remain planned work.
