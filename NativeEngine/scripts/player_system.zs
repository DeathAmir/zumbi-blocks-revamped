fn Main() {
    if (!state.has("player.entity")) {
        let id = player.create();
        state.set("player.entity", id);
        player.set(id, "health", 100);
        player.set(id, "maxHealth", 100);
        player.set(id, "armor", 100);
        player.set(id, "stamina", 100);
        player.set(id, "maxStamina", 100);
        player.set(id, "minStamina", 10);
        player.set(id, "walkingSpeed", 10);
        player.set(id, "sprintingSpeed", 16);
        player.set(id, "crouchingSpeed", 4);
        player.set(id, "standingHeight", 2);
        player.set(id, "crouchingHeight", 1);
        player.set(id, "currentHeight", 2);
        player.set(id, "jumpHeight", 1.2);
        player.set(id, "standingNoise", 0);
        player.set(id, "walkingNoise", 5);
        player.set(id, "sprintingNoise", 15);
        player.set(id, "crouchingNoise", 1);
        player.set(id, "jumpingNoise", 10);
        player.set(id, "noiseReduction", 2);
        player.set(id, "standingStaminaGain", 5);
        player.set(id, "walkingStaminaGain", 2);
        player.set(id, "sprintingStaminaGain", -8);
        player.set(id, "crouchingStaminaGain", 4);
        player.set(id, "aerialStaminaGain", -2);
        player.set(id, "jumpingStaminaCost", 10);
        player.set(id, "averageBleedingDuration", 60);
        player.set(id, "bleedingVariation", 10);
        player.set(id, "bleedingDamage", 2);
        player.set(id, "bleedingRate", 0.7);
        player.set(id, "noiseReduction", 2);
        ui_manager_init();
        hud_initialize();
    }
    let id = state.get("player.entity");
    if (game.active()) {
        player_movement_tick(id, time.delta());
        player_stats_tick(id, time.delta());
        player_hud_tick(id, time.delta());
        ui_manager_tick(time.delta());
    }
}

fn player_stats_tick(id, dt) {
    let noise = player.get(id, "currentNoise");
    if (noise > 0) {
        noise = noise - dt * player.get(id, "noiseReduction");
        if (noise < 0) { noise = 0; }
        player.set(id, "currentNoise", noise);
    }
    if (player.get(id, "dead")) { return; }
    if (!player.get(id, "dummy")) {
        let movement = player.get(id, "movementState");
        let gain = 0;
        let noiseTarget = noise;
        if (movement == 0) {
            gain = player.get(id, "standingStaminaGain");
            noiseTarget = player.get(id, "standingNoise");
        }
        if (movement == 1) {
            gain = player.get(id, "walkingStaminaGain");
            noiseTarget = player.get(id, "walkingNoise");
        }
        if (movement == 2) {
            gain = player.get(id, "sprintingStaminaGain");
            noiseTarget = player.get(id, "sprintingNoise");
        }
        if (movement == 3) {
            gain = player.get(id, "crouchingStaminaGain");
            noiseTarget = player.get(id, "crouchingNoise");
        }
        if (movement == 4 || movement == 5) {
            gain = player.get(id, "aerialStaminaGain");
        }
        if (!player.get(id, "tireless")) {
            let stamina = player.get(id, "stamina");
            let maximum = player.get(id, "maxStamina");
            if ((stamina < maximum && gain > 0) || (stamina > 0 && gain < 0)) {
                stamina = stamina + dt * gain;
            }
            if (stamina < 0) { stamina = 0; }
            if (stamina > maximum) { stamina = maximum; }
            player.set(id, "stamina", stamina);
        }
        if (movement <= 3 && noiseTarget > noise) {
            player.set(id, "currentNoise", noiseTarget);
        }
        if (!player.get(id, "unbleedable")) {
            let duration = player.get(id, "bleedingDuration");
            if (duration <= 0) {
                player_stop_bleeding(id);
            } else {
                player.set(id, "bleedingDuration", duration - dt);
            }
            if (player.get(id, "bleeding")) {
                let next = player.get(id, "nextBleed");
                if (next <= 0) {
                    let rate = player.get(id, "bleedingRate");
                    if (rate > 0) {
                        player.set(id, "nextBleed", 1 / rate);
                        player_bleed(id);
                    }
                } else {
                    player.set(id, "nextBleed", next - dt);
                }
            }
        }
    }
}

fn player_hurt(id, damage, bleedChance) {
    if (player.get(id, "dead") || damage <= 0) { return; }
    let armor = player.get(id, "armor");
    if (armor <= 0) { armor = 100; }
    let health = player.get(id, "health") - damage / (armor / 100);
    player.set(id, "health", health);
    audio.play("player.flesh");
    let luck = random.value();
    if (luck <= bleedChance) {
        let duration = player.get(id, "averageBleedingDuration") + random.range(-player.get(id, "bleedingVariation"), player.get(id, "bleedingVariation"));
        player.set(id, "bleedingDuration", duration);
        if (!player.get(id, "bleeding")) {
            player.set(id, "bleeding", true);
            particles.set("player.blood", true);
        }
    }
    if (!player.get(id, "dummy")) {
        ui.alpha("hud.hurt", 1);
        state.set("hud.hurt_timer", 1);
        ui.value("hud.health", max_number(health, 0), player.get(id, "maxHealth"));
    }
    if (health <= 0) {
        player.set(id, "health", 0);
        if (!player.get(id, "immortal")) { player_death(id); }
    }
}

fn player_bleed(id) {
    if (player.get(id, "dead") || player.get(id, "unbleedable")) { return; }
    let health = player.get(id, "health") - player.get(id, "bleedingDamage");
    player.set(id, "health", health);
    if (!player.get(id, "dummy")) {
        ui.alpha("hud.hurt", 1);
        state.set("hud.hurt_timer", 1);
        ui.value("hud.health", max_number(health, 0), player.get(id, "maxHealth"));
    }
    if (health <= 0) {
        player.set(id, "health", 0);
        player_death(id);
    }
}

fn player_stop_bleeding(id) {
    player.set(id, "bleedingDuration", 0);
    player.set(id, "bleeding", false);
    particles.set("player.blood", false);
}

fn player_heal(id, amount, stanch) {
    if (player.get(id, "dead") || amount <= 0) { return; }
    let health = player.get(id, "health") + amount;
    let maximum = player.get(id, "maxHealth");
    if (health > maximum) { health = maximum; }
    player.set(id, "health", health);
    if (stanch) { player_stop_bleeding(id); }
    if (!player.get(id, "dummy")) {
        ui.value("hud.health", health, maximum);
    }
}

fn player_give_stamina(id, amount) {
    if (player.get(id, "dead") || amount <= 0) { return; }
    let stamina = player.get(id, "stamina") + amount;
    let maximum = player.get(id, "maxStamina");
    if (stamina > maximum) { stamina = maximum; }
    player.set(id, "stamina", stamina);
    if (!player.get(id, "dummy")) {
        ui.value("hud.stamina", stamina, maximum);
    }
}

fn player_death(id) {
    if (player.get(id, "dead") || player.get(id, "immortal")) { return; }
    player.set(id, "dead", true);
    ui.visible("hud.death_screen", true);
    inventory.death_drop(id);
    player_manager.death(id);
    event.emit("player.died", id, player.get(id, "health"));
}

fn player_movement_tick(id, dt) {
    if (!game.active()) { return; }
    let grounded = physics.grounded(id);
    let vertical = player.get(id, "verticalVelocity");
    if (!grounded) {
        vertical = vertical + physics.gravity_y() * dt;
    } else {
        if (vertical < 0) { vertical = 0; }
    }
    player.set(id, "verticalVelocity", vertical);
    if (player.get(id, "dead")) { return; }
    let crouch = input.get("Crouch");
    let sprint = input.get("Sprint");
    let jump = input.get("Jump");
    let x = 0;
    let z = 0;
    if (input.get("MoveRight")) { x = 1; }
    if (input.get("MoveLeft")) { x = -1; }
    if (input.get("MoveForward")) { z = 1; }
    if (input.get("MoveBackward")) { z = -1; }
    let yaw = camera.yaw() * 0.0174532925199433;
    let rx = x * math.cos(yaw) + z * math.sin(yaw);
    let rz = z * math.cos(yaw) - x * math.sin(yaw);
    let length = math.sqrt(rx * rx + rz * rz);
    if (length > 0) {
        rx = rx / length;
        rz = rz / length;
    }
    let movement = 0;
    let speed = player.get(id, "currentSpeed");
    let height = player.get(id, "currentHeight");
    if (grounded) {
        if (physics.ceiling(id)) {
            speed = player.get(id, "crouchingSpeed");
            height = player.get(id, "crouchingHeight");
            movement = 3;
        } else {
            if (crouch) {
                speed = player.get(id, "crouchingSpeed");
                height = player.get(id, "crouchingHeight");
                movement = 3;
            } else {
                if (length == 0) {
                    height = player.get(id, "standingHeight");
                    movement = 0;
                } else {
                    if (sprint && player.get(id, "stamina") > 0 && player.get(id, "stamina") >= player.get(id, "minStamina")) {
                        speed = player.get(id, "sprintingSpeed");
                        height = player.get(id, "standingHeight");
                        movement = 2;
                    } else {
                        speed = player.get(id, "walkingSpeed");
                        height = player.get(id, "standingHeight");
                        movement = 1;
                    }
                }
            }
        }
    } else {
        if (movement != 5 || vertical < 0) {
            speed = player.get(id, "walkingSpeed");
            movement = 4;
        }
    }
    player.set(id, "currentSpeed", speed);
    player.set(id, "currentHeight", height);
    player.set(id, "movementState", movement);
    if (jump) {
        player_jump(id, grounded);
    }
    let horizontalSpeed = player.get(id, "currentSpeed") * dt;
    physics.move(id, rx * horizontalSpeed, player.get(id, "verticalVelocity") * dt, rz * horizontalSpeed);
}

fn player_jump(id, grounded) {
    if (!grounded || player.get(id, "stamina") < player.get(id, "minStamina")) { return; }
    let velocity = player.get(id, "verticalVelocity");
    let jumpSpeed = math.sqrt(-2 * physics.gravity_y() * player.get(id, "jumpHeight"));
    if (velocity > 0) {
        jumpSpeed = max_number(jumpSpeed - velocity, 0);
    }
    velocity = velocity + jumpSpeed;
    player.set(id, "verticalVelocity", velocity);
    player.set(id, "currentNoise", max_number(player.get(id, "jumpingNoise"), player.get(id, "currentNoise")));
    player.set(id, "stamina", player.get(id, "stamina") - player.get(id, "jumpingStaminaCost"));
    player.set(id, "movementState", 5);
}

fn ui_manager_init() {
    ui.visible("menu.0", true);
    ui.visible("pause.menu", false);
    ui.visible("title", true);
    ui.visible("center.primary", false);
    ui.visible("center.secondary", false);
    state.set("ui.primary_timer", 0);
    state.set("ui.secondary_timer", 0);
    state.set("ui.stopwatch", 0);
}

fn ui_menu(index) {
    if (index < 0 || index > 10) { return; }
    let i = 0;
    while (i <= 10) {
        ui.visible("menu." + i, false);
        i = i + 1;
    }
    ui.visible("pause.menu", false);
    ui.visible("menu." + index, true);
    if (index == 0 || index == 5) {
        ui.visible("title", true);
    } else {
        ui.visible("title", false);
    }
}

fn ui_close_menu(pause) {
    let i = 0;
    while (i <= 10) {
        ui.visible("menu." + i, false);
        i = i + 1;
    }
    ui.visible("pause.menu", pause);
    ui.visible("title", false);
}

fn ui_primary_message(message, duration) {
    if (duration <= 0) { duration = 3; }
    ui.text("center.primary", message);
    ui.visible("center.primary", true);
    state.set("ui.primary_timer", duration);
}

fn ui_secondary_message(message, duration) {
    if (duration <= 0) { duration = 3; }
    ui.text("center.secondary", message);
    ui.visible("center.secondary", true);
    state.set("ui.secondary_timer", duration);
}

fn ui_manager_tick(dt) {
    let primary = state.get("ui.primary_timer");
    if (primary > 0) {
        primary = primary - dt;
        state.set("ui.primary_timer", primary);
        if (primary <= 0) { ui.visible("center.primary", false); }
    }
    let secondary = state.get("ui.secondary_timer");
    if (secondary > 0) {
        secondary = secondary - dt;
        state.set("ui.secondary_timer", secondary);
        if (secondary <= 0) { ui.visible("center.secondary", false); }
    }
    let elapsed = state.get("ui.stopwatch") + dt;
    state.set("ui.stopwatch", elapsed);
}

fn hud_initialize() {
    ui.visible("hud.death_screen", false);
    ui.visible("hud.hurt", true);
    ui.alpha("hud.hurt", 0);
    ui.visible("hud.stats_panel", true);
    ui.visible("hud.gun_panel", true);
    ui.visible("hud.center_crosshair", true);
    ui.visible("hud.dynamic_crosshair", true);
    ui.visible("hud.hitmarker", false);
    ui.visible("hud.interaction_crosshair", true);
    state.set("hud.hurt_timer", 0);
    state.set("hud.hitmarker_timer", 0);
}

fn player_hud_tick(id, dt) {
    if (!player.get(id, "dummy")) {
        ui.value("hud.health", player.get(id, "health"), player.get(id, "maxHealth"));
        ui.value("hud.stamina", player.get(id, "stamina"), player.get(id, "maxStamina"));
        ui.value("hud.noise", player.get(id, "currentNoise"), 100);
    }
    let hurt = state.get("hud.hurt_timer");
    if (hurt > 0) {
        hurt = hurt - dt;
        state.set("hud.hurt_timer", hurt);
        ui.alpha("hud.hurt", max_number(hurt, 0));
    }
    let hit = state.get("hud.hitmarker_timer");
    if (hit > 0) {
        hit = hit - dt;
        state.set("hud.hitmarker_timer", hit);
        if (hit <= 0) { ui.visible("hud.hitmarker", false); }
    }
}

fn hud_weapon(name, ammo) {
    ui.text("hud.weapon_name", name);
    ui.text("hud.ammo", ammo);
}

fn hud_hitmarker(duration) {
    if (duration <= 0) { duration = 1; }
    ui.visible("hud.hitmarker", true);
    state.set("hud.hitmarker_timer", duration);
}

fn max_number(a, b) {
    if (a > b) { return a; }
    return b;
}
