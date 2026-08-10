use mosaic_pack::{DfaView, PackError, PackValidationLimits, PackView};

fn parse(name: &str, bytes: &[u8], expected: PackError) {
    let actual = PackView::parse(bytes, PackValidationLimits::DEFAULT).expect_err(name);
    assert_eq!(actual, expected, "unexpected error for {name}");
}

#[test]
fn malformed_outer_container_fails_closed() {
    parse(
        "bad magic",
        include_bytes!("../../../fixtures/packs/malformed/bad-magic.mpack"),
        PackError::InvalidMagic,
    );
    parse(
        "bad file length",
        include_bytes!("../../../fixtures/packs/malformed/bad-file-length.mpack"),
        PackError::LengthMismatch,
    );
    parse(
        "bad content hash",
        include_bytes!("../../../fixtures/packs/malformed/bad-content-hash.mpack"),
        PackError::ContentHashMismatch,
    );
    parse(
        "reserved header bytes",
        include_bytes!("../../../fixtures/packs/malformed/header-reserved-nonzero.mpack"),
        PackError::ReservedNotZero,
    );
    parse(
        "unsupported pack flags",
        include_bytes!("../../../fixtures/packs/malformed/unsupported-pack-flags.mpack"),
        PackError::UnsupportedPackFlags,
    );
    parse(
        "unsupported section flags",
        include_bytes!("../../../fixtures/packs/malformed/unsupported-section-flags.mpack"),
        PackError::UnsupportedSectionFlags,
    );
    parse(
        "noncanonical padding",
        include_bytes!("../../../fixtures/packs/malformed/noncanonical-padding.mpack"),
        PackError::NonCanonicalPadding,
    );
    parse(
        "overlapping sections",
        include_bytes!("../../../fixtures/packs/malformed/overlapping-sections.mpack"),
        PackError::OverlappingOrUnsortedSections,
    );
    parse(
        "section out of bounds",
        include_bytes!("../../../fixtures/packs/malformed/section-out-of-bounds.mpack"),
        PackError::SectionOutOfBounds,
    );
}

#[test]
fn canonical_metadata_rejects_lock_tampering_and_invalid_identity() {
    let limits = PackValidationLimits::DEFAULT;
    let lock_hash = include_bytes!("../../../fixtures/packs/malformed/lock-hash-mismatch.mpack");
    let pack = PackView::parse(lock_hash, limits).expect("outer pack remains valid");
    assert_eq!(
        pack.validate_canonical_metadata(limits),
        Err(PackError::DependencyLockHashMismatch)
    );

    let invalid_utf8 =
        include_bytes!("../../../fixtures/packs/malformed/dependency-invalid-utf8.mpack");
    let pack = PackView::parse(invalid_utf8, limits).expect("outer pack remains valid");
    assert_eq!(
        pack.validate_canonical_metadata(limits),
        Err(PackError::InvalidIdentityUtf8)
    );

    let zero_hash = include_bytes!("../../../fixtures/packs/malformed/dependency-zero-hash.mpack");
    let pack = PackView::parse(zero_hash, limits).expect("outer pack remains valid");
    assert_eq!(
        pack.validate_canonical_metadata(limits),
        Err(PackError::ZeroDependencyHash)
    );

    let duplicate =
        include_bytes!("../../../fixtures/packs/malformed/dependency-duplicate-identity.mpack");
    let pack = PackView::parse(duplicate, limits).expect("outer pack remains valid");
    assert_eq!(
        pack.validate_canonical_metadata(limits),
        Err(PackError::DuplicateDependencyIdentity)
    );
}

#[test]
fn dfa_validation_rejects_out_of_range_state() {
    let limits = PackValidationLimits::DEFAULT;
    let bytes = include_bytes!("../../../fixtures/packs/malformed/dfa-state-out-of-bounds.mpack");
    let pack = PackView::parse(bytes, limits).expect("outer pack remains valid");
    let dfa = pack.section_bytes(2).expect("DFA section");
    assert!(matches!(
        DfaView::parse(dfa, limits),
        Err(PackError::DfaStateOutOfBounds)
    ));

    let flagged = include_bytes!("../../../fixtures/packs/malformed/dfa-transition-flags.mpack");
    let pack = PackView::parse(flagged, limits).expect("outer flagged pack remains valid");
    let dfa = pack.section_bytes(2).expect("DFA section");
    assert!(matches!(
        DfaView::parse(dfa, limits),
        Err(PackError::UnsupportedDfaTransitionFlags)
    ));
}
