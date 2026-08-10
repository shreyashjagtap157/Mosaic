use crate::{LockGraphView, ManifestV1, PackError, PackHash, PackValidationLimits, Sha256, sha256};

pub const V1_HEADER_LEN: usize = 96;
pub const SECTION_ENTRY_LEN: usize = 32;
const SECTION_ENTRY_LEN_U64: u64 = 32;
pub const CONTENT_HASH_START: usize = 48;
pub const CONTENT_HASH_END: usize = 80;
pub const NONE_SECTION_INDEX: u32 = u32::MAX;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(u16)]
pub enum HashAlgorithm {
    Sha256 = 1,
    Blake3 = 2,
}

impl HashAlgorithm {
    fn parse(value: u16) -> Result<Self, PackError> {
        match value {
            1 => Ok(Self::Sha256),
            2 => Ok(Self::Blake3),
            _ => Err(PackError::UnsupportedHashAlgorithm),
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PackHeaderV1 {
    pub format_major: u16,
    pub format_minor: u16,
    pub header_len: u16,
    pub flags: u16,
    pub file_len: u64,
    pub section_count: u32,
    pub section_entry_len: u16,
    pub hash_algorithm: HashAlgorithm,
    pub section_directory_offset: u64,
    pub manifest_section_index: u32,
    pub lock_section_index: u32,
    pub content_hash: PackHash,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct SectionEntry {
    pub kind: u32,
    pub flags: u32,
    pub offset: u64,
    pub length: u64,
    pub element_count: u32,
    pub element_width: u16,
    pub alignment_log2: u8,
}

#[derive(Clone, Copy, Debug)]
pub struct PackView<'a> {
    bytes: &'a [u8],
    header: PackHeaderV1,
}

impl PackHeaderV1 {
    /// Parses a MOSPACK v1 header.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the header is too short, uses unsupported format
    /// features, violates resource limits, or points outside the provided bytes.
    pub fn parse(bytes: &[u8], limits: PackValidationLimits) -> Result<Self, PackError> {
        if bytes.len() < V1_HEADER_LEN {
            return Err(PackError::TooShort);
        }
        if bytes[..8] != crate::MAGIC {
            return Err(PackError::InvalidMagic);
        }

        let format_major = read_u16(bytes, 8)?;
        let format_minor = read_u16(bytes, 10)?;
        let header_len = read_u16(bytes, 12)?;
        let flags = read_u16(bytes, 14)?;
        let file_len = read_u64(bytes, 16)?;
        let section_count = read_u32(bytes, 24)?;
        let section_entry_len = read_u16(bytes, 28)?;
        let hash_algorithm = HashAlgorithm::parse(read_u16(bytes, 30)?)?;
        let section_directory_offset = read_u64(bytes, 32)?;
        let manifest_section_index = read_u32(bytes, 40)?;
        let lock_section_index = read_u32(bytes, 44)?;
        let mut content_hash = [0_u8; 32];
        content_hash.copy_from_slice(&bytes[CONTENT_HASH_START..CONTENT_HASH_END]);

        if format_major != 1 {
            return Err(PackError::UnsupportedFormatMajor);
        }
        if flags != 0 {
            return Err(PackError::UnsupportedPackFlags);
        }
        if usize::from(header_len) != V1_HEADER_LEN {
            return Err(PackError::UnsupportedHeaderLength);
        }
        if usize::from(section_entry_len) != SECTION_ENTRY_LEN {
            return Err(PackError::UnsupportedSectionEntryLength);
        }
        let actual_file_len =
            u64::try_from(bytes.len()).map_err(|_| PackError::ResourceLimitExceeded)?;
        if file_len != actual_file_len {
            return Err(PackError::LengthMismatch);
        }
        if file_len > limits.max_file_bytes {
            return Err(PackError::ResourceLimitExceeded);
        }
        if section_count > limits.max_sections {
            return Err(PackError::ResourceLimitExceeded);
        }
        if bytes[CONTENT_HASH_END..V1_HEADER_LEN]
            .iter()
            .any(|byte| *byte != 0)
        {
            return Err(PackError::ReservedNotZero);
        }

        let directory_len = u64::from(section_count)
            .checked_mul(SECTION_ENTRY_LEN_U64)
            .ok_or(PackError::IntegerOverflow)?;
        let directory_end = section_directory_offset
            .checked_add(directory_len)
            .ok_or(PackError::IntegerOverflow)?;
        if section_directory_offset < u64::from(header_len) || directory_end > file_len {
            return Err(PackError::SectionDirectoryOutOfBounds);
        }
        if section_directory_offset % 8 != 0 {
            return Err(PackError::MisalignedSection);
        }
        let directory_start =
            usize::try_from(section_directory_offset).map_err(|_| PackError::IntegerOverflow)?;
        if bytes[usize::from(header_len)..directory_start]
            .iter()
            .any(|byte| *byte != 0)
        {
            return Err(PackError::NonCanonicalPadding);
        }

        for index in [manifest_section_index, lock_section_index] {
            if index != NONE_SECTION_INDEX && index >= section_count {
                return Err(PackError::InvalidSectionIndex);
            }
        }

        Ok(Self {
            format_major,
            format_minor,
            header_len,
            flags,
            file_len,
            section_count,
            section_entry_len,
            hash_algorithm,
            section_directory_offset,
            manifest_section_index,
            lock_section_index,
            content_hash: PackHash(content_hash),
        })
    }
}

impl<'a> PackView<'a> {
    /// Parses a complete MOSPACK v1 file view.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the header, section directory, content hash, or
    /// special manifest/lock section references are invalid.
    pub fn parse(bytes: &'a [u8], limits: PackValidationLimits) -> Result<Self, PackError> {
        let header = PackHeaderV1::parse(bytes, limits)?;
        let view = Self { bytes, header };
        view.validate_sections(limits)?;
        view.verify_content_hash()?;
        view.validate_special_sections()?;
        Ok(view)
    }

    #[must_use]
    pub const fn header(self) -> PackHeaderV1 {
        self.header
    }

    /// Returns a section directory entry.
    ///
    /// # Errors
    ///
    /// Returns `PackError` when `index` is out of bounds or the directory entry
    /// cannot be decoded.
    pub fn section(self, index: u32) -> Result<SectionEntry, PackError> {
        if index >= self.header.section_count {
            return Err(PackError::InvalidSectionIndex);
        }
        let base = self
            .header
            .section_directory_offset
            .checked_add(
                u64::from(index)
                    .checked_mul(SECTION_ENTRY_LEN_U64)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        let base = usize::try_from(base).map_err(|_| PackError::IntegerOverflow)?;
        parse_section_entry(self.bytes, base)
    }

    /// Returns the raw payload bytes for a section.
    ///
    /// # Errors
    ///
    /// Returns `PackError` when `index` is invalid or the section payload points
    /// outside the pack.
    pub fn section_bytes(self, index: u32) -> Result<&'a [u8], PackError> {
        let section = self.section(index)?;
        let start = usize::try_from(section.offset).map_err(|_| PackError::IntegerOverflow)?;
        let length = usize::try_from(section.length).map_err(|_| PackError::IntegerOverflow)?;
        let end = start
            .checked_add(length)
            .ok_or(PackError::IntegerOverflow)?;
        self.bytes
            .get(start..end)
            .ok_or(PackError::SectionOutOfBounds)
    }

    /// Returns the canonical manifest section bytes.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the pack has no manifest section or it cannot be
    /// sliced from the pack bytes.
    pub fn manifest_bytes(self) -> Result<&'a [u8], PackError> {
        if self.header.manifest_section_index == NONE_SECTION_INDEX {
            return Err(PackError::MissingManifestSection);
        }
        self.section_bytes(self.header.manifest_section_index)
    }

    /// Returns the canonical lock-graph section bytes.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the pack has no lock section or it cannot be
    /// sliced from the pack bytes.
    pub fn lock_bytes(self) -> Result<&'a [u8], PackError> {
        if self.header.lock_section_index == NONE_SECTION_INDEX {
            return Err(PackError::MissingLockSection);
        }
        self.section_bytes(self.header.lock_section_index)
    }

    /// Parses the canonical manifest section.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the manifest section is missing or malformed.
    pub fn canonical_manifest(self) -> Result<ManifestV1, PackError> {
        ManifestV1::parse(self.manifest_bytes()?)
    }

    /// Parses the canonical lock-graph section.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the lock section is missing, malformed, or exceeds
    /// configured limits.
    pub fn lock_graph(self, limits: PackValidationLimits) -> Result<LockGraphView<'a>, PackError> {
        LockGraphView::parse(self.lock_bytes()?, limits)
    }

    /// Validates the required singleton manifest and lock metadata.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if manifest/lock sections are not canonical or the
    /// manifest's lock hash does not match the lock bytes.
    pub fn validate_canonical_metadata(
        self,
        limits: PackValidationLimits,
    ) -> Result<(), PackError> {
        let mut manifests = 0_u32;
        let mut locks = 0_u32;
        for index in 0..self.header.section_count {
            match self.section(index)?.kind {
                crate::SECTION_KIND_MANIFEST => manifests += 1,
                crate::SECTION_KIND_LOCK_GRAPH => locks += 1,
                _ => {}
            }
        }
        if manifests != 1 || locks != 1 {
            return Err(PackError::InvalidSingletonSectionCount);
        }
        let manifest = self.canonical_manifest()?;
        let lock_bytes = self.lock_bytes()?;
        let _lock = LockGraphView::parse(lock_bytes, limits)?;
        if sha256(lock_bytes) != manifest.dependency_lock_hash {
            return Err(PackError::DependencyLockHashMismatch);
        }
        Ok(())
    }

    /// Validates canonical metadata and dependency availability.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if metadata validation fails or any locked dependency
    /// hash is absent from `available`.
    pub fn validate_canonical_dependencies(
        self,
        limits: PackValidationLimits,
        available: &[PackHash],
    ) -> Result<(), PackError> {
        self.validate_canonical_metadata(limits)?;
        let lock = self.lock_graph(limits)?;
        if lock.dependencies_satisfied_by(available, limits)? {
            Ok(())
        } else {
            Err(PackError::UnresolvedDependency)
        }
    }

    fn validate_sections(self, limits: PackValidationLimits) -> Result<(), PackError> {
        let directory_len = u64::from(self.header.section_count)
            .checked_mul(SECTION_ENTRY_LEN_U64)
            .ok_or(PackError::IntegerOverflow)?;
        let directory_end = self
            .header
            .section_directory_offset
            .checked_add(directory_len)
            .ok_or(PackError::IntegerOverflow)?;
        let mut previous_end = directory_end;

        for index in 0..self.header.section_count {
            let section = self.section(index)?;
            if section.kind == 0 {
                return Err(PackError::InvalidSectionKind);
            }
            if section.flags != 0 {
                return Err(PackError::UnsupportedSectionFlags);
            }
            if section.length > limits.max_section_bytes {
                return Err(PackError::ResourceLimitExceeded);
            }
            if section.alignment_log2 > limits.max_alignment_log2 {
                return Err(PackError::ResourceLimitExceeded);
            }
            let alignment = 1_u64
                .checked_shl(u32::from(section.alignment_log2))
                .ok_or(PackError::IntegerOverflow)?;
            if section.offset % alignment != 0 {
                return Err(PackError::MisalignedSection);
            }
            if section.offset < previous_end {
                return Err(PackError::OverlappingOrUnsortedSections);
            }
            if section.offset > self.header.file_len {
                return Err(PackError::SectionOutOfBounds);
            }
            let end = section
                .offset
                .checked_add(section.length)
                .ok_or(PackError::IntegerOverflow)?;
            if end > self.header.file_len {
                return Err(PackError::SectionOutOfBounds);
            }
            let gap_start =
                usize::try_from(previous_end).map_err(|_| PackError::IntegerOverflow)?;
            let gap_end =
                usize::try_from(section.offset).map_err(|_| PackError::IntegerOverflow)?;
            if self.bytes[gap_start..gap_end].iter().any(|byte| *byte != 0) {
                return Err(PackError::NonCanonicalPadding);
            }
            if section.element_width != 0 {
                let minimum = u64::from(section.element_count)
                    .checked_mul(u64::from(section.element_width))
                    .ok_or(PackError::IntegerOverflow)?;
                if minimum > section.length {
                    return Err(PackError::InvalidElementLayout);
                }
            }
            previous_end = end;
        }
        if previous_end != self.header.file_len {
            return Err(PackError::NonCanonicalPadding);
        }
        Ok(())
    }

    fn verify_content_hash(self) -> Result<(), PackError> {
        if self.header.hash_algorithm != HashAlgorithm::Sha256 {
            return Err(PackError::HashAlgorithmNotImplemented);
        }
        let mut hash = Sha256::new();
        hash.update(&self.bytes[..CONTENT_HASH_START]);
        hash.update(&[0_u8; 32]);
        hash.update(&self.bytes[CONTENT_HASH_END..]);
        if hash.finalize() != self.header.content_hash {
            return Err(PackError::ContentHashMismatch);
        }
        Ok(())
    }

    fn validate_special_sections(self) -> Result<(), PackError> {
        if self.header.manifest_section_index != NONE_SECTION_INDEX {
            let section = self.section(self.header.manifest_section_index)?;
            if section.kind != crate::SECTION_KIND_MANIFEST {
                return Err(PackError::ManifestSectionKindMismatch);
            }
        }
        if self.header.lock_section_index != NONE_SECTION_INDEX {
            let section = self.section(self.header.lock_section_index)?;
            if section.kind != crate::SECTION_KIND_LOCK_GRAPH {
                return Err(PackError::LockSectionKindMismatch);
            }
        }
        Ok(())
    }
}

fn parse_section_entry(bytes: &[u8], base: usize) -> Result<SectionEntry, PackError> {
    let end = base
        .checked_add(SECTION_ENTRY_LEN)
        .ok_or(PackError::IntegerOverflow)?;
    let entry = bytes
        .get(base..end)
        .ok_or(PackError::SectionDirectoryOutOfBounds)?;
    if entry[31] != 0 {
        return Err(PackError::ReservedNotZero);
    }
    Ok(SectionEntry {
        kind: read_u32(entry, 0)?,
        flags: read_u32(entry, 4)?,
        offset: read_u64(entry, 8)?,
        length: read_u64(entry, 16)?,
        element_count: read_u32(entry, 24)?,
        element_width: read_u16(entry, 28)?,
        alignment_log2: entry[30],
    })
}

fn read_u16(bytes: &[u8], offset: usize) -> Result<u16, PackError> {
    let data = bytes.get(offset..offset + 2).ok_or(PackError::TooShort)?;
    Ok(u16::from_le_bytes([data[0], data[1]]))
}

fn read_u32(bytes: &[u8], offset: usize) -> Result<u32, PackError> {
    let data = bytes.get(offset..offset + 4).ok_or(PackError::TooShort)?;
    Ok(u32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}

fn read_u64(bytes: &[u8], offset: usize) -> Result<u64, PackError> {
    let data = bytes.get(offset..offset + 8).ok_or(PackError::TooShort)?;
    Ok(u64::from_le_bytes([
        data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
    ]))
}
