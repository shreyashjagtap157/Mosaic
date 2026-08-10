#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

use alloc::vec::Vec;
use mosaic_core::ByteRange;
use mosaic_pack::{
    PackError, PackValidationLimits, PackView, SECTION_KIND_UNICODE, UnicodeDataView,
};

const G_CONTROL: u8 = 1;
const G_LF: u8 = 2;
const G_CR: u8 = 3;
const G_EXTEND: u8 = 4;
const G_PREPEND: u8 = 5;
const G_SPACING_MARK: u8 = 6;
const G_L: u8 = 7;
const G_V: u8 = 8;
const G_T: u8 = 9;
const G_ZWJ: u8 = 10;
const G_LV: u8 = 11;
const G_LVT: u8 = 12;
const G_RI: u8 = 13;
const I_EXTEND: u8 = 1;
const I_CONSONANT: u8 = 2;
const I_LINKER: u8 = 3;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum UnicodeError {
    Pack(PackError),
    InvalidUnicodeSectionCount,
    InputTooLarge,
}
impl From<PackError> for UnicodeError {
    fn from(value: PackError) -> Self {
        Self::Pack(value)
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ScalarUnit {
    pub start: usize,
    pub end: usize,
    pub scalar: Option<u32>,
}

/// Extracts the single Unicode section from a pack.
///
/// # Errors
///
/// Returns `UnicodeError` if the pack has zero or multiple Unicode sections, or
/// if the selected Unicode section is malformed.
pub fn unicode_from_pack(
    pack: PackView<'_>,
    limits: PackValidationLimits,
) -> Result<UnicodeDataView<'_>, UnicodeError> {
    let mut found = None;
    for index in 0..pack.header().section_count {
        if pack.section(index)?.kind == SECTION_KIND_UNICODE {
            if found.is_some() {
                return Err(UnicodeError::InvalidUnicodeSectionCount);
            }
            found = Some(UnicodeDataView::parse(pack.section_bytes(index)?, limits)?);
        }
    }
    found.ok_or(UnicodeError::InvalidUnicodeSectionCount)
}

#[must_use]
pub fn decode_strict_utf8(input: &[u8]) -> Vec<ScalarUnit> {
    let mut out = Vec::new();
    let mut i = 0_usize;
    while i < input.len() {
        let b0 = input[i];
        if b0 <= 0x7f {
            out.push(ScalarUnit {
                start: i,
                end: i + 1,
                scalar: Some(u32::from(b0)),
            });
            i += 1;
            continue;
        }
        if (0xc2..=0xdf).contains(&b0) && i + 1 < input.len() && is_cont(input[i + 1]) {
            let cp = (u32::from(b0 & 0x1f) << 6) | u32::from(input[i + 1] & 0x3f);
            out.push(ScalarUnit {
                start: i,
                end: i + 2,
                scalar: Some(cp),
            });
            i += 2;
            continue;
        }
        if (0xe0..=0xef).contains(&b0) && i + 2 < input.len() {
            let b1 = input[i + 1];
            let b2 = input[i + 2];
            let valid_b1 = if b0 == 0xe0 {
                (0xa0..=0xbf).contains(&b1)
            } else if b0 == 0xed {
                (0x80..=0x9f).contains(&b1)
            } else {
                is_cont(b1)
            };
            if valid_b1 && is_cont(b2) {
                let cp = (u32::from(b0 & 0x0f) << 12)
                    | (u32::from(b1 & 0x3f) << 6)
                    | u32::from(b2 & 0x3f);
                out.push(ScalarUnit {
                    start: i,
                    end: i + 3,
                    scalar: Some(cp),
                });
                i += 3;
                continue;
            }
        }
        if (0xf0..=0xf4).contains(&b0) && i + 3 < input.len() {
            let b1 = input[i + 1];
            let b2 = input[i + 2];
            let b3 = input[i + 3];
            let valid_b1 = if b0 == 0xf0 {
                (0x90..=0xbf).contains(&b1)
            } else if b0 == 0xf4 {
                (0x80..=0x8f).contains(&b1)
            } else {
                is_cont(b1)
            };
            if valid_b1 && is_cont(b2) && is_cont(b3) {
                let cp = (u32::from(b0 & 0x07) << 18)
                    | (u32::from(b1 & 0x3f) << 12)
                    | (u32::from(b2 & 0x3f) << 6)
                    | u32::from(b3 & 0x3f);
                out.push(ScalarUnit {
                    start: i,
                    end: i + 4,
                    scalar: Some(cp),
                });
                i += 4;
                continue;
            }
        }
        // Invalid/truncated UTF-8 is preserved as a one-byte opaque unit. This
        // intentionally never substitutes U+FFFD into the authoritative source view.
        out.push(ScalarUnit {
            start: i,
            end: i + 1,
            scalar: None,
        });
        i += 1;
    }
    out
}

/// Computes extended grapheme-cluster spans for `input`.
///
/// # Errors
///
/// Returns `UnicodeError` if Unicode property lookups fail or the resulting
/// byte spans cannot be represented in Mosaic's coordinate types.
pub fn grapheme_spans(
    unicode: UnicodeDataView<'_>,
    input: &[u8],
) -> Result<Vec<ByteRange>, UnicodeError> {
    let units = decode_strict_utf8(input);
    if units.is_empty() {
        return Ok(Vec::new());
    }
    let mut gcb = Vec::with_capacity(units.len());
    let mut incb = Vec::with_capacity(units.len());
    let mut ep = Vec::with_capacity(units.len());
    for unit in &units {
        if let Some(cp) = unit.scalar {
            gcb.push(Some(unicode.grapheme_break(cp)?));
            incb.push(Some(unicode.indic_conjunct_break(cp)?));
            ep.push(unicode.is_extended_pictographic(cp)?);
        } else {
            gcb.push(None);
            incb.push(None);
            ep.push(false);
        }
    }
    let mut boundaries = Vec::with_capacity(units.len() + 1);
    boundaries.push(0_usize);
    for i in 1..units.len() {
        if should_break(&gcb, &incb, &ep, i) {
            boundaries.push(i);
        }
    }
    boundaries.push(units.len());
    let mut spans = Vec::with_capacity(boundaries.len().saturating_sub(1));
    for pair in boundaries.windows(2) {
        let start = units[pair[0]].start;
        let end = units[pair[1] - 1].end;
        spans.push(ByteRange::new(
            u64::try_from(start).map_err(|_| UnicodeError::InputTooLarge)?,
            u64::try_from(end - start).map_err(|_| UnicodeError::InputTooLarge)?,
        ));
    }
    Ok(spans)
}

fn should_break(gcb: &[Option<u8>], incb: &[Option<u8>], ep: &[bool], i: usize) -> bool {
    if gcb[i - 1].is_none() || gcb[i].is_none() {
        return true;
    }
    let a = gcb[i - 1].unwrap_or(0);
    let b = gcb[i].unwrap_or(0);
    if a == G_CR && b == G_LF {
        return false;
    }
    if matches!(a, G_CONTROL | G_CR | G_LF) {
        return true;
    }
    if matches!(b, G_CONTROL | G_CR | G_LF) {
        return true;
    }
    if a == G_L && matches!(b, G_L | G_V | G_LV | G_LVT) {
        return false;
    }
    if matches!(a, G_LV | G_V) && matches!(b, G_V | G_T) {
        return false;
    }
    if matches!(a, G_LVT | G_T) && b == G_T {
        return false;
    }
    if matches!(b, G_EXTEND | G_ZWJ) {
        return false;
    }
    if b == G_SPACING_MARK {
        return false;
    }
    if a == G_PREPEND {
        return false;
    }
    if incb[i] == Some(I_CONSONANT) && gb9c_no_break(incb, i) {
        return false;
    }
    if ep[i] && a == G_ZWJ && gb11_no_break(gcb, ep, i) {
        return false;
    }
    if a == G_RI && b == G_RI && preceding_ri_count(gcb, i) % 2 == 1 {
        return false;
    }
    true
}

fn gb9c_no_break(incb: &[Option<u8>], i: usize) -> bool {
    let mut j = i;
    let mut seen_linker = false;
    while j > 0 {
        j -= 1;
        match incb[j] {
            Some(I_EXTEND) => {}
            Some(I_LINKER) => seen_linker = true,
            Some(I_CONSONANT) => return seen_linker,
            _ => return false,
        }
    }
    false
}

fn gb11_no_break(gcb: &[Option<u8>], ep: &[bool], i: usize) -> bool {
    if i < 2 {
        return false;
    }
    let mut j = i - 2;
    loop {
        if gcb[j] != Some(G_EXTEND) {
            return ep[j];
        }
        if j == 0 {
            return false;
        }
        j -= 1;
    }
}

fn preceding_ri_count(gcb: &[Option<u8>], i: usize) -> usize {
    let mut count = 0;
    let mut j = i;
    while j > 0 {
        j -= 1;
        if gcb[j] == Some(G_RI) {
            count += 1;
        } else {
            break;
        }
    }
    count
}

const fn is_cont(value: u8) -> bool {
    value >= 0x80 && value <= 0xbf
}
