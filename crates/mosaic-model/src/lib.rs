#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;

use alloc::vec::Vec;
use mosaic_ir::{NamespaceId, ProjectedToken, TokenId, TokenKind};
use mosaic_pack::{
    PackError, PackValidationLimits, PackView, SECTION_KIND_VOCABULARY, VocabularyView,
};

pub const MODEL_NAMESPACE: NamespaceId = NamespaceId(1);
pub const MODEL_KIND: TokenKind = TokenKind(0);

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct EncodedToken {
    pub start: usize,
    pub end: usize,
    pub token_id: TokenId,
    pub cost: i32,
}

impl EncodedToken {
    /// Converts the encoded model token into the shared token projection.
    ///
    /// # Errors
    ///
    /// Returns `ModelError` if the token span cannot be represented in Mosaic's
    /// byte-coordinate types.
    pub fn projected(self) -> Result<ProjectedToken, ModelError> {
        token(self.start, self.end, self.token_id.0)
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ModelError {
    Pack(PackError),
    InvalidVocabularySectionCount,
    UnknownTokenId,
    InputTooLarge,
    CostOverflow,
}

impl From<PackError> for ModelError {
    fn from(value: PackError) -> Self {
        Self::Pack(value)
    }
}

/// Extracts the single vocabulary section from a pack.
///
/// # Errors
///
/// Returns `ModelError` if the pack has zero or multiple vocabulary sections,
/// or if the selected vocabulary section is malformed.
pub fn vocabulary_from_pack(
    pack: PackView<'_>,
    limits: PackValidationLimits,
) -> Result<VocabularyView<'_>, ModelError> {
    let mut found = None;
    for index in 0..pack.header().section_count {
        if pack.section(index)?.kind == SECTION_KIND_VOCABULARY {
            if found.is_some() {
                return Err(ModelError::InvalidVocabularySectionCount);
            }
            found = Some(VocabularyView::parse(pack.section_bytes(index)?, limits)?);
        }
    }
    found.ok_or(ModelError::InvalidVocabularySectionCount)
}

/// Decodes model token IDs back to source bytes.
///
/// # Errors
///
/// Returns `ModelError` if any token ID is unknown or a vocabulary lookup fails.
pub fn decode_ids(vocabulary: VocabularyView<'_>, ids: &[TokenId]) -> Result<Vec<u8>, ModelError> {
    let mut output = Vec::new();
    for id in ids {
        let entry = vocabulary
            .entry_by_token_id(id.0)?
            .ok_or(ModelError::UnknownTokenId)?;
        output.extend_from_slice(entry.surface);
    }
    Ok(output)
}

/// Builds a model-token projection over a source byte span.
///
/// # Errors
///
/// Returns `ModelError` if the span is inverted or cannot fit Mosaic's
/// byte-coordinate types.
pub fn token(start: usize, end: usize, id: u32) -> Result<ProjectedToken, ModelError> {
    let start = u64::try_from(start).map_err(|_| ModelError::InputTooLarge)?;
    let len = u64::try_from(
        end.checked_sub(usize::try_from(start).map_err(|_| ModelError::InputTooLarge)?)
            .ok_or(ModelError::InputTooLarge)?,
    )
    .map_err(|_| ModelError::InputTooLarge)?;
    Ok(ProjectedToken {
        source: mosaic_core::ByteRange::new(start, len),
        namespace: MODEL_NAMESPACE,
        kind: MODEL_KIND,
        id: TokenId(id),
    })
}
