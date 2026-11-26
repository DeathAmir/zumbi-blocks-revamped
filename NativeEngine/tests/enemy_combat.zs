fn Main() {
    let hit = take_hit(120, 30, 100, false);
    let armored = take_hit(120, 30, 150, false);
    let ignored = take_hit(120, 30, 100, true);
    if (hit == 90 && armored == 100 && ignored == 120) {
        log("Enemy combat port tests OK");
    } else {
        log("Enemy combat port tests FAILED");
    }
}

fn take_hit(health, damage, armor, dead) {
    if (dead || damage <= 0) { return health; }
    let safe_armor = armor;
    if (safe_armor <= 0) { safe_armor = 100; }
    let result = health - damage / (safe_armor / 100);
    if (result < 0) { result = 0; }
    return result;
}

fn attack_ready(cooldown) {
    return cooldown <= 0;
}

fn next_attack_cooldown(attacks_per_second) {
    if (attacks_per_second <= 0) { return 0; }
    return 1 / attacks_per_second;
}
