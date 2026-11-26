fn Main() {
    let inventory = array.new();
    array.push(inventory, "pistol");
    array.push(inventory, "medkit");
    array.set(inventory, 0, "rifle");
    let count = array.size(inventory);
    let selected = array.get(inventory, 1);
    let data = map.new();
    map.set(data, "health", 75);
    map.set(data, "name", "survivor");
    let health = map.get(data, "health");
    let has_name = map.has(data, "name");
    state.set("active_wave", 3);
    let wave = state.get("active_wave");
    if (count == 2 && selected == "medkit" && array.get(inventory, 0) == "rifle" && health == 75 && has_name && wave == 3) {
        log("ZS collections and state tests OK");
    } else {
        log("ZS collections and state tests FAILED");
    }
    array.pop(inventory);
    map.erase(data, "name");
    state.erase("active_wave");
    if (array.size(inventory) == 1 && !map.has(data, "name") && !state.has("active_wave")) {
        log("ZS collection mutation tests OK");
    } else {
        log("ZS collection mutation tests FAILED");
    }
}
