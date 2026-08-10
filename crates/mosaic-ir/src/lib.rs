#![no_std]
#![forbid(unsafe_code)]

use mosaic_core::ByteRange;

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct NamespaceId(pub u16);

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct TokenKind(pub u16);

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct TokenId(pub u32);

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct ProjectionId(pub u32);

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(C)]
pub struct ContentHash(pub [u8; 32]);

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct EdgeCost(pub i32);

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct PathCost(pub i64);

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(C)]
pub struct CanonicalEdgeKey {
    pub start: u64,
    pub end: u64,
    pub namespace: NamespaceId,
    pub kind: TokenKind,
    pub token_id: TokenId,
    pub source_pack_hash: ContentHash,
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(C)]
pub struct CandidateEdge {
    pub key: CanonicalEdgeKey,
    pub cost: EdgeCost,
}

impl CandidateEdge {
    #[must_use]
    pub const fn checked_span_len(self) -> Option<u64> {
        self.key.end.checked_sub(self.key.start)
    }
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(C)]
pub struct SourceMapping {
    pub source: ByteRange,
    pub synthetic: bool,
}

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(C)]
pub struct ProjectedToken {
    pub source: ByteRange,
    pub namespace: NamespaceId,
    pub kind: TokenKind,
    pub id: TokenId,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(C)]
pub struct DfaRun {
    pub token_id: u32,
    pub cost: i64,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum PathComparisonError {
    CostOverflow,
    InvalidEdgeRange,
}
