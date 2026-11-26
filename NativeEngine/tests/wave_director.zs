fn Main() {
    let required = required_kills(20, 75);
    let complete = wave_finished(15, 20, 75);
    let incomplete = wave_finished(14, 20, 75);
    let next = next_wave_number(3);
    if (required == 15 && complete && !incomplete && next == 4) {
        log("WaveDirector port tests OK");
    } else {
        log("WaveDirector port tests FAILED");
    }
}

fn required_kills(zombie_amount, kill_percent) {
    return zombie_amount * (kill_percent / 100);
}

fn wave_finished(death_count, zombie_amount, kill_percent) {
    return death_count >= required_kills(zombie_amount, kill_percent);
}

fn next_wave_number(current_wave) {
    return current_wave + 1;
}

fn countdown_tick(remaining, delta) {
    let next = remaining - delta;
    if (next < 0) { next = 0; }
    return next;
}
