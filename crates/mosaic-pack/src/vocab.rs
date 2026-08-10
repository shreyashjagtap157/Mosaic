use crate::{PackError, PackValidationLimits};

pub const VOCAB_MAGIC: [u8; 4] = *b"MSVC";
pub const VOCAB_HEADER_LEN: usize = 40;
pub const VOCAB_ENTRY_LEN: usize = 16;
pub const FIRST_BYTE_INDEX_COUNT: usize = 257;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct VocabularyEntry<'a> {
    pub token_id: u32,
    pub cost: i32,
    pub surface: &'a [u8],
}

#[derive(Clone, Copy, Debug)]
pub struct VocabularyView<'a> {
    bytes: &'a [u8],
    entry_count: u32,
    entries_offset: usize,
    id_index_offset: usize,
    first_byte_index_offset: usize,
    blob_offset: usize,
    blob_len: usize,
}

impl<'a> VocabularyView<'a> {
    /// Parses and validates a vocabulary section.
    ///
    /// # Errors
    ///
    /// Returns `PackError` when the header, layout, canonical ordering, indexes,
    /// byte-fallback coverage, or configured resource limits are invalid.
    pub fn parse(bytes: &'a [u8], limits: PackValidationLimits) -> Result<Self, PackError> {
        if bytes.len() < VOCAB_HEADER_LEN {
            return Err(PackError::InvalidVocabularyHeader);
        }
        if bytes[..4] != VOCAB_MAGIC {
            return Err(PackError::InvalidVocabularyHeader);
        }
        let version = read_u16(bytes, 4)?;
        let flags = read_u16(bytes, 6)?;
        let entry_count = read_u32(bytes, 8)?;
        let entry_len = read_u16(bytes, 12)?;
        let reserved = read_u16(bytes, 14)?;
        let entries_offset = read_u32(bytes, 16)?;
        let id_index_offset = read_u32(bytes, 20)?;
        let first_byte_index_offset = read_u32(bytes, 24)?;
        let blob_offset = read_u32(bytes, 28)?;
        let blob_len = read_u32(bytes, 32)?;
        let reserved2 = read_u32(bytes, 36)?;

        if version != 1 {
            return Err(PackError::UnsupportedVocabularyVersion);
        }
        if flags != 0 || reserved != 0 || reserved2 != 0 {
            return Err(PackError::ReservedNotZero);
        }
        if usize::from(entry_len) != VOCAB_ENTRY_LEN
            || usize::try_from(entries_offset).ok() != Some(VOCAB_HEADER_LEN)
        {
            return Err(PackError::InvalidVocabularyLayout);
        }
        if entry_count > limits.max_vocab_entries {
            return Err(PackError::ResourceLimitExceeded);
        }

        let entries_offset =
            usize::try_from(entries_offset).map_err(|_| PackError::IntegerOverflow)?;
        let id_index_offset =
            usize::try_from(id_index_offset).map_err(|_| PackError::IntegerOverflow)?;
        let first_byte_index_offset =
            usize::try_from(first_byte_index_offset).map_err(|_| PackError::IntegerOverflow)?;
        let blob_offset = usize::try_from(blob_offset).map_err(|_| PackError::IntegerOverflow)?;
        let blob_len = usize::try_from(blob_len).map_err(|_| PackError::IntegerOverflow)?;
        let count = usize::try_from(entry_count).map_err(|_| PackError::IntegerOverflow)?;

        let entries_end = entries_offset
            .checked_add(
                count
                    .checked_mul(VOCAB_ENTRY_LEN)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        let id_index_end = id_index_offset
            .checked_add(count.checked_mul(4).ok_or(PackError::IntegerOverflow)?)
            .ok_or(PackError::IntegerOverflow)?;
        let first_byte_index_end = first_byte_index_offset
            .checked_add(
                FIRST_BYTE_INDEX_COUNT
                    .checked_mul(4)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        let blob_end = blob_offset
            .checked_add(blob_len)
            .ok_or(PackError::IntegerOverflow)?;

        if !(entries_end <= id_index_offset
            && id_index_offset <= id_index_end
            && id_index_end <= first_byte_index_offset
            && first_byte_index_offset <= first_byte_index_end
            && first_byte_index_end <= blob_offset
            && blob_offset <= blob_end
            && blob_end == bytes.len())
        {
            return Err(PackError::InvalidVocabularyLayout);
        }
        for (start, end) in [
            (entries_end, id_index_offset),
            (id_index_end, first_byte_index_offset),
            (first_byte_index_end, blob_offset),
        ] {
            if bytes[start..end].iter().any(|byte| *byte != 0) {
                return Err(PackError::NonCanonicalPadding);
            }
        }

        let view = Self {
            bytes,
            entry_count,
            entries_offset,
            id_index_offset,
            first_byte_index_offset,
            blob_offset,
            blob_len,
        };
        view.validate_entries(limits)?;
        view.validate_id_index()?;
        view.validate_first_byte_index()?;
        view.validate_byte_fallback()?;
        Ok(view)
    }

    #[must_use]
    pub const fn len(self) -> u32 {
        self.entry_count
    }

    #[must_use]
    pub const fn is_empty(self) -> bool {
        self.entry_count == 0
    }

    /// Returns the vocabulary entry at canonical position `index`.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if `index` is out of bounds or the encoded entry is
    /// malformed.
    pub fn entry(self, index: u32) -> Result<VocabularyEntry<'a>, PackError> {
        if index >= self.entry_count {
            return Err(PackError::VocabularyIndexOutOfBounds);
        }
        let base = self
            .entries_offset
            .checked_add(
                usize::try_from(index)
                    .map_err(|_| PackError::IntegerOverflow)?
                    .checked_mul(VOCAB_ENTRY_LEN)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        let token_id = read_u32(self.bytes, base)?;
        let cost = read_i32(self.bytes, base + 4)?;
        let surface_offset = usize::try_from(read_u32(self.bytes, base + 8)?)
            .map_err(|_| PackError::IntegerOverflow)?;
        let surface_len = usize::from(read_u16(self.bytes, base + 12)?);
        let flags = read_u16(self.bytes, base + 14)?;
        if flags != 0 {
            return Err(PackError::UnsupportedVocabularyEntryFlags);
        }
        if surface_len == 0 {
            return Err(PackError::InvalidVocabularySurface);
        }
        let relative_end = surface_offset
            .checked_add(surface_len)
            .ok_or(PackError::IntegerOverflow)?;
        if relative_end > self.blob_len {
            return Err(PackError::InvalidVocabularySurface);
        }
        let start = self
            .blob_offset
            .checked_add(surface_offset)
            .ok_or(PackError::IntegerOverflow)?;
        let end = start
            .checked_add(surface_len)
            .ok_or(PackError::IntegerOverflow)?;
        let surface = self
            .bytes
            .get(start..end)
            .ok_or(PackError::InvalidVocabularySurface)?;
        Ok(VocabularyEntry {
            token_id,
            cost,
            surface,
        })
    }

    /// Looks up a vocabulary entry by token ID.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the token-ID index points outside the vocabulary
    /// or the referenced entry is malformed.
    pub fn entry_by_token_id(
        self,
        token_id: u32,
    ) -> Result<Option<VocabularyEntry<'a>>, PackError> {
        let mut low = 0_u32;
        let mut high = self.entry_count;
        while low < high {
            let middle = low + (high - low) / 2;
            let entry_index = self.id_index(middle)?;
            let entry = self.entry(entry_index)?;
            match entry.token_id.cmp(&token_id) {
                core::cmp::Ordering::Less => low = middle + 1,
                core::cmp::Ordering::Greater => high = middle,
                core::cmp::Ordering::Equal => return Ok(Some(entry)),
            }
        }
        Ok(None)
    }

    /// Returns the canonical entry range whose surfaces begin with `byte`.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the first-byte index is malformed.
    pub fn candidate_range_for_first_byte(
        self,
        byte: u8,
    ) -> Result<core::ops::Range<u32>, PackError> {
        let start = self.first_byte_index(u16::from(byte))?;
        let end = self.first_byte_index(u16::from(byte) + 1)?;
        Ok(start..end)
    }

    fn validate_entries(self, limits: PackValidationLimits) -> Result<(), PackError> {
        let mut previous_index: Option<u32> = None;
        for index in 0..self.entry_count {
            let entry = self.entry(index)?;
            let surface_len =
                u32::try_from(entry.surface.len()).map_err(|_| PackError::ResourceLimitExceeded)?;
            if surface_len > limits.max_token_bytes {
                return Err(PackError::ResourceLimitExceeded);
            }
            if let Some(previous) = previous_index {
                let prior = self.entry(previous)?;
                let ordering = prior
                    .surface
                    .cmp(entry.surface)
                    .then(prior.token_id.cmp(&entry.token_id));
                if ordering != core::cmp::Ordering::Less {
                    return Err(PackError::VocabularyNotCanonical);
                }
            }
            previous_index = Some(index);
        }
        Ok(())
    }

    fn validate_id_index(self) -> Result<(), PackError> {
        let count = usize::try_from(self.entry_count).map_err(|_| PackError::IntegerOverflow)?;
        // The index contains exactly `entry_count` in-range positions. Strictly
        // increasing token IDs also reject a repeated entry index (same ID) and
        // repeated token IDs across different entries. Therefore the bounded O(n)
        // pass proves the index is a permutation without an allocation-heavy bitmap.
        let mut previous_id: Option<u32> = None;
        for position in 0..self.entry_count {
            let index = self.id_index(position)?;
            if usize::try_from(index).map_err(|_| PackError::IntegerOverflow)? >= count {
                return Err(PackError::InvalidVocabularyIdIndex);
            }
            let token_id = self.entry(index)?.token_id;
            if previous_id.is_some_and(|prior| prior >= token_id) {
                return Err(PackError::VocabularyTokenIdsNotUnique);
            }
            previous_id = Some(token_id);
        }
        Ok(())
    }

    fn validate_first_byte_index(self) -> Result<(), PackError> {
        if self.first_byte_index(0)? != 0 || self.first_byte_index(256)? != self.entry_count {
            return Err(PackError::InvalidVocabularyFirstByteIndex);
        }
        for byte in 0_u8..=u8::MAX {
            let start = self.first_byte_index(u16::from(byte))?;
            let end = self.first_byte_index(u16::from(byte) + 1)?;
            if start > end || end > self.entry_count {
                return Err(PackError::InvalidVocabularyFirstByteIndex);
            }
            for index in start..end {
                if self.entry(index)?.surface.first().copied() != Some(byte) {
                    return Err(PackError::InvalidVocabularyFirstByteIndex);
                }
            }
        }
        Ok(())
    }

    fn validate_byte_fallback(self) -> Result<(), PackError> {
        for value in 0_u8..=u8::MAX {
            let Some(entry) = self.entry_by_token_id(u32::from(value))? else {
                return Err(PackError::MissingByteFallback);
            };
            if entry.surface.len() != 1 || entry.surface[0] != value {
                return Err(PackError::MissingByteFallback);
            }
        }
        Ok(())
    }

    fn id_index(self, position: u32) -> Result<u32, PackError> {
        if position >= self.entry_count {
            return Err(PackError::VocabularyIndexOutOfBounds);
        }
        let offset = self
            .id_index_offset
            .checked_add(
                usize::try_from(position)
                    .map_err(|_| PackError::IntegerOverflow)?
                    .checked_mul(4)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        read_u32(self.bytes, offset)
    }

    fn first_byte_index(self, position: u16) -> Result<u32, PackError> {
        if usize::from(position) >= FIRST_BYTE_INDEX_COUNT {
            return Err(PackError::InvalidVocabularyFirstByteIndex);
        }
        let offset = self
            .first_byte_index_offset
            .checked_add(
                usize::from(position)
                    .checked_mul(4)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        read_u32(self.bytes, offset)
    }
}

fn read_u16(bytes: &[u8], offset: usize) -> Result<u16, PackError> {
    let end = offset.checked_add(2).ok_or(PackError::IntegerOverflow)?;
    let data = bytes
        .get(offset..end)
        .ok_or(PackError::InvalidVocabularyLayout)?;
    Ok(u16::from_le_bytes([data[0], data[1]]))
}

fn read_u32(bytes: &[u8], offset: usize) -> Result<u32, PackError> {
    let end = offset.checked_add(4).ok_or(PackError::IntegerOverflow)?;
    let data = bytes
        .get(offset..end)
        .ok_or(PackError::InvalidVocabularyLayout)?;
    Ok(u32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}

fn read_i32(bytes: &[u8], offset: usize) -> Result<i32, PackError> {
    let end = offset.checked_add(4).ok_or(PackError::IntegerOverflow)?;
    let data = bytes
        .get(offset..end)
        .ok_or(PackError::InvalidVocabularyLayout)?;
    Ok(i32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}
