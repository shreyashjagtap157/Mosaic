use mosaic_pack::{DfaView, PackHash, PackValidationLimits, PackView, SECTION_KIND_DFA};

const DEPENDENCY: PackHash = PackHash([
    0xe9, 0x73, 0xe5, 0xd7, 0xe8, 0xc5, 0x22, 0xeb, 0x89, 0x11, 0x6f, 0x21, 0x09, 0x0d, 0xfc, 0x8b,
    0x96, 0x57, 0xa2, 0x9b, 0xc2, 0x20, 0xa3, 0x70, 0x2e, 0x87, 0xf1, 0xbb, 0x32, 0x51, 0x52, 0x2c,
]);

#[test]
fn m2_fixture_has_valid_hash_manifest_lock_and_dfa() {
    let bytes = include_bytes!("../../../fixtures/packs/m2-v1.mpack");
    let limits = PackValidationLimits::DEFAULT;
    let pack = PackView::parse(bytes, limits).expect("M2 fixture must validate");
    pack.validate_canonical_metadata(limits)
        .expect("manifest must bind lock graph");
    assert_eq!(pack.header().format_major, 1);
    assert_eq!(pack.header().section_count, 3);
    assert_eq!(pack.lock_graph(limits).expect("lock graph").len(), 1);
    assert!(pack.validate_canonical_dependencies(limits, &[]).is_err());
    pack.validate_canonical_dependencies(limits, &[DEPENDENCY])
        .expect("exact dependency hash must satisfy lock");

    let dfa_section = pack.section(2).expect("DFA section entry");
    assert_eq!(dfa_section.kind, SECTION_KIND_DFA);
    let dfa = DfaView::parse(pack.section_bytes(2).expect("DFA bytes"), limits).expect("valid DFA");
    assert_eq!(dfa.state_count(), 2);
}

#[test]
fn one_byte_mutation_breaks_content_identity() {
    let original = include_bytes!("../../../fixtures/packs/m2-v1.mpack");
    let mut bytes = original.to_vec();
    let last = bytes.len() - 1;
    bytes[last] ^= 1;
    assert!(PackView::parse(&bytes, PackValidationLimits::DEFAULT).is_err());
}

#[test]
fn tokenizer_manifest_identity_is_canonical() {
    use mosaic_pack::{ManifestPackRef, PackRole, TokenizerManifestV1};

    let unicode = PackHash([
        0xe9, 0x73, 0xe5, 0xd7, 0xe8, 0xc5, 0x22, 0xeb, 0x89, 0x11, 0x6f, 0x21, 0x09, 0x0d, 0xfc,
        0x8b, 0x96, 0x57, 0xa2, 0x9b, 0xc2, 0x20, 0xa3, 0x70, 0x2e, 0x87, 0xf1, 0xbb, 0x32, 0x51,
        0x52, 0x2c,
    ]);
    let model = PackHash([
        0x69, 0x86, 0x5a, 0xf9, 0x7c, 0x99, 0x56, 0x50, 0x81, 0xf3, 0x93, 0x2f, 0x1b, 0x35, 0x38,
        0x4d, 0x82, 0x86, 0x69, 0xf3, 0x98, 0x1b, 0xb7, 0x96, 0xdf, 0x11, 0x59, 0x10, 0x65, 0xc7,
        0x49, 0x86,
    ]);
    let resource = PackHash([
        0x9d, 0xa0, 0x4b, 0x5f, 0x24, 0xe6, 0xb0, 0xac, 0x26, 0x4a, 0xeb, 0x71, 0x4b, 0x3f, 0x6a,
        0xa5, 0x96, 0x33, 0xac, 0x49, 0x59, 0xda, 0xf6, 0xd1, 0xdb, 0x5c, 0x8d, 0x96, 0x16, 0xd5,
        0x27, 0x94,
    ]);
    let packs = [
        ManifestPackRef {
            role: PackRole::Unicode,
            ordinal: 0,
            content_hash: unicode,
        },
        ManifestPackRef {
            role: PackRole::Model,
            ordinal: 0,
            content_hash: model,
        },
    ];
    let manifest = TokenizerManifestV1::new(
        1,
        1,
        1,
        1,
        1,
        1,
        0,
        resource,
        &packs,
        PackValidationLimits::DEFAULT,
    )
    .expect("manifest");
    assert_eq!(
        manifest.identity().0,
        [
            0xc6, 0xfc, 0x3e, 0x21, 0x7d, 0xff, 0x6a, 0xef, 0x17, 0xd5, 0x88, 0xa9, 0xed, 0x7c,
            0x12, 0xbf, 0x4d, 0xb1, 0xb7, 0xfc, 0xc2, 0x93, 0x82, 0x92, 0xf0, 0xa2, 0x86, 0x71,
            0x9e, 0xc6, 0x27, 0x64,
        ]
    );
}
