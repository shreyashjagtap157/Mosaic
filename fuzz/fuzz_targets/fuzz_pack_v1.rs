#![no_main]

use libfuzzer_sys::fuzz_target;
use mosaic_pack::{PackValidationLimits, PackView};

fuzz_target!(|data: &[u8]| {
    if let Ok(pack) = PackView::parse(data, PackValidationLimits::DEFAULT) {
        let _ = pack.validate_canonical_metadata(PackValidationLimits::DEFAULT);
    }
});
