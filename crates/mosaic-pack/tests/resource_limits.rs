use mosaic_pack::{
    DfaView, ManifestPackRef, PackError, PackHash, PackRole, PackValidationLimits, PackView,
    TokenizerManifestV1,
};

#[test]
fn outer_pack_limits_fail_before_execution() {
    let bytes = include_bytes!("../../../fixtures/packs/m2-v1.mpack");
    let too_small = PackValidationLimits { max_file_bytes: 100, ..PackValidationLimits::DEFAULT };
    assert!(matches!(PackView::parse(bytes, too_small), Err(PackError::ResourceLimitExceeded)));

    let too_few_sections = PackValidationLimits { max_sections: 2, ..PackValidationLimits::DEFAULT };
    assert!(matches!(PackView::parse(bytes, too_few_sections), Err(PackError::ResourceLimitExceeded)));
}

#[test]
fn dependency_and_dfa_limits_are_enforced() {
    let bytes = include_bytes!("../../../fixtures/packs/m2-v1.mpack");
    let pack = PackView::parse(bytes, PackValidationLimits::DEFAULT).expect("outer pack");

    let no_dependencies = PackValidationLimits { max_dependencies: 0, ..PackValidationLimits::DEFAULT };
    assert_eq!(pack.validate_canonical_metadata(no_dependencies), Err(PackError::ResourceLimitExceeded));

    let one_state = PackValidationLimits { max_dfa_states: 1, ..PackValidationLimits::DEFAULT };
    assert!(matches!(
        DfaView::parse(pack.section_bytes(2).expect("dfa"), one_state),
        Err(PackError::ResourceLimitExceeded)
    ));
}

#[test]
fn tokenizer_manifest_pack_count_is_bounded() {
    let hash = PackHash([1; 32]);
    let packs = [
        ManifestPackRef { role: PackRole::Unicode, ordinal: 0, content_hash: hash },
        ManifestPackRef { role: PackRole::Model, ordinal: 0, content_hash: PackHash([2; 32]) },
    ];
    let limits = PackValidationLimits { max_manifest_packs: 1, ..PackValidationLimits::DEFAULT };
    assert!(matches!(
        TokenizerManifestV1::new(1, 1, 1, 1, 1, 1, 0, PackHash([3; 32]), &packs, limits),
        Err(PackError::ResourceLimitExceeded)
    ));
}
