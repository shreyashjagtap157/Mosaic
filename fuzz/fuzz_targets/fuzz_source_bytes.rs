#![no_main]

use libfuzzer_sys::fuzz_target;
use mosaic_core::{BorrowedSource, ByteRange, Source};

fuzz_target!(|data: &[u8]| {
    let source = BorrowedSource::new(data);
    assert_eq!(source.len(), u64::try_from(data.len()).expect("length must fit u64"));
    assert_eq!(source.canonical_leaves().count(), data.len());

    let mut output = vec![0_u8; data.len()];
    source
        .read_exact(ByteRange::new(0, u64::try_from(data.len()).expect("length must fit u64")), &mut output)
        .expect("whole source must be readable");
    assert_eq!(output, data);
});
