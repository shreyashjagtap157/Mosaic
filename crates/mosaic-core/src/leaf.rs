use crate::{ByteOffset, ByteRange};

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
#[repr(transparent)]
pub struct CanonicalLeaf {
    offset: ByteOffset,
}

impl CanonicalLeaf {
    #[must_use]
    pub const fn at(offset: u64) -> Self {
        Self {
            offset: ByteOffset(offset),
        }
    }

    #[must_use]
    pub const fn offset(self) -> ByteOffset {
        self.offset
    }

    #[must_use]
    pub const fn range(self) -> ByteRange {
        ByteRange::new(self.offset.0, 1)
    }
}

#[derive(Clone, Debug)]
pub struct CanonicalLeaves {
    next: u64,
    end: u64,
}

impl CanonicalLeaves {
    #[must_use]
    pub const fn new(source_len: u64) -> Self {
        Self {
            next: 0,
            end: source_len,
        }
    }

    #[must_use]
    pub const fn remaining(&self) -> u64 {
        self.end - self.next
    }
}

impl Iterator for CanonicalLeaves {
    type Item = CanonicalLeaf;

    fn next(&mut self) -> Option<Self::Item> {
        if self.next == self.end {
            return None;
        }

        let leaf = CanonicalLeaf::at(self.next);
        self.next += 1;
        Some(leaf)
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let remaining = self.remaining();
        match usize::try_from(remaining) {
            Ok(value) => (value, Some(value)),
            Err(_) => (usize::MAX, None),
        }
    }
}
