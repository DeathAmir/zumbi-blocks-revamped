fn Main() {
    let sprint = movement_speed(10, 16, 4, true, false, 80, 10);
    let crouch = movement_speed(10, 16, 4, false, true, 80, 10);
    let exhausted = movement_speed(10, 16, 4, true, false, 0, 10);
    let jump = jump_velocity(-9.81, 4);
    if (sprint == 16 && crouch == 4 && exhausted == 10 && jump > 8.8 && jump < 8.9) {
        log("PlayerMovement port tests OK");
    } else {
        log("PlayerMovement port tests FAILED");
    }
}

fn movement_speed(walk, sprint, crouch, sprint_intent, crouch_intent, stamina, minimum) {
    if (crouch_intent) { return crouch; }
    if (sprint_intent && stamina > 0 && stamina >= minimum) { return sprint; }
    return walk;
}

fn jump_velocity(gravity_y, jump_height) {
    return sqrt(-2 * gravity_y * jump_height);
}
