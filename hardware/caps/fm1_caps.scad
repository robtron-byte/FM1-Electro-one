// FM1-Electro: 3D-printable knobs and button caps for the M-VAVE FM-1.
// All sizes in mm. Measure your FM-1 (see README.md) and set the values
// below, then render the part you need (part = "...") and export STL.
// The defaults are common sizes, NOT measured on an FM-1.

part = "knob_large";   // knob_large | knob_small | button_cap | fit_test

/* [Encoder shaft] */
shaft_d      = 6.0;    // shaft diameter
shaft_flat   = 4.5;    // D-shaft: distance from the flat to the far side (= shaft_d for a round shaft)
shaft_depth  = 10.0;   // how deep the knob sits on the shaft
fit          = 0.15;   // extra clearance; raise if too tight, lower if loose

/* [Knobs] */
large_d = 16;   large_h = 15;    // KNOB 1-4 (sound)
small_d = 13;   small_h = 13;    // SELECT, ALGORITHM, PRESETS (structure)
flutes  = 24;   flute_depth = 0.6;
pointer_w = 0.9; pointer_depth = 0.7;

/* [Button caps (slip over the existing buttons)] */
btn_w = 10.4;   btn_l = 5.4;     // size of the existing button
btn_h = 2.0;                     // how far the cap reaches down over it
btn_r = 1.2;                     // corner radius of the existing button
wall  = 0.8;    top = 1.2;       // cap wall and top thickness
btn_fit = 0.15;                  // clearance around the button

$fn = 96;

module rounded_rect(w, l, r, h) {
    linear_extrude(h) offset(r) square([w - 2 * r, l - 2 * r], center = true);
}

module d_bore() {
    d = shaft_d + 2 * fit;
    translate([0, 0, -0.01])
    intersection() {
        cylinder(d = d, h = shaft_depth + 0.01);
        // keep everything up to the flat
        translate([-d / 2, -d / 2, 0]) cube([d, shaft_flat + fit - shaft_d / 2 + d / 2, shaft_depth + 0.02]);
    }
}

module knob(d, h) {
    difference() {
        // body with a small chamfer at the top
        union() {
            cylinder(d = d, h = h - 1);
            translate([0, 0, h - 1]) cylinder(d1 = d, d2 = d - 2, h = 1);
        }
        // grip flutes
        for (i = [0 : flutes - 1]) rotate(i * 360 / flutes)
            translate([d / 2 + 0.6 - flute_depth, 0, 1.5]) cylinder(d = 1.2, h = h, $fn = 16);
        // pointer: groove across the top and down the side
        translate([-pointer_w / 2, 0, h - pointer_depth]) cube([pointer_w, d / 2 + 1, pointer_depth + 1]);
        translate([-pointer_w / 2, d / 2 - pointer_depth, h * 0.45]) cube([pointer_w, 2, h]);
        d_bore();
    }
}

module button_cap() {
    iw = btn_w + 2 * btn_fit, il = btn_l + 2 * btn_fit;
    difference() {
        rounded_rect(iw + 2 * wall, il + 2 * wall, btn_r + wall, btn_h + top);
        translate([0, 0, -0.01]) rounded_rect(iw, il, btn_r + btn_fit, btn_h + 0.01);
    }
}

// Bores from 5.8 to 6.4 mm in one strip: push it onto a shaft to find the
// clearance that holds. The number of dots tells which bore fits.
module fit_test() {
    sizes = [5.8, 5.9, 6.0, 6.1, 6.2, 6.3, 6.4];
    difference() {
        rounded_rect(len(sizes) * 10 + 4, 14, 2, 8);
        for (i = [0 : len(sizes) - 1]) {
            x = (i - (len(sizes) - 1) / 2) * 10;
            translate([x, -1.5, 0]) intersection() {
                translate([0, 0, -0.01]) cylinder(d = sizes[i], h = 8.02);
                translate([-sizes[i] / 2, -sizes[i] / 2, -0.01]) cube([sizes[i], shaft_flat - shaft_d / 2 + sizes[i] / 2 + (sizes[i] - shaft_d) / 2, 8.02]);
            }
            for (k = [0 : i]) translate([x - 3 + k * 1, 5, 7.4]) cylinder(d = 0.7, h = 1, $fn = 12);
        }
    }
}

if (part == "knob_large") knob(large_d, large_h);
if (part == "knob_small") knob(small_d, small_h);
if (part == "button_cap") button_cap();
if (part == "fit_test") fit_test();
