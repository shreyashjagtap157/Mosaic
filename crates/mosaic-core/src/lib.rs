#![no_std]
#![forbid(unsafe_code)]

#[cfg(feature = "alloc")]
extern crate alloc;

mod identity;
mod leaf;
mod range;
mod source;

pub use identity::{SourceId, SourceIdentity, SourceVersion};
pub use leaf::{CanonicalLeaf, CanonicalLeaves};
pub use range::{ByteLen, ByteOffset, ByteRange, RangeError};
pub use source::{BorrowedSource, Source, SourceError, VersionedSource};

#[cfg(feature = "alloc")]
pub use source::OwnedSource;
