// FM1-Electro: 3D-printable knobs and button caps for the M-VAVE FM-1.
// All sizes in mm. The defaults come from the CAD model of the FM-1
// (../fm1_model/MASSE.md): knurled encoder shafts with 20 teeth and the
// square silicone buttons right of the display. Print the fit test first,
// then render the part you need (part = "...") and export STL.

part = "knob_large";   // knob_large | knob_small | button_cap | fit_test

/* [Encoder shaft (knurled, 20 teeth)] */
knurl_teeth = 20;
knurl_od    = 5.88;    // over the teeth
knurl_rd    = 5.08;    // at the root
collar_d    = 6.0;     // smooth part of the shaft below the knurl
shaft_above = 6.7;     // shaft top above the panel
collar_above = 1.5;    // smooth part above the panel (knurl starts here)
knob_gap    = 0.5;     // knob stays this far above the panel
bore        = "knurl"; // knurl: teeth like the shaft | round: plain bore that clamps on the teeth
fit         = 0.10;    // knurl bore: radial clearance; round bore: added to knurl_rd
                       // (raise if too tight, lower if loose; the fit test shows the value)

/* [Knobs] */
large_d = 16;   large_h = 15;    // KNOB 1-4 (sound), shafts 18 apart
small_d = 13;   small_h = 13;    // SELECT, ALGORITHM, PRESETS (structure), shafts 17 apart
flutes  = 24;   flute_depth = 0.6;
pointer_w = 0.9; pointer_depth = 0.7;

/* [Button caps (12 square buttons right of the display)] */
btn_base  = 8.4;     // button size at the panel
btn_top   = 6.9;     // button size at the top
btn_h     = 1.26;    // button height above the panel
btn_r     = 1.0;     // corner radius of the button
btn_pitch = 10;      // button raster
btn_fit   = 0.1;     // clearance around the button
cap_gap   = 0.8;     // cap stays this far above the panel: more than the button travel
wall = 0.6;   top = 1.2;

$fn = 96;

shaft_depth = shaft_above - knob_gap + 0.2;   // bore depth, 0.2 deeper than the shaft
smooth_len  = collar_above - knob_gap;         // part of the bore around the smooth collar

module rounded_rect(w, l, r, h) {
    linear_extrude(h) offset(r) square([w - 2 * r, l - 2 * r], center = true);
}

// Profile of the knurled shaft grown by f: zig-zag, roots at 0°, tips half a tooth further
module knurl_profile(f) {
    n = knurl_teeth;
    polygon([for (i = [0 : 2 * n - 1])
        let(a = i * 180 / n, r = (i % 2 == 0 ? knurl_rd : knurl_od) / 2 + f) [r * cos(a), r * sin(a)]]);
}

module shaft_bore(f) {
    translate([0, 0, -0.01]) {
        cylinder(d = collar_d + 0.3, h = smooth_len + 0.02);
        translate([0, 0, smooth_len])
            if (bore == "knurl") linear_extrude(shaft_depth - smooth_len + 0.01) knurl_profile(f);
            else cylinder(d = knurl_rd + 2 * f, h = shaft_depth - smooth_len + 0.01);
        // lead-in so the knob finds the shaft
        cylinder(d1 = collar_d + 0.9, d2 = collar_d + 0.3, h = 0.3);
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
        shaft_bore(fit);
    }
}

// Cap over the upper part of a tapered button. It reaches down to cap_gap
// above the panel, so the button can still travel.
module button_cap() {
    reach = btn_h - cap_gap;
    s0 = btn_base - (btn_base - btn_top) * cap_gap / btn_h + 2 * btn_fit;   // cavity at the cap's lower edge
    s1 = btn_top + 2 * btn_fit;
    outer = s0 + 2 * wall;
    assert(outer < btn_pitch - 0.4, "cap too wide for the button raster");
    difference() {
        rounded_rect(outer, outer, btn_r + wall, reach + top);
        translate([0, 0, -0.01]) hull() {
            rounded_rect(s0, s0, btn_r, 0.01);
            translate([0, 0, reach]) rounded_rect(s1, s1, btn_r * s1 / s0, 0.01);
        }
    }
}

// Seven knurled bores with fit -0.10 to +0.20 in one strip: push it onto a
// shaft to find the one that holds. The number of dots tells which bore fits.
fits = [-0.10, -0.05, 0, 0.05, 0.10, 0.15, 0.20];
module fit_test() {
    difference() {
        rounded_rect(len(fits) * 10 + 4, 14, 2, 8);
        for (i = [0 : len(fits) - 1]) {
            x = (i - (len(fits) - 1) / 2) * 10;
            translate([x, -1.5, -0.01]) linear_extrude(8.02) knurl_profile(fits[i]);
            for (k = [0 : i]) translate([x - 3 + k * 1, 5, 7.4]) cylinder(d = 0.7, h = 1, $fn = 12);
        }
    }
}

if (part == "knob_large") knob(large_d, large_h);
if (part == "knob_small") knob(small_d, small_h);
if (part == "button_cap") button_cap();
if (part == "fit_test") fit_test();
