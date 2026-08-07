#![no_main]

use libfuzzer_sys::fuzz_target;
use mosaic_pack::PackHeaderV0;

fuzz_target!(|data: &[u8]| {
    let _ = PackHeaderV0::parse(data);
});
