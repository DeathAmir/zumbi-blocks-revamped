fn Main() {
    let health = apply_damage(100, 25, 100, false);
    let armored = apply_damage(100, 25, 200, false);
    let healed = heal(health, 50, 100, false);
    let stamina = stamina_tick(40, 100, 8, 2, false);
    let noise = noise_tick(30, 4, 2);
    if (health == 75 && armored == 87.5 && healed == 100 && stamina == 56 && noise == 22) {
        log("PlayerStats port tests OK");
    } else {
        log("PlayerStats port tests FAILED");
    }
}

fn apply_damage(current, damage, armor, immortal) {
    if (current <= 0 || damage <= 0) { return current; }
    let safe_armor = armor;
    if (safe_armor <= 0) { safe_armor = 100; }
    let next = current - damage / (safe_armor / 100);
    if (next <= 0) { next = 0; }
    return next;
}

fn heal(current, amount, maximum, dead) {
    if (dead || amount <= 0) { return current; }
    let next = current + amount;
    if (next > maximum) { next = maximum; }
    return next;
}

fn stamina_tick(current, maximum, gain, delta, tireless) {
    if (!tireless) {
        if ((current < maximum && gain > 0) || (current > 0 && gain < 0)) {
            current = current + gain * delta;
        }
        if (current < 0) { current = 0; }
        if (current > maximum) { current = maximum; }
    }
    return current;
}

fn noise_tick(current, reduction, delta) {
    let next = current - reduction * delta;
    if (next < 0) { next = 0; }
    return next;
}
