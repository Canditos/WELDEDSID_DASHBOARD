// ============================================================
// SICHARGE D - Professional Enclosure
// ESP32 16ch Relay + LM2596 Step-Down + DFRobot DAC
// ============================================================
// Author: SICHARGE D Project
// Units: millimeters
// Print Settings: 0.2mm layer, 20% infill, no supports needed
// ============================================================

/* [Render Options] */
// What to render
render_part = "both"; // [both, base, lid]
// Explode view (for visualization)
explode = 0; // [0:1:60]

/* [Wall Settings] */
wall = 2.5;           // Wall thickness
floor_h = 2.5;        // Floor thickness  
lid_h = 2.0;          // Lid thickness
corner_r = 4;         // Corner radius

/* [Component Dimensions - Relay Board] */
// 16-channel WiFi relay module (ESP32)
relay_l = 168;        // Length (X) - with tolerance
relay_w = 93;         // Width (Y) - with tolerance
relay_h = 28;         // Height (Z) - including relay tops
relay_mount_d = 3.2;  // Mounting hole diameter
relay_mount_inset = 3; // Mounting hole inset from edges

/* [Component Dimensions - LM2596 Step-Down] */
lm2596_l = 68;        // Length
lm2596_w = 35;        // Width
lm2596_h = 15;        // Height
lm2596_mount_d = 3.2; // Mounting hole diameter

/* [Component Dimensions - DAC DFR0971] */
dac_l = 36;           // Length
dac_w = 28;           // Width
dac_h = 13;           // Height
dac_mount_d = 3.2;    // Mounting hole diameter

/* [Internal Layout] */
// Spacing between components
comp_gap = 8;         // Gap between components
back_gap = 25;        // Extra gap at the back for cable routing and screwdriver access
cable_channel_w = 15; // Width of cable routing channels

// ============================================================
// CALCULATED DIMENSIONS & LAYOUT POSITIONS
// ============================================================

// Base inner dimensions
// With LM2596 rotated 90 deg, its X dimension is lm2596_w (35) and Y is lm2596_l (68)
// Max X needed on the right is max(dac_l, lm2596_w) = max(36, 35) = 36
// Added +15mm to ensure modules clear the corner screw bosses
inner_l = relay_l + max(dac_l, lm2596_w) + (comp_gap * 3) + 15;
// Increased to 145mm to give >30mm gap between Step-Up and DAC for wiring
inner_w = 145;
inner_h = 36; // Sufficient height for 28mm tall relays + 6mm posts + 2mm tolerance

// Outer dimensions
outer_l = inner_l + (wall * 2);
outer_w = inner_w + (wall * 2);
outer_h = inner_h + wall; // Base total height

// Component Positions
relay_x = comp_gap / 2;
relay_y = comp_gap + 20; // Moved 20mm up to avoid bottom-left corner pillar

aux_x = relay_l + comp_gap + comp_gap / 2;
aux_w = inner_w;
aux_x_space = inner_l - aux_x; // Total X space for the right side

// Center components in the available X space on the right
dac_x = aux_x + (aux_x_space - dac_l) / 2;
lm2596_x = aux_x + (aux_x_space - lm2596_w) / 2; // Using width because it's rotated

// Distribute Y space according to diagram
// Step-Up (LM2596) at the bottom (front), DAC at the top (back)
lm2596_y = comp_gap + 10; // Shifted 10mm up for bottom wiring clearance
dac_y = inner_w - dac_w - comp_gap; // Pushed to the top to maximize gap

// ============================================================
// MAIN RENDERING
// ============================================================

echo(str("=== ENCLOSURE DIMENSIONS ==="));
echo(str("External: ", outer_l, " x ", outer_w, " x ", outer_h, " mm"));
echo(str("Internal: ", inner_l, " x ", inner_w, " x ", inner_h, " mm"));
echo(str("============================"));

if (render_part == "both") {
    color("#1a1a2e") base();
    if(show_pcbs) dummy_pcbs();
    translate([0, 0, outer_h - lid_h + explode]) 
        color("#009999", 0.8) lid();
} else if (render_part == "base") {
    base();
    if(show_pcbs) dummy_pcbs();
} else {
    lid();
}

// ============================================================
// BASE MODULE
// ============================================================
module base() {
    difference() {
        union() {
            // Main shell
            rounded_box(outer_l, outer_w, outer_h - lid_h, corner_r);
            
            // Lid alignment rim (inner lip)
            translate([wall + 1, wall + 1, outer_h - lid_h - 0.01])
                difference() {
                    rounded_box(outer_l - wall*2 - 2, outer_w - wall*2 - 2, 3, corner_r - 2);
                    translate([1.2, 1.2, -0.5])
                        rounded_box(outer_l - wall*2 - 4.4, outer_w - wall*2 - 4.4, 4, corner_r - 3);
                }
        }
        
        // Hollow interior
        translate([wall, wall, floor_h])
            rounded_box(inner_l, inner_w, inner_h + 10, corner_r - wall/2);
        
        // --- CABLE SLOTS ---
        cable_slots();
        
        // --- VENTILATION SLOTS ---
        ventilation_slots();
        
        // --- LABEL RECESS on front ---
        translate([outer_l/2, -0.5, outer_h/2 - lid_h/2])
            rotate([90, 0, 0])
                rounded_box(60, 12, 1, 2);
    }
    
    // --- MOUNTING POSTS ---
    // Relay Board mounts (4 posts based on real PCB)
    translate([wall, wall, floor_h]) {
        // Top-Left
        mount_post(relay_x + relay_mount_inset, 
                   relay_y + relay_w - relay_mount_inset, 
                   6, relay_mount_d);
        // Middle-Left
        mount_post(relay_x + relay_mount_inset, 
                   relay_y + relay_w / 2, 
                   6, relay_mount_d);
        // Top-Right
        mount_post(relay_x + relay_l - relay_mount_inset, 
                   relay_y + relay_w - relay_mount_inset, 
                   6, relay_mount_d);
        // Bottom-Right
        mount_post(relay_x + relay_l - relay_mount_inset, 
                   relay_y + relay_mount_inset, 
                   6, relay_mount_d);
    }
    
    // LM2596 mounts (4 posts, rotated 90 degrees so L is along Y and W is along X)
    translate([wall, wall, floor_h]) {
        mount_post(lm2596_x + 3, lm2596_y + 3, 4, lm2596_mount_d);
        mount_post(lm2596_x + lm2596_w - 3, lm2596_y + lm2596_l - 3, 4, lm2596_mount_d);
        mount_post(lm2596_x + 3, lm2596_y + lm2596_l - 3, 4, lm2596_mount_d);
        mount_post(lm2596_x + lm2596_w - 3, lm2596_y + 3, 4, lm2596_mount_d);
    }
    
    // DAC mounts (2 posts + platform)
    translate([wall, wall, floor_h]) {
        // DAC platform (raised to align with relay board connectors)
        translate([dac_x, dac_y, 0])
            dac_platform();
    }
    
    // --- CORNER REINFORCEMENTS ---
    translate([wall, wall, floor_h]) {
        corner_gusset(0, 0);
        corner_gusset(inner_l, 0);
        corner_gusset(0, inner_w);
        corner_gusset(inner_l, inner_w);
    }
    
    // --- SCREW BOSSES for lid (4 corners) ---
    screw_boss_positions() {
        screw_boss(inner_h - 2);
    }
}

// ============================================================
// LID MODULE
// ============================================================
module lid() {
    difference() {
        union() {
            // Main lid plate
            rounded_box(outer_l, outer_w, lid_h, corner_r);
            
            // Inner lip for alignment
            translate([wall + 2.4, wall + 2.4, -2])
                rounded_box(outer_l - wall*2 - 4.8, outer_w - wall*2 - 4.8, 2.01, corner_r - 3.2);
        }
        
        // Ventilation slots on lid
        lid_ventilation();
        
        // Screw holes
        screw_boss_positions() {
            translate([0, 0, -5])
                cylinder(h = 10, d = 3.2, $fn = 20);
        }
    }
}

// ============================================================
// HELPER MODULES
// ============================================================

// Rounded box (2D hull extruded)
module rounded_box(l, w, h, r) {
    r2 = min(r, min(l/2, w/2));
    translate([r2, r2, 0])
        linear_extrude(h)
            offset(r = r2)
                square([l - r2*2, w - r2*2]);
}

// Mounting post with screw hole
module mount_post(x, y, h, d) {
    translate([x, y, 0]) {
        difference() {
            // Post body
            cylinder(h = h, d = d + 4, $fn = 24);
            // Screw hole
            translate([0, 0, -0.5])
                cylinder(h = h + 1, d = d, $fn = 20);
        }
    }
}

// DAC platform (raised pedestal)
module dac_platform() {
    platform_h = 4;
    difference() {
        // Platform base
        translate([0, 0, 0])
            cube([dac_l, dac_w, platform_h]);
        // Center cutout (weight saving)
        translate([4, 4, -0.5])
            cube([dac_l - 8, dac_w - 8, platform_h + 1]);
    }
    // Mount posts on platform (front side)
    translate([3, 3, 0])
        mount_post(0, 0, platform_h + 3, dac_mount_d);
    translate([dac_l - 3, 3, 0])
        mount_post(0, 0, platform_h + 3, dac_mount_d);
}

// Corner reinforcement gusset
module corner_gusset(x, y) {
    gusset_size = 8;
    gusset_h = 10;
    
    translate([x, y, 0]) {
        // Determine which corner
        sx = (x < inner_l/2) ? 1 : -1;
        sy = (y < inner_w/2) ? 1 : -1;
        
        translate([sx < 0 ? -gusset_size : 0, sy < 0 ? -gusset_size : 0, 0])
            difference() {
                cube([gusset_size, gusset_size, gusset_h]);
                translate([sx > 0 ? gusset_size : 0, sy > 0 ? gusset_size : 0, -0.5])
                    cylinder(h = gusset_h + 1, r = gusset_size, $fn = 30);
            }
    }
}

// Show PCBs for preview
show_pcbs = false;

// Cable routing slots
module cable_slots() {
    slot_h = 14; // Standard height for cables
    slot_z = floor_h + 8;
    
    // Front side - Relay terminals (4 segmented slots)
    for (i = [0:3]) {
        translate([wall + relay_x + 12 + i * 38, -1, slot_z])
            cube([28, wall + 2, slot_h]);
    }
        
    // Right side - DAC output and LM2596 power input (Separate slots)
    // LM2596 Power input slot
    translate([outer_l - wall - 1, wall + 10, slot_z])
        cube([wall + 2, 18, slot_h]);
    // DAC output slot
    translate([outer_l - wall - 1, wall + dac_y + 5, slot_z])
        cube([wall + 2, 22, slot_h]);
}

module dummy_pcbs() {
    // Relay Board
    color("darkgreen", 0.7)
    translate([wall + relay_x, wall + relay_y, floor_h + 5])
        cube([relay_l, relay_w, 20]);
        
    // LM2596 (Rotated 90 degrees)
    color("blue", 0.7)
    translate([wall + lm2596_x, wall + lm2596_y, floor_h + 5])
        cube([lm2596_w, lm2596_l, 14]);
        
    // DAC
    color("red", 0.7)
    translate([wall + dac_x, wall + dac_y, floor_h + 5 + 4])
        cube([dac_l, dac_w, 12]);
}

// Ventilation slots on sides
module ventilation_slots() {
    // Front ventilation
    vent_count = 8;
    vent_w = 2;
    vent_spacing = (outer_l - 40) / vent_count;
    
    for (i = [0:vent_count-1]) {
        // Front
        translate([20 + i * vent_spacing, -1, floor_h + inner_h - 8])
            cube([vent_w, wall + 2, 6]);
        // Back  
        translate([20 + i * vent_spacing, outer_w - wall - 1, floor_h + inner_h - 8])
            cube([vent_w, wall + 2, 6]);
    }
    
    // Side ventilation
    vent_count_side = 6;
    vent_spacing_side = (outer_w - 30) / vent_count_side;
    
    for (i = [0:vent_count_side-1]) {
        // Left side
        translate([-1, 15 + i * vent_spacing_side, floor_h + inner_h - 6])
            cube([wall + 2, vent_w, 4]);
        // Right side
        translate([outer_l - wall - 1, 15 + i * vent_spacing_side, floor_h + inner_h - 6])
            cube([wall + 2, vent_w, 4]);
    }
}

// Lid ventilation pattern
module lid_ventilation() {
    // Hex pattern ventilation over relay area
    hex_r = 3;
    hex_spacing = 9;
    
    for (x = [wall + 15 : hex_spacing : wall + relay_l - 10]) {
        for (y = [wall + 15 : hex_spacing : wall + relay_w - 10]) {
            offset_y = (floor((x - wall) / hex_spacing) % 2 == 0) ? 0 : hex_spacing / 2;
            translate([x, y + offset_y, -1])
                cylinder(h = lid_h + 2, r = hex_r, $fn = 6);
        }
    }
}

// Screw boss positions (4 corners)
module screw_boss_positions() {
    inset = 8;
    positions = [
        [wall + inset, wall + inset, floor_h],
        [outer_l - wall - inset, wall + inset, floor_h],
        [wall + inset, outer_w - wall - inset, floor_h],
        [outer_l - wall - inset, outer_w - wall - inset, floor_h]
    ];
    
    for (pos = positions) {
        translate(pos) children();
    }
}

// Screw boss cylinder
module screw_boss(h) {
    difference() {
        cylinder(h = h, d = 8, $fn = 24);
        translate([0, 0, h - 8])
            cylinder(h = 9, d = 2.8, $fn = 20); // Self-tapping screw hole
    }
}
