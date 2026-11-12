//! A small Rust library for tests/new/rust_*.fch: plain numbers, a #[repr(C)] struct, text in.
use std::ffi::CStr;
use std::os::raw::c_char;

#[repr(C)]
pub struct Point {
    pub x: i32,
    pub y: i32,
}

#[no_mangle]
pub extern "C" fn path_len(x: f32, y: f32) -> f32 {
    (x * x + y * y).sqrt()
}

#[no_mangle]
pub extern "C" fn manhattan(a: Point, b: Point) -> i32 {
    (a.x - b.x).abs() + (a.y - b.y).abs()
}

/// How many words the text has (a Vec and String from Rust's std, to show it links).
#[no_mangle]
pub extern "C" fn count_words(text: *const c_char) -> u32 {
    let s = unsafe { CStr::from_ptr(text) }.to_string_lossy().into_owned();
    let words: Vec<&str> = s.split_whitespace().collect();
    words.len() as u32
}
