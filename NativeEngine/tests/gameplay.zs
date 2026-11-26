fn Main() {
    let health = 100;
    let wave = 1;
    let totalDamage = calculateDamage(45, 12);
    while (wave < 4) {
        log("Wave", wave);
        wave = wave + 1;
    }
    if (health > 0 && totalDamage == 33) {
        log("Zumbi Scripts VM gameplay logic OK");
    } else {
        log("Gameplay condition failed");
    }
}

fn calculateDamage(base, armor) {
    let reduced = base - armor;
    if (reduced < 0) {
        return 0;
    }
    return reduced;
}
