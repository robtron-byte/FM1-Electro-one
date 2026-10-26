// M-VAVE FM-1: dimensions of the housing and the controls, measured from
// FM1.obj / FM1.stp (see MASSE.md, same numbers as fm1_dimensions.json).
// Units mm. Origin: front left bottom corner, x right, y back (ports), z up.
//
// Use it in your own file:
//   use <fm1_panel.scad>      // modules only
//   include <fm1_panel.scad>  // modules and the variables (then set show = "none")
//
// Rendered on its own it shows an example: a 1 mm faceplate with cutouts.

/* [Example] */
show = "overlay";   // overlay | controls | reference | none
plate_t = 1.0;      // faceplate thickness (keys stick out 1.5, option buttons 1.26 above the panel)
clear = 0.4;        // gap around every control

$fn = 64;

// ---- Housing ----
fm1_size      = [162, 97];
fm1_corner_r  = 10;      // plan view
fm1_edge_r    = 2;       // round top edge
fm1_z_panel   = 21.5;    // panel top
fm1_z_shell   = 6;       // underside of the housing shell (tray and feet below)
fm1_panel_t   = 2;

// ---- Recesses in the panel (0.5 deep, floor at z 21.0): [center, size, radius]
fm1_pocket_depth = 0.5;
fm1_pockets = [
    [[81.0, 24.25],  [148.6, 35.1], 5.3],   // keys
    [[118.15, 58.65], [63.9, 22.5], 3.3],   // option buttons
    [[22.6, 51.3],   [25.2, 8.0],   1.5],   // octave buttons
    [[61.1, 70.2],   [38.2, 38.2],  5.0]    // display
];

// ---- 27 keys, stadium shape, long side along y
fm1_key_base = [7.9, 14.9];
fm1_key_top  = [5.9, 12.9];
fm1_key_z    = [21.5, 23.0];
fm1_keys_lower_y = 16.5;
fm1_keys_lower_x = [13.4, 22.4, 31.4, 40.4, 49.4, 58.4, 67.4, 76.4, 85.4, 94.4, 103.4, 112.4, 121.4, 130.4, 139.4, 148.4];
fm1_keys_upper_y = 31.5;
fm1_keys_upper_x = [17.9, 26.9, 35.9, 53.9, 62.9, 80.9, 89.9, 98.9, 116.9, 125.9, 143.9];
fm1_led_slot = [1.2, 8.2];   // in the middle of every key top

// ---- 12 option buttons (6 x 2) and 2 octave buttons
fm1_opt_base = [8.4, 8.4];
fm1_opt_top  = [6.37, 6.37];
fm1_opt_z    = [21.5, 22.76];
fm1_opt_x    = [93.15, 103.15, 113.15, 123.15, 133.15, 143.15];
fm1_opt_y    = [53.65, 63.65];
fm1_oct_base = [10.0, 5.6];
fm1_oct_top  = [8.36, 3.96];
fm1_oct_z    = [21.5, 23.0];
fm1_oct      = [[16.4, 51.3], [28.8, 51.3]];

// ---- Display
fm1_glass       = [[46.825, 55.9], [28.5, 28.5]];   // [front left corner, size]
fm1_screen_frame = [[42.4, 51.5], [37.4, 37.4]];

// ---- Encoders and MASTER: knurled shafts in holes through the panel
fm1_enc_hole_d   = 7.9;      // 8.5 at the top chamfer
fm1_enc_collar_d = 6.0;      // z 19.5 to 22.75
fm1_enc_knurl_d  = 5.88;     // outer, 20 teeth, root 5.08, z 23.0 to 27.75
fm1_enc_z_top    = 28.2;
fm1_encoders = [             // [name, position]
    ["MASTER",    [14, 81]],
    ["SELECT",    [31, 81]],
    ["PRESETS",   [14, 64]],
    ["ALGORITHM", [31, 64]],
    ["KNOB_1",    [91, 81]],
    ["KNOB_2",    [109, 81]],
    ["KNOB_3",    [127, 81]],
    ["KNOB_4",    [145, 81]]
];

// ---- Bottom
fm1_feet   = [[15, 15], [15, 82], [147, 82], [147, 15]];
fm1_foot_d = 13.1;

// ---- Back (port side, y = 97): [x, z] centers
fm1_back_recess = [[27.1, 8.3], [54.4, 8.7]];   // [corner, size], 1.5 deep
fm1_usb_c  = [[50.0, 12.65], [9.0, 3.3]];
fm1_jacks  = [[62.75, 12.55], [75.05, 12.55]];  // both Ø 3.9
fm1_jack_d = 3.9;
fm1_switch = [[34.0, 10.8], [6.2, 3.6]];        // slot [corner, size]
fm1_led    = [29.85, 12.65];                    // Ø 0.9

// ================= 2D helpers =================

module fm1_rrect(c, s, r) {
    r2 = min(r, s[0] / 2, s[1] / 2);
    translate(c) offset(r2) square([s[0] - 2 * r2, s[1] - 2 * r2], center = true);
}

module fm1_stadium(c, s) {   // rounded ends along y
    hull() {
        translate([c[0], c[1] - s[1] / 2 + s[0] / 2]) circle(d = s[0]);
        translate([c[0], c[1] + s[1] / 2 - s[0] / 2]) circle(d = s[0]);
    }
}

// Plan outline of the housing
module fm1_outline() { fm1_rrect(fm1_size / 2, fm1_size, fm1_corner_r); }

// Footprints of all controls at their base, grown by g
module fm1_controls(g = 0) {
    offset(delta = g) {
        for (x = fm1_keys_lower_x) fm1_stadium([x, fm1_keys_lower_y], fm1_key_base);
        for (x = fm1_keys_upper_x) fm1_stadium([x, fm1_keys_upper_y], fm1_key_base);
        for (x = fm1_opt_x, y = fm1_opt_y) fm1_rrect([x, y], fm1_opt_base, 1);
        for (c = fm1_oct) fm1_rrect(c, fm1_oct_base, 1);
        translate(fm1_glass[0]) square(fm1_glass[1]);
        for (e = fm1_encoders) translate(e[1]) circle(d = fm1_enc_collar_d);
    }
}

// Mat and display recesses
module fm1_pocket_outlines(g = 0) {
    for (p = fm1_pockets) offset(delta = g) fm1_rrect(p[0], p[1], p[2]);
}

// ================= 3D =================

// Simplified reference body: housing up to the panel top plus the controls
module fm1_reference() {
    color("dimgray") difference() {
        translate([0, 0, fm1_z_shell]) linear_extrude(fm1_z_panel - fm1_z_shell) fm1_outline();
        translate([0, 0, fm1_z_panel - fm1_pocket_depth]) linear_extrude(1) fm1_pocket_outlines();
        for (e = fm1_encoders) translate([e[1][0], e[1][1], fm1_z_panel - 3]) cylinder(d = fm1_enc_hole_d, h = 4);
    }
    color("black") {
        for (x = fm1_keys_lower_x) translate([0, 0, fm1_key_z[0]]) linear_extrude(fm1_key_z[1] - fm1_key_z[0]) fm1_stadium([x, fm1_keys_lower_y], fm1_key_top);
        for (x = fm1_keys_upper_x) translate([0, 0, fm1_key_z[0]]) linear_extrude(fm1_key_z[1] - fm1_key_z[0]) fm1_stadium([x, fm1_keys_upper_y], fm1_key_top);
        for (x = fm1_opt_x, y = fm1_opt_y) translate([0, 0, fm1_opt_z[0]]) linear_extrude(fm1_opt_z[1] - fm1_opt_z[0]) fm1_rrect([x, y], fm1_opt_top, 0.8);
        for (c = fm1_oct) translate([0, 0, fm1_oct_z[0]]) linear_extrude(fm1_oct_z[1] - fm1_oct_z[0]) fm1_rrect(c, fm1_oct_top, 0.6);
        for (e = fm1_encoders) translate([e[1][0], e[1][1], fm1_z_panel - 2]) cylinder(d = fm1_enc_knurl_d, h = fm1_enc_z_top - fm1_z_panel + 2);
    }
}

// Example part: faceplate lying on the panel, with cutouts around every control
module fm1_overlay(t = plate_t, g = clear) {
    translate([0, 0, fm1_z_panel]) linear_extrude(t) difference() {
        offset(delta = -fm1_edge_r) fm1_outline();
        fm1_controls(g);
        for (e = fm1_encoders) translate(e[1]) circle(d = fm1_enc_hole_d + 2 * g);
    }
}

if (show == "overlay") fm1_overlay();
else if (show == "controls") linear_extrude(1) fm1_controls(clear);
else if (show == "reference") fm1_reference();
