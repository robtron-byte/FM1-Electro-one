use mks_engine::{parse_preset, Synth};
fn walk(d: &std::path::Path, out: &mut Vec<std::path::PathBuf>) {
    for e in std::fs::read_dir(d).unwrap() { let p = e.unwrap().path(); if p.is_dir() { walk(&p, out) } else if p.extension().map(|x| x == "mks").unwrap_or(false) { out.push(p) } }
}
fn main() {
    let root = std::path::PathBuf::from(std::env::args().nth(1).unwrap());
    let mut files = Vec::new(); walk(&root, &mut files);
    let mut rel: Vec<String> = files.iter().map(|p| p.strip_prefix(&root).unwrap().to_string_lossy().to_string()).collect();
    rel.sort_by(|a, b| a.as_bytes().cmp(b.as_bytes()));
    for r in rel {
        let xml = std::fs::read_to_string(root.join(&r)).unwrap();
        let p = match parse_preset(&xml) { Some(p) => p, None => continue };
        let mut s = Synth::new(48000.0); s.load_preset(&p);
        let mut l = vec![0.0f32; 512]; let mut rr = vec![0.0f32; 512];
        for _ in 0..4 { s.process(&mut l, &mut rr); }
        s.note_on(60, 100); s.note_on(64, 90);
        let mut h: u64 = 0xcbf29ce484222325;
        for b in 0..48 { if b == 24 { s.note_off(60); s.note_off(64); }
            s.process(&mut l, &mut rr);
            for k in 0..512 { for v in [l[k], rr[k]] { for by in v.to_le_bytes() { h ^= by as u64; h = h.wrapping_mul(0x100000001b3); } } } }
        println!("{:016x} {}", h, r);
    }
}
