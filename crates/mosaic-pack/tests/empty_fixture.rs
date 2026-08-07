use mosaic_pack::PackHeaderV0;

#[test]
fn empty_fixture_is_structurally_valid() {
    let bytes = include_bytes!("../../../fixtures/packs/empty-v0.mpack");
    let header = PackHeaderV0::parse(bytes).expect("fixture must remain valid");
    assert_eq!(header.format_major, 0);
    assert_eq!(header.format_minor, 1);
    assert_eq!(header.section_count, 0);
    assert!(header.is_test_fixture());
}

#[test]
fn malformed_headers_fail_closed() {
    let fixture = include_bytes!("../../../fixtures/packs/empty-v0.mpack");

    assert!(PackHeaderV0::parse(&fixture[..8]).is_err());

    let mut bad_magic = *fixture;
    bad_magic[0] ^= 0xff;
    assert!(PackHeaderV0::parse(&bad_magic).is_err());

    let mut bad_len = *fixture;
    bad_len[16] = 31;
    assert!(PackHeaderV0::parse(&bad_len).is_err());

    let mut bad_reserved = *fixture;
    bad_reserved[28] = 1;
    assert!(PackHeaderV0::parse(&bad_reserved).is_err());
}
