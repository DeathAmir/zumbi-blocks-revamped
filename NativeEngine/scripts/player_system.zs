fn Main() {
    if (!state.has("player.entity")) {
        let id = player.create();
        state.set("player.entity", id);
        player.set(id, "health", 100);
        player.set(id, "attackDamage", 14);
        player.set(id, "smelliness", 8);
        player.set(id, "maxHealth", 100);
        player.set(id, "armor", 100);
        player.set(id, "stamina", 40);
        player.set(id, "maxStamina", 40);
        player.set(id, "minStamina", 8);
        player.set(id, "walkingSpeed", 6);
        player.set(id, "acceleration", 6);
        player.set(id, "aerialAcceleration", 2);
        player.set(id, "slowResistance", 10);
        player.set(id, "sprintingSpeed", 10);
        player.set(id, "crouchingSpeed", 2);
        player.set(id, "standingHeight", 2.4);
        player.set(id, "crouchingHeight", 1.2);
        player.set(id, "currentHeight", 2.4);
        player.set(id, "jumpHeight", 1.5);
        player.set(id, "standingNoise", 0);
        player.set(id, "walkingNoise", 400);
        player.set(id, "sprintingNoise", 800);
        player.set(id, "crouchingNoise", 150);
        player.set(id, "jumpingNoise", 800);
        player.set(id, "noiseReduction", 1000);
        player.set(id, "maxNoise", 3000);
        player.set(id, "standingStaminaGain", 6);
        player.set(id, "walkingStaminaGain", 4);
        player.set(id, "sprintingStaminaGain", -7);
        player.set(id, "crouchingStaminaGain", 5);
        player.set(id, "aerialStaminaGain", 2);
        player.set(id, "jumpingStaminaCost", 10);
        player.set(id, "averageBleedingDuration", 60);
        player.set(id, "bleedingVariation", 10);
        player.set(id, "bleedingDamage", 1);
        player.set(id, "bleedingRate", 0.5);
        player.set(id, "crouchHeightPercentage", 50);
        player.set(id, "noiseReduction", 2);
        controls_init();
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
    if (input.get("testMode")) {
        test.expect(player.get(id, "movementState") == 2, "Player sprint state did not migrate");
        test.expect(player.position(id, "z") > 0, "Player transform did not move through the runtime");
        player_hurt(id, 25, 0);
        test.expect(player.get(id, "health") == 75, "Armor-adjusted player damage mismatch");
        player_heal(id, 10, false);
        test.expect(player.get(id, "health") == 85, "Player healing mismatch");
        player_give_stamina(id, 50);
        test.expect(player.get(id, "stamina") == 40, "Stamina cap mismatch");
        ui_menu(4);
        test.expect(ui.get_visible("menu.4"), "UI menu selection did not activate the requested panel");
        test.expect(!ui.get_visible("menu.0"), "UI menu selection did not hide the previous panel");
        test.expect(ui.get_value("hud.health") == 85, "HUD health bar did not update");
        ui_primary_message("Runtime test", 0.05);
        ui_manager_tick(0.1);
        test.expect(!ui.get_visible("center.primary"), "UI message timer did not close the message");
        test.expect(!player.get(id, "dead"), "Player was unexpectedly marked dead");
        log("Connected ZS player, movement and UI runtime tests OK");
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
        state.set("hud.hurt_timer", 0.5);
        state.set("hud.hurt_power", damage / player.get(id, "maxHealth"));
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
        state.set("hud.hurt_timer", 0.5);
        state.set("hud.hurt_power", player.get(id, "bleedingDamage") * 5 / player.get(id, "maxHealth"));
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
    let crouch = input.key_down(controls.get("Crouch"));
    let sprint = input.key_down(controls.get("Sprint"));
    let jump = input.key_pressed(controls.get("Jump"));
    let x = 0;
    let z = 0;
    if (input.key_down(controls.get("MoveRight"))) { x = 1; }
    if (input.key_down(controls.get("MoveLeft"))) { x = -1; }
    if (input.key_down(controls.get("MoveForward"))) { z = 1; }
    if (input.key_down(controls.get("MoveBackward"))) { z = -1; }
    let yaw = camera.yaw() * 0.0174532925199433;
    let rx = x * math.cos(yaw) + z * math.sin(yaw);
    let rz = z * math.cos(yaw) - x * math.sin(yaw);
    let length = math.sqrt(rx * rx + rz * rz);
    if (length > 0) {
        rx = rx / length;
        rz = rz / length;
    }
    let movement = player.get(id, "movementState");
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
                    if (sprint && player.get(id, "stamina") > 0 && (player.get(id, "stamina") >= player.get(id, "minStamina") || movement == 2)) {
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

fn controls_init() {
    let names = array.new();
    let keys = array.new();
    array.push(names, "MoveForward"); array.push(keys, "W");
    array.push(names, "MoveLeft"); array.push(keys, "A");
    array.push(names, "MoveBackward"); array.push(keys, "S");
    array.push(names, "MoveRight"); array.push(keys, "D");
    array.push(names, "Jump"); array.push(keys, "Space");
    array.push(names, "Crouch"); array.push(keys, "LeftControl");
    array.push(names, "Sprint"); array.push(keys, "LeftShift");
    array.push(names, "BasicFire"); array.push(keys, "Mouse0");
    array.push(names, "Aim"); array.push(keys, "Mouse1");
    array.push(names, "Reload"); array.push(keys, "R");
    array.push(names, "Inspect"); array.push(keys, "F");
    array.push(names, "AltFire"); array.push(keys, "Mouse1");
    array.push(names, "ItemInteract"); array.push(keys, "E");
    array.push(names, "ItemDrop"); array.push(keys, "G");
    array.push(names, "ItemSlot1"); array.push(keys, "Alpha1");
    array.push(names, "ItemSlot2"); array.push(keys, "Alpha2");
    array.push(names, "ItemSlot3"); array.push(keys, "Alpha3");
    array.push(names, "ItemSlot4"); array.push(keys, "Alpha4");
    array.push(names, "ItemSlot5"); array.push(keys, "Alpha5");
    array.push(names, "ItemSlot6"); array.push(keys, "Alpha6");
    array.push(names, "ItemSlot7"); array.push(keys, "Alpha7");
    array.push(names, "ItemSlot8"); array.push(keys, "Alpha8");
    array.push(names, "ItemSlot9"); array.push(keys, "Alpha9");
    array.push(names, "BasicAttack"); array.push(keys, "Mouse0");
    array.push(names, "AltAttack"); array.push(keys, "Mouse1");
    array.push(names, "BasicThrow"); array.push(keys, "Mouse0");
    array.push(names, "AltThrow"); array.push(keys, "Mouse1");
    array.push(names, "BasicPlace"); array.push(keys, "Mouse0");
    array.push(names, "AltPlace"); array.push(keys, "Mouse1");
    array.push(names, "BasicUse"); array.push(keys, "Mouse0");
    array.push(names, "AltUse"); array.push(keys, "Mouse1");
    array.push(names, "SpectatorForward"); array.push(keys, "W");
    array.push(names, "SpectatorLeft"); array.push(keys, "A");
    array.push(names, "SpectatorBackward"); array.push(keys, "S");
    array.push(names, "SpectatorRight"); array.push(keys, "D");
    array.push(names, "SpectatorUp"); array.push(keys, "Space");
    array.push(names, "SpectatorDown"); array.push(keys, "V");
    array.push(names, "SpectatorFast"); array.push(keys, "LeftShift");
    array.push(names, "SpectatorSlow"); array.push(keys, "LeftControl");
    state.set("controls.names", names);
    state.set("controls.mouseSensitivity", 6);
    let i = 0;
    while (i < array.size(names)) {
        let name = array.get(names, i);
        let key = array.get(keys, i);
        controls.set(name, key);
        ui.create("controls.bind." + name, "button", "menu.10", 373, -60 - i * 42, 360, 38);
        ui.text("controls.bind." + name, name + " : " + key);
        ui.action("controls.bind." + name, "Controls.Rebind", 0);
        i = i + 1;
    }
    ui.create("controls.back", "button", "menu.10", 373, -1740, 360, 44);
    ui.text("controls.back", "Back");
    ui.action("controls.back", "Menu", 4);
    ui.create("controls.reset", "button", "menu.10", 373, -1790, 360, 44);
    ui.text("controls.reset", "Reset controls");
    ui.action("controls.reset", "ResetAllControls", 0);
    ui.create("controls.sensitivity", "slider", "menu.10", 373, -1840, 360, 36);
    ui.value("controls.sensitivity", 6, 20);
}

fn ui_manager_init() {
    ui.create("canvas", "canvas", "", 0, 0, 800, 600);
    ui.create("menu.0", "panel", "canvas", 0, -70, 400, 200);
    ui.create("menu.0.play", "button", "menu.0", 0, 0, 360, 40);
    ui.text("menu.0.play", "Play");
    ui.action("menu.0.play", "Menu", 1);
    ui.create("menu.0.character", "button", "menu.0", 0, -40, 360, 40);
    ui.text("menu.0.character", "Character");
    ui.action("menu.0.character", "Menu", 3);
    ui.create("menu.0.options", "button", "menu.0", 0, -80, 360, 40);
    ui.text("menu.0.options", "Options");
    ui.action("menu.0.options", "Menu", 4);
    ui.create("menu.0.quit", "button", "menu.0", 0, -120, 360, 40);
    ui.text("menu.0.quit", "Quit");
    ui.action("menu.0.quit", "Quit", 1);
    ui.create("menu.0.credits", "button", "menu.0", 0, -160, 360, 40);
    ui.text("menu.0.credits", "Credits");
    ui.action("menu.0.credits", "Menu", 5);

    ui.create("menu.1", "panel", "canvas", 0, -49.934, 700, 300);
    ui.create("menu.1.singleplayer", "button", "menu.1", 350, -50.666664, 696, 97.33333);
    ui.text("menu.1.singleplayer", "Singleplayer");
    ui.action("menu.1.singleplayer", "SceneChanger", 1);
    ui.create("menu.1.multiplayer", "button", "menu.1", 350, -150, 696, 97.33333);
    ui.text("menu.1.multiplayer", "Multiplayer (coming soon)");
    ui.action("menu.1.multiplayer", "SceneChanger", 1);
    ui.create("menu.1.back", "button", "menu.1", 350, -249.33331, 696, 97.33333);
    ui.text("menu.1.back", "Back");
    ui.action("menu.1.back", "Menu", 0);

    ui.create("menu.2", "panel", "canvas", 0, -50, 700, 300);
    ui.create("menu.2.map1", "button", "menu.2", 0, -120.25, 696, 57.5);
    ui.text("menu.2.map1", "zb_070_remastered");
    ui.action("menu.2.map1", "SceneChanger", 1);
    ui.create("menu.2.map2", "button", "menu.2", 0, -179.75, 696, 57.5);
    ui.text("menu.2.map2", "zb_alphatests");
    ui.action("menu.2.map2", "SceneChanger", 2);
    ui.create("menu.2.back", "button", "menu.2", 0, -254.25, 696, 87.5);
    ui.text("menu.2.back", "Back");
    ui.action("menu.2.back", "Menu", 1);

    ui.create("menu.3", "panel", "canvas", 0, -49.934, 400, 200);
    ui.create("menu.3.appearance", "button", "menu.3", 350, -50.666668, 696, 97.33333);
    ui.text("menu.3.appearance", "Appearance (coming soon)");
    ui.action("menu.3.appearance", "Menu", 0);
    ui.create("menu.3.shop", "button", "menu.3", 350, -150, 696, 97.33333);
    ui.text("menu.3.shop", "Shop (coming soon)");
    ui.action("menu.3.shop", "Menu", 0);
    ui.create("menu.3.back", "button", "menu.3", 350, -250, 696, 97.33333);
    ui.text("menu.3.back", "Back");
    ui.action("menu.3.back", "Menu", 0);

    ui.create("menu.4", "panel", "canvas", 0, 0, 750, 430);
    ui.create("menu.4.volume", "slider", "menu.4", 375, -32, 746, 60);
    ui.create("menu.4.resolution", "dropdown", "menu.4", 375, -94, 746, 60);
    ui.create("menu.4.quality", "dropdown", "menu.4", 375, -156, 746, 60);
    ui.create("menu.4.controls", "button", "menu.4", 375, -333.5, 746, 55);
    ui.text("menu.4.controls", "Controls");
    ui.action("menu.4.controls", "Menu", 10);
    ui.create("menu.4.save", "button", "menu.4", 375, -276.5, 746, 55);
    ui.text("menu.4.save", "Save options");
    ui.action("menu.4.save", "SaveOptions", 0);
    ui.create("menu.4.back", "button", "menu.4", 375, -390.5, 746, 55);
    ui.text("menu.4.back", "Back");
    ui.action("menu.4.back", "Menu", 0);

    ui.create("menu.5", "panel", "canvas", 0, 0, 400, 200);
    ui.create("menu.5.back", "button", "menu.5", 200, -177, 396, 40);
    ui.text("menu.5.back", "Back");
    ui.action("menu.5.back", "Menu", 0);

    ui.create("menu.6", "panel", "canvas", 0, 0, 700, 500);
    ui.create("menu.7", "panel", "canvas", 0, 0, 700, 500);
    ui.create("menu.8", "panel", "canvas", 0, 0, 700, 500);
    ui.create("menu.9", "panel", "canvas", 0, 0, 700, 500);
    ui.create("menu.10", "scroll_panel", "canvas", 0, 0, 750, 550);
    ui.create("title", "image", "canvas", 0, 220, 420, 120);
    ui.create("pause.menu", "panel", "canvas", 0, 0, 500, 300);
    ui.create("pause.resume", "button", "pause.menu", 250, -171.96066, 495.99994, 89.39928);
    ui.text("pause.resume", "Resume");
    ui.action("pause.resume", "UnPause", 0);
    ui.create("pause.main_menu", "button", "pause.menu", 250, -263.35992, 495.99994, 89.39928);
    ui.text("pause.main_menu", "Menu");
    ui.action("pause.main_menu", "SceneChanger", 0);
    let panelIndex = 0;
    while (panelIndex <= 10) {
        ui.visible("menu." + panelIndex, false);
        panelIndex = panelIndex + 1;
    }
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
    if (input.consume("ui.clicked.menu.0.play")) { ui_menu(1); }
    if (input.consume("ui.clicked.menu.0.quit")) { event.emit("game.quit", 0, 0); }
    if (input.consume("ui.clicked.controls.back")) { ui_menu(4); }
    if (input.consume("ui.clicked.controls.reset")) { controls_init(); ui_primary_message("Controls reset", 2); }
    let binding = state.get("controls.binding");
    if (binding != null) {
        let newKey = input.get("lastKey");
        if (newKey != false && newKey != "") {
            controls.set(binding, newKey);
            ui.text("controls.bind." + binding, binding + " : " + newKey);
            state.erase("controls.binding");
        }
    }
    let controlNames = state.get("controls.names");
    let controlIndex = 0;
    while (controlIndex < array.size(controlNames)) {
        let controlName = array.get(controlNames, controlIndex);
        if (input.consume("ui.clicked.controls.bind." + controlName)) {
            state.set("controls.binding", controlName);
            ui_primary_message("Press a key for " + controlName, 0);
        }
        controlIndex = controlIndex + 1;
    }
    if (input.consume("ui.clicked.menu.0.character")) { ui_menu(3); }
    if (input.consume("ui.clicked.menu.0.options")) { ui_menu(4); }
    if (input.consume("ui.clicked.menu.0.credits")) { ui_menu(5); }
    if (input.consume("ui.clicked.menu.1.back")) { ui_menu(0); }
    if (input.consume("ui.clicked.menu.1.singleplayer")) { event.emit("scene.change", 0, 1); }
    if (input.consume("ui.clicked.menu.1.multiplayer")) { ui_primary_message("Multiplayer is coming soon", 3); }
    if (input.consume("ui.clicked.menu.2.back")) { ui_menu(1); }
    if (input.consume("ui.clicked.menu.2.map1")) { event.emit("scene.change", 0, 1); }
    if (input.consume("ui.clicked.menu.2.map2")) { event.emit("scene.change", 0, 2); }
    if (input.consume("ui.clicked.menu.3.back")) { ui_menu(0); }
    if (input.consume("ui.clicked.menu.3.appearance")) { ui_primary_message("Appearance is coming soon", 3); }
    if (input.consume("ui.clicked.menu.3.shop")) { ui_primary_message("Shop is coming soon", 3); }
    if (input.consume("ui.clicked.menu.4.back")) { ui_menu(0); }
    if (input.consume("ui.clicked.menu.4.controls")) { ui_menu(10); }
    if (input.consume("ui.clicked.menu.4.save")) { event.emit("options.save", 0, 0); ui_primary_message("Options saved", 2); }
    if (input.consume("ui.clicked.menu.5.back")) { ui_menu(0); }
    if (input.consume("ui.clicked.pause.resume")) { ui_close_menu(false); event.emit("game.resume", 0, 0); }
    if (input.consume("ui.clicked.pause.main_menu")) { event.emit("scene.change", 0, 0); }
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
    ui.create("hud.stats_panel", "panel", "canvas", 150, 50, 300, 100);
    ui.create("hud.health", "slider", "hud.stats_panel", 150, -18.666666, 292, 29.333332);
    ui.create("hud.stamina", "slider", "hud.stats_panel", 150, -50, 292, 29.333332);
    ui.create("hud.noise", "slider", "hud.stats_panel", 150, -81.33333, 292, 29.333332);
    ui.create("hud.gun_panel", "panel", "canvas", -120, 50, 240, 100);
    ui.create("hud.weapon_name", "text", "hud.gun_panel", 120, -26.5, 232, 45);
    ui.create("hud.ammo", "text", "hud.gun_panel", 120, -73.5, 232, 45);
    ui.create("hud.death_screen", "image", "canvas", 0, 0, 800, 600);
    ui.create("hud.hurt", "image", "canvas", 0, 0, 800, 600);
    ui.create("hud.center_crosshair", "image", "canvas", 0, 0, 32, 32);
    ui.create("hud.dynamic_crosshair", "image", "canvas", 0, 0, 32, 32);
    ui.create("hud.hitmarker", "image", "canvas", 0, 0, 32, 32);
    ui.create("hud.interaction_crosshair", "image", "canvas", 0, 0, 32, 32);
    ui.create("center.primary", "text", "canvas", 0, -138, 600, 48);
    ui.create("center.secondary", "text", "canvas", 0, -190, 600, 40);
    ui.create("stopwatch", "text", "canvas", 0, -19, 200, 30);
    ui.visible("hud.death_screen", false);
    ui.visible("hud.hurt", true);
    ui.alpha("hud.hurt", 0);
    ui.alpha("hud.death_screen", 0.39215687);
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
        ui.value("hud.noise", player.get(id, "currentNoise"), 3000);
    }
    let hurt = state.get("hud.hurt_timer");
    if (hurt > 0) {
        hurt = hurt - dt;
        state.set("hud.hurt_timer", hurt);
        ui.alpha("hud.hurt", max_number(hurt, 0) / 0.5 * state.get("hud.hurt_power"));
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
    if (duration <= 0) { duration = 0.5; }
    ui.visible("hud.hitmarker", true);
    state.set("hud.hitmarker_timer", duration);
}

fn max_number(a, b) {
    if (a > b) { return a; }
    return b;
}
