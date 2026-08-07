#![no_main]

use libfuzzer_sys::fuzz_target;
use mosaic_pack::{DfaView, PackValidationLimits};

fuzz_target!(|data: &[u8]| {
    let _ = DfaView::parse(data, PackValidationLimits::DEFAULT);
});
