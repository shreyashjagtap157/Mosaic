use mosaic_core::{BorrowedSource, ByteRange, Source};

#[test]
fn every_byte_value_round_trips() {
    let bytes: Vec<u8> = (0_u8..=u8::MAX).collect();
    let source = BorrowedSource::new(&bytes);
    let mut output = vec![0_u8; bytes.len()];

    source
        .read_exact(
            ByteRange::new(0, u64::try_from(bytes.len()).expect("length must fit u64")),
            &mut output,
        )
        .expect("full range must be readable");

    assert_eq!(output, bytes);
}

#[test]
fn canonical_leaves_are_gapless_single_bytes() {
    let bytes = b"mosaic";
    let source = BorrowedSource::new(bytes);
    let leaves: Vec<_> = source.canonical_leaves().collect();

    assert_eq!(leaves.len(), bytes.len());
    for (index, leaf) in leaves.into_iter().enumerate() {
        let range = leaf.range();
        assert_eq!(
            range.start.0,
            u64::try_from(index).expect("index must fit u64")
        );
        assert_eq!(range.len.0, 1);
    }
}

#[test]
fn generated_buffers_round_trip() {
    let mut state = 0x4d59_5df4_d0f3_3173_u64;
    #[cfg(miri)]
    const LIMIT: usize = 256;
    #[cfg(not(miri))]
    const LIMIT: usize = 2048;

    for len in 0..LIMIT {
        let mut bytes = vec![0_u8; len];
        for byte in &mut bytes {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            *byte = state.to_le_bytes()[0];
        }

        let source = BorrowedSource::new(&bytes);
        let mut output = vec![0_u8; len];
        source
            .read_exact(
                ByteRange::new(0, u64::try_from(len).expect("length must fit u64")),
                &mut output,
            )
            .expect("generated buffer must round-trip");
        assert_eq!(output, bytes);
    }
}

#[test]
fn empty_source_has_no_leaves() {
    let source = BorrowedSource::new(b"");
    assert_eq!(source.canonical_leaves().count(), 0);
}

#[test]
fn range_errors_are_explicit() {
    let source = BorrowedSource::new(b"abc");
    let mut one = [0_u8; 1];

    assert!(source.read_exact(ByteRange::new(3, 1), &mut one).is_err());
    assert!(
        source
            .read_exact(ByteRange::new(u64::MAX, 2), &mut one)
            .is_err()
    );
}

#[test]
fn range_read_preserves_exact_subslice() {
    let source = BorrowedSource::new(b"0123456789");
    let mut output = [0_u8; 4];
    source
        .read_exact(ByteRange::new(3, 4), &mut output)
        .expect("valid range must be readable");
    assert_eq!(&output, b"3456");
}

#[test]
fn versioned_source_keeps_identity_outside_byte_semantics() {
    use mosaic_core::{SourceIdentity, VersionedSource};

    let source = VersionedSource::new(
        BorrowedSource::new(b"same bytes"),
        SourceIdentity::new(*b"0123456789abcdef", 7),
    );

    assert_eq!(source.len(), 10);
    assert_eq!(source.identity().version.0, 7);
    assert_eq!(source.canonical_leaves().count(), 10);
}

#[test]
fn checked_ranges_accept_the_largest_valid_empty_range() {
    assert!(
        ByteRange::new(u64::MAX, 0)
            .validate_for_len(u64::MAX)
            .is_ok()
    );
}

#[test]
fn committed_all_bytes_golden_fixture_is_exact() {
    let bytes = include_bytes!("../../../fixtures/golden/all-bytes.bin");
    assert_eq!(bytes.len(), 256);
    for (index, byte) in bytes.iter().copied().enumerate() {
        assert_eq!(usize::from(byte), index);
    }
    let source = BorrowedSource::new(bytes);
    assert_eq!(source.canonical_leaves().count(), 256);
}
