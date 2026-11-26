// Zumbi Scripts - UI module example
fn Main() {
    ui.panel("hud")
    ui.text("health", "100")
    ui.text("ammo", "30 / 120")
    ui.button("inventory", "Inventory")
    ui.visible("hud", true)
}
