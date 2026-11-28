// Gegenstück zum C-Test (bausteine/mksultra/test/test.c) auf dem Rust-Kern der Vorlage.
//   sweep <presets>        eine FNV-1a-Prüfsumme je Preset (kurzer Ablauf, wie `test --sweep`)
//   sweep <presets> ron    Kennwerte der Ron-Presets (wie `test` Abschnitt Ron): fnv rms peak Pfad -> ref/ron.txt
use mks_engine::{parse_preset, Synth};

fn walk(d: &std::path::Path, out: &mut Vec<std::path::PathBuf>) {
    for e in std::fs::read_dir(d).unwrap() {
        let p = e.unwrap().path();
        if p.is_dir() {
            walk(&p, out)
        } else if p.extension().map(|x| x == "mks").unwrap_or(false) {
            out.push(p)
        }
    }
}

fn fnv(h: &mut u64, v: f32) {
    for by in v.to_le_bytes() {
        *h ^= by as u64;
        *h = h.wrapping_mul(0x100000001b3);
    }
}

/// Ron-Ablauf: 20×512 Vorlauf, Akkord 48 55 60 64 (v100), 1 s halten, 1 s loslassen, 48 kHz, Blöcke à 512.
fn ron(s: &mut Synth) -> (u64, f64, f32) {
    let (len, rel) = (96000usize, 48000usize);
    let notes = [48u8, 55, 60, 64];
    let mut l = vec![0.0f32; 512];
    let mut r = vec![0.0f32; 512];
    for _ in 0..20 {
        s.process(&mut l, &mut r);
    }
    for &n in &notes {
        s.note_on(n, 100);
    }
    let (mut h, mut sq, mut pk) = (0xcbf29ce484222325u64, 0.0f64, 0.0f32);
    let mut pos = 0;
    while pos < len {
        if pos == rel {
            for &n in &notes {
                s.note_off(n);
            }
        }
        let mut n = 512.min(len - pos);
        if pos < rel {
            n = n.min(rel - pos);
        }
        s.process(&mut l[..n], &mut r[..n]);
        for k in 0..n {
            for v in [l[k], r[k]] {
                fnv(&mut h, v);
                sq += (v as f64) * (v as f64);
                pk = pk.max(v.abs());
            }
        }
        pos += n;
    }
    (h, (sq / (2 * len) as f64).sqrt(), pk)
}

fn main() {
    let root = std::path::PathBuf::from(std::env::args().nth(1).unwrap());
    let ron_mode = std::env::args().nth(2).map(|a| a == "ron").unwrap_or(false);
    let mut files = Vec::new();
    walk(&root, &mut files);
    let mut rel: Vec<String> =
        files.iter().map(|p| p.strip_prefix(&root).unwrap().to_string_lossy().replace('\\', "/")).collect();
    rel.sort_by(|a, b| a.as_bytes().cmp(b.as_bytes()));
    for r in rel {
        if ron_mode && !r.starts_with("Ron/") {
            continue;
        }
        let xml = std::fs::read_to_string(root.join(&r)).unwrap();
        let p = match parse_preset(&xml) {
            Some(p) => p,
            None => continue,
        };
        let mut s = Synth::new(48000.0);
        s.load_preset(&p);
        if ron_mode {
            let (h, rms, pk) = ron(&mut s);
            println!("{:016x} {:.6} {:.6} {}", h, rms, pk, r);
            continue;
        }
        let mut l = vec![0.0f32; 512];
        let mut rr = vec![0.0f32; 512];
        for _ in 0..4 {
            s.process(&mut l, &mut rr);
        }
        s.note_on(60, 100);
        s.note_on(64, 90);
        let mut h: u64 = 0xcbf29ce484222325;
        for b in 0..48 {
            if b == 24 {
                s.note_off(60);
                s.note_off(64);
            }
            s.process(&mut l, &mut rr);
            for k in 0..512 {
                fnv(&mut h, l[k]);
                fnv(&mut h, rr[k]);
            }
        }
        println!("{:016x} {}", h, r);
    }
}
