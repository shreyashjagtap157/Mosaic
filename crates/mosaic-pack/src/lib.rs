#![no_std]
#![forbid(unsafe_code)]

mod dfa;
mod execution_manifest;
mod hash;
mod lock;
mod manifest;
mod unicode;
mod v1;
mod vocab;

pub use dfa::{DfaAccept, DfaTransition, DfaView};
pub use execution_manifest::{ManifestPackRef, PackRole, TokenizerManifestV1};
pub use hash::{PackHash, Sha256, sha256};
pub use lock::{LockGraphView, ResolvedPackIdentityRef};
pub use manifest::ManifestV1;
pub use unicode::{CodepointRange, PropertyRange, UnicodeDataView};
pub use v1::{HashAlgorithm, PackHeaderV1, PackView, SectionEntry};
pub use vocab::{VocabularyEntry, VocabularyView};

pub const MAGIC: [u8; 8] = *b"MOSPACK\0";
pub const M0_HEADER_LEN: usize = 32;
pub const FLAG_TEST_FIXTURE: u16 = 1;
pub const SECTION_KIND_MANIFEST: u32 = 1;
pub const SECTION_KIND_LOCK_GRAPH: u32 = 2;
pub const SECTION_KIND_DFA: u32 = 3;
pub const SECTION_KIND_VOCABULARY: u32 = 4;
pub const SECTION_KIND_UNICODE: u32 = 5;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PackValidationLimits {
    pub max_file_bytes: u64,
    pub max_sections: u32,
    pub max_section_bytes: u64,
    pub max_alignment_log2: u8,
    pub max_dependencies: u32,
    pub max_identity_component_bytes: u16,
    pub max_dfa_states: u32,
    pub max_dfa_transitions: u32,
    pub max_manifest_packs: u32,
    pub max_vocab_entries: u32,
    pub max_token_bytes: u32,
    pub max_unicode_ranges: u32,
}

impl PackValidationLimits {
    pub const DEFAULT: Self = Self {
        max_file_bytes: 1 << 30,
        max_sections: 4096,
        max_section_bytes: 1 << 28,
        max_alignment_log2: 20,
        max_dependencies: 1024,
        max_identity_component_bytes: 1024,
        max_dfa_states: 262_144,
        max_dfa_transitions: 1_000_000,
        max_manifest_packs: 4096,
        max_vocab_entries: 1_048_576,
        max_token_bytes: 1 << 20,
        max_unicode_ranges: 1_048_576,
    };
}

impl Default for PackValidationLimits {
    fn default() -> Self {
        Self::DEFAULT
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PackError {
    TooShort,
    InvalidMagic,
    UnsupportedFormatMajor,
    UnsupportedHeaderLength,
    UnsupportedSectionEntryLength,
    UnsupportedHashAlgorithm,
    HashAlgorithmNotImplemented,
    LengthMismatch,
    ReservedNotZero,
    ResourceLimitExceeded,
    IntegerOverflow,
    SectionDirectoryOutOfBounds,
    InvalidSectionIndex,
    InvalidSectionKind,
    MisalignedSection,
    OverlappingOrUnsortedSections,
    SectionOutOfBounds,
    InvalidElementLayout,
    ContentHashMismatch,
    MissingManifestSection,
    MissingLockSection,
    ManifestSectionKindMismatch,
    LockSectionKindMismatch,
    UnsupportedPackFlags,
    UnsupportedSectionFlags,
    NonCanonicalPadding,
    InvalidSingletonSectionCount,
    InvalidManifestLength,
    InvalidManifestMagic,
    UnsupportedManifestVersion,
    InvalidLockGraphLength,
    InvalidLockGraphMagic,
    UnsupportedLockGraphVersion,
    UnsupportedLockEntryLength,
    InvalidIdentityUtf8,
    ZeroDependencyHash,
    DuplicateDependencyIdentity,
    DependencyIndexOutOfBounds,
    DependencyLockHashMismatch,
    UnresolvedDependency,
    InvalidDfaHeader,
    UnsupportedDfaVersion,
    InvalidDfaLayout,
    DfaStateOutOfBounds,
    DfaTransitionsNotStrictlySorted,
    UnsupportedDfaTransitionFlags,
    DfaAcceptsNotStrictlySorted,
    DfaIndexOutOfBounds,
    CostOverflow,
    ZeroResourcePolicyHash,
    ManifestPacksNotCanonical,
    InvalidVocabularyHeader,
    UnsupportedVocabularyVersion,
    InvalidVocabularyLayout,
    UnsupportedVocabularyEntryFlags,
    InvalidVocabularySurface,
    VocabularyNotCanonical,
    InvalidVocabularyIdIndex,
    VocabularyTokenIdsNotUnique,
    InvalidVocabularyFirstByteIndex,
    MissingByteFallback,
    VocabularyIndexOutOfBounds,
    InvalidVocabularySectionCount,
    UnknownTokenId,
    InvalidUnicodeHeader,
    UnsupportedUnicodeVersion,
    InvalidUnicodeLayout,
    InvalidUnicodeRange,
    UnicodeRangeIndexOutOfBounds,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PackHeaderV0 {
    pub format_major: u16,
    pub format_minor: u16,
    pub header_len: u16,
    pub flags: u16,
    pub file_len: u64,
    pub section_count: u32,
}

impl PackHeaderV0 {
    /// Parses a legacy M0 pack header.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the bytes are too short, the magic/header layout
    /// is unsupported, reserved fields are non-zero, or the encoded length does
    /// not match the provided buffer.
    pub fn parse(bytes: &[u8]) -> Result<Self, PackError> {
        if bytes.len() < M0_HEADER_LEN {
            return Err(PackError::TooShort);
        }
        if bytes[..8] != MAGIC {
            return Err(PackError::InvalidMagic);
        }
        let format_major = u16::from_le_bytes([bytes[8], bytes[9]]);
        let format_minor = u16::from_le_bytes([bytes[10], bytes[11]]);
        let header_len = u16::from_le_bytes([bytes[12], bytes[13]]);
        let flags = u16::from_le_bytes([bytes[14], bytes[15]]);
        let file_len =
            u64::from_le_bytes(bytes[16..24].try_into().map_err(|_| PackError::TooShort)?);
        let section_count =
            u32::from_le_bytes(bytes[24..28].try_into().map_err(|_| PackError::TooShort)?);
        let reserved =
            u32::from_le_bytes(bytes[28..32].try_into().map_err(|_| PackError::TooShort)?);
        if usize::from(header_len) != M0_HEADER_LEN {
            return Err(PackError::UnsupportedHeaderLength);
        }
        let actual_len =
            u64::try_from(bytes.len()).map_err(|_| PackError::ResourceLimitExceeded)?;
        if file_len != actual_len {
            return Err(PackError::LengthMismatch);
        }
        if reserved != 0 {
            return Err(PackError::ReservedNotZero);
        }
        Ok(Self {
            format_major,
            format_minor,
            header_len,
            flags,
            file_len,
            section_count,
        })
    }

    #[must_use]
    pub const fn is_test_fixture(self) -> bool {
        self.flags & FLAG_TEST_FIXTURE != 0
    }
}
