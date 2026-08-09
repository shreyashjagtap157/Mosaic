use crate::{ByteRange, CanonicalLeaves, RangeError, SourceIdentity};

#[cfg(feature = "alloc")]
use alloc::{boxed::Box, vec::Vec};

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum SourceError {
    Range(RangeError),
    LengthDoesNotFitPlatform,
}

impl From<RangeError> for SourceError {
    fn from(value: RangeError) -> Self {
        Self::Range(value)
    }
}

pub trait Source {
    fn len(&self) -> u64;

    fn read_byte(&self, offset: u64) -> Option<u8>;

    fn read_exact(&self, range: ByteRange, output: &mut [u8]) -> Result<(), SourceError>;

    #[must_use]
    fn is_empty(&self) -> bool {
        self.len() == 0
    }

    #[must_use]
    fn canonical_leaves(&self) -> CanonicalLeaves {
        CanonicalLeaves::new(self.len())
    }
}

#[derive(Clone, Copy, Debug)]
pub struct BorrowedSource<'a> {
    bytes: &'a [u8],
}

impl<'a> BorrowedSource<'a> {
    #[must_use]
    pub const fn new(bytes: &'a [u8]) -> Self {
        Self { bytes }
    }

    #[must_use]
    pub const fn as_bytes(self) -> &'a [u8] {
        self.bytes
    }
}

impl Source for BorrowedSource<'_> {
    fn len(&self) -> u64 {
        u64::try_from(self.bytes.len()).expect("slice length must fit Mosaic u64 coordinates")
    }

    fn read_byte(&self, offset: u64) -> Option<u8> {
        let index = usize::try_from(offset).ok()?;
        self.bytes.get(index).copied()
    }

    fn read_exact(&self, range: ByteRange, output: &mut [u8]) -> Result<(), SourceError> {
        range.validate_for_len(self.len())?;
        let expected = usize::try_from(range.len.0).map_err(|_| SourceError::LengthDoesNotFitPlatform)?;
        if output.len() != expected {
            return Err(RangeError::OutputLengthMismatch.into());
        }

        let start = usize::try_from(range.start.0).map_err(|_| SourceError::LengthDoesNotFitPlatform)?;
        let end = start.checked_add(expected).ok_or(RangeError::EndOverflow)?;
        output.copy_from_slice(&self.bytes[start..end]);
        Ok(())
    }
}

#[cfg(feature = "alloc")]
#[derive(Clone, Debug)]
pub struct OwnedSource {
    bytes: Box<[u8]>,
}

#[cfg(feature = "alloc")]
impl OwnedSource {
    #[must_use]
    pub fn new(bytes: Vec<u8>) -> Self {
        Self {
            bytes: bytes.into_boxed_slice(),
        }
    }

    #[must_use]
    pub fn as_bytes(&self) -> &[u8] {
        &self.bytes
    }
}

#[cfg(feature = "alloc")]
impl Source for OwnedSource {
    fn len(&self) -> u64 {
        u64::try_from(self.bytes.len()).expect("slice length must fit Mosaic u64 coordinates")
    }

    fn read_byte(&self, offset: u64) -> Option<u8> {
        BorrowedSource::new(&self.bytes).read_byte(offset)
    }

    fn read_exact(&self, range: ByteRange, output: &mut [u8]) -> Result<(), SourceError> {
        BorrowedSource::new(&self.bytes).read_exact(range, output)
    }
}

#[derive(Clone, Debug)]
pub struct VersionedSource<S> {
    source: S,
    identity: SourceIdentity,
}

impl<S> VersionedSource<S> {
    #[must_use]
    pub const fn new(source: S, identity: SourceIdentity) -> Self {
        Self { source, identity }
    }

    #[must_use]
    pub const fn identity(&self) -> SourceIdentity {
        self.identity
    }

    #[must_use]
    pub const fn inner(&self) -> &S {
        &self.source
    }

    #[must_use]
    pub fn into_inner(self) -> S {
        self.source
    }
}

impl<S: Source> Source for VersionedSource<S> {
    fn len(&self) -> u64 {
        self.source.len()
    }

    fn read_byte(&self, offset: u64) -> Option<u8> {
        self.source.read_byte(offset)
    }

    fn read_exact(&self, range: ByteRange, output: &mut [u8]) -> Result<(), SourceError> {
        self.source.read_exact(range, output)
    }
}
