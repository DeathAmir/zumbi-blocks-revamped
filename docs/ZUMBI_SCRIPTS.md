# Zumbi Scripts (ZS) toolchain

ZS is the high-level, engine-owned scripting language. It is not Lua and does not embed a Lua runtime. The native C++ engine owns rendering, physics, input, audio, navigation, networking, resource lifetime, and the VM. ZS is intended for UI composition, menus, HUD behavior, gameplay rules, events, and mod-facing logic.

## Compilation pipeline

1. `.zs` — readable high-level source written by developers.
2. `.zsa` — readable Zumbi Script Assembly emitted by the ZSC compiler. It exposes the lowered UI opcodes and the symbol table for debugging.
3. `.zsc` — planned compact bytecode module. The assembler/bytecode stage will validate instruction operands, encode opcodes, and apply compression.
4. `.zbp` — Zumbi Bundle Package. The final `ZBR Engine.zbp` package will contain compiled ZSC modules, asset metadata, and encrypted asset chunks. Development builds may additionally include ZSA debug listings.

The current compiler milestone implements the first lowering pass from ZS to readable ZSA. The bytecode VM, compression, bundle directory/index, and package signing are subsequent milestones; the current encrypted single-file archive is only the cryptographic primitive, not yet the final ZBP format.

## Syntax v0.1

A source file contains zero-argument functions. UI calls are deliberately small and declarative:

```zs
fn Main() {
    ui.panel("hud")
    ui.text("health", "100")
    ui.button("inventory", "Inventory")
    ui.visible("hud", true)
}
```

Supported calls in compiler v0.1:

- `ui.panel(id)`
- `ui.text(id, value)`
- `ui.button(id, label)`
- `ui.visible(id, true|false)`

Strings use double quotes. A function is declared with `fn Name() {` and closed with `}`. The parser reports source line numbers for invalid declarations or calls.

## Symbol scrambling

Each function receives a unique three-character lowercase-alphanumeric symbol during compilation. The generated symbol table maps readable source names to short assembly names. The random mapping is for compactness/obfuscation only; it is not encryption or a security boundary. Runtime API names and engine ABI symbols remain stable and are not randomized.

## Native/script boundary

The C++ runtime owns authoritative state and performance-critical systems. ZS accesses them through a versioned, capability-checked native API. A script cannot directly call arbitrary C++ functions. UI rendering is submitted through the native UI API; physics and simulation remain native.

## Current compiler command

```sh
zumbi_native compile samples/ui/main.zs main.zsa
```

The current command writes human-readable ZSA. It does not yet execute scripts or emit ZSC bytecode.
