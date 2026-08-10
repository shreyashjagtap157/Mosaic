use core::fmt;

#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct ByteOffset(pub u64);

#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct ByteLen(pub u64);

#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(C)]
pub struct ByteRange {
    pub start: ByteOffset,
    pub len: ByteLen,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RangeError {
    EndOverflow,
    OutOfBounds,
    OutputLengthMismatch,
}

impl fmt::Display for RangeError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::EndOverflow => f.write_str("byte range end overflows u64"),
            Self::OutOfBounds => f.write_str("byte range is outside the source"),
            Self::OutputLengthMismatch => {
                f.write_str("output buffer length does not match byte range")
            }
        }
    }
}

impl ByteRange {
    pub const EMPTY: Self = Self {
        start: ByteOffset(0),
        len: ByteLen(0),
    };

    #[must_use]
    pub const fn new(start: u64, len: u64) -> Self {
        Self {
            start: ByteOffset(start),
            len: ByteLen(len),
        }
    }

    /// Returns the exclusive end offset.
    ///
    /// # Errors
    ///
    /// Returns `RangeError::EndOverflow` when `start + len` cannot fit in `u64`.
    pub const fn checked_end(self) -> Result<ByteOffset, RangeError> {
        match self.start.0.checked_add(self.len.0) {
            Some(end) => Ok(ByteOffset(end)),
            None => Err(RangeError::EndOverflow),
        }
    }

    /// Validates that the range is contained by a source of `source_len` bytes.
    ///
    /// # Errors
    ///
    /// Returns `RangeError` if the range end overflows or exceeds `source_len`.
    pub const fn validate_for_len(self, source_len: u64) -> Result<(), RangeError> {
        match self.checked_end() {
            Ok(end) if end.0 <= source_len => Ok(()),
            Ok(_) => Err(RangeError::OutOfBounds),
            Err(error) => Err(error),
        }
    }

    #[must_use]
    pub const fn is_empty(self) -> bool {
        self.len.0 == 0
    }
}
