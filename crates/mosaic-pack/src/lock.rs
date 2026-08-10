use core::str;

use crate::{PackError, PackHash, PackValidationLimits};

const HEADER_LEN: usize = 16;
const ENTRY_HEADER_LEN: usize = 48;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ResolvedPackIdentityRef<'a> {
    pub publisher: &'a str,
    pub name: &'a str,
    pub semver_major: u16,
    pub semver_minor: u16,
    pub semver_patch: u16,
    pub pack_format_major: u16,
    pub pack_format_minor: u16,
    pub content_hash: PackHash,
}

#[derive(Clone, Copy, Debug)]
pub struct LockGraphView<'a> {
    bytes: &'a [u8],
    count: u32,
}

impl<'a> LockGraphView<'a> {
    /// Parses and validates a dependency lock graph section.
    ///
    /// # Errors
    ///
    /// Returns `PackError` when the lock graph header, UTF-8 identities,
    /// dependency limits, duplicate identities, or padding are invalid.
    pub fn parse(bytes: &'a [u8], limits: PackValidationLimits) -> Result<Self, PackError> {
        if bytes.len() < HEADER_LEN {
            return Err(PackError::InvalidLockGraphLength);
        }
        if bytes[..4] != *b"MSLK" {
            return Err(PackError::InvalidLockGraphMagic);
        }
        let version = read_u16(bytes, 4)?;
        let entry_header_len = read_u16(bytes, 6)?;
        let count = read_u32(bytes, 8)?;
        let reserved = read_u32(bytes, 12)?;
        if version != 1 {
            return Err(PackError::UnsupportedLockGraphVersion);
        }
        if usize::from(entry_header_len) != ENTRY_HEADER_LEN {
            return Err(PackError::UnsupportedLockEntryLength);
        }
        if reserved != 0 {
            return Err(PackError::ReservedNotZero);
        }
        if count > limits.max_dependencies {
            return Err(PackError::ResourceLimitExceeded);
        }

        let view = Self { bytes, count };
        let mut current_offset = HEADER_LEN;
        for index in 0..count {
            let (current, next) = view.entry_from_offset(current_offset, limits)?;

            // O(n^2), allocation-free duplicate validation. The dependency
            // count is resource-bounded, so attacker-controlled work is bounded.
            let mut prior_offset = HEADER_LEN;
            for _ in 0..index {
                let (prior, prior_next) = view.entry_from_offset(prior_offset, limits)?;
                if prior.publisher == current.publisher
                    && prior.name == current.name
                    && prior.semver_major == current.semver_major
                    && prior.semver_minor == current.semver_minor
                    && prior.semver_patch == current.semver_patch
                {
                    return Err(PackError::DuplicateDependencyIdentity);
                }
                prior_offset = prior_next;
            }
            current_offset = next;
        }
        if current_offset != bytes.len() {
            return Err(PackError::InvalidLockGraphLength);
        }
        Ok(view)
    }

    #[must_use]
    pub const fn len(self) -> u32 {
        self.count
    }

    #[must_use]
    pub const fn is_empty(self) -> bool {
        self.count == 0
    }

    /// Returns the resolved dependency identity at `index`.
    ///
    /// # Errors
    ///
    /// Returns `PackError` when `index` is out of bounds or the encoded entry
    /// cannot be decoded under `limits`.
    pub fn entry(
        self,
        index: u32,
        limits: PackValidationLimits,
    ) -> Result<ResolvedPackIdentityRef<'a>, PackError> {
        if index >= self.count {
            return Err(PackError::DependencyIndexOutOfBounds);
        }
        let mut offset = HEADER_LEN;
        for current in 0..=index {
            let (entry, next) = self.entry_from_offset(offset, limits)?;
            if current == index {
                return Ok(entry);
            }
            offset = next;
        }
        Err(PackError::DependencyIndexOutOfBounds)
    }

    /// Checks whether every locked dependency hash appears in `available`.
    ///
    /// # Errors
    ///
    /// Returns `PackError` when a dependency entry cannot be decoded under
    /// `limits`.
    pub fn dependencies_satisfied_by(
        self,
        available: &[PackHash],
        limits: PackValidationLimits,
    ) -> Result<bool, PackError> {
        let mut offset = HEADER_LEN;
        for _ in 0..self.count {
            let (dependency, next) = self.entry_from_offset(offset, limits)?;
            if !available.contains(&dependency.content_hash) {
                return Ok(false);
            }
            offset = next;
        }
        Ok(true)
    }

    fn entry_from_offset(
        self,
        offset: usize,
        limits: PackValidationLimits,
    ) -> Result<(ResolvedPackIdentityRef<'a>, usize), PackError> {
        let header_end = offset
            .checked_add(ENTRY_HEADER_LEN)
            .ok_or(PackError::IntegerOverflow)?;
        let header = self
            .bytes
            .get(offset..header_end)
            .ok_or(PackError::InvalidLockGraphLength)?;
        let publisher_len = usize::from(read_u16(header, 0)?);
        let name_len = usize::from(read_u16(header, 2)?);
        if publisher_len == 0
            || name_len == 0
            || publisher_len > usize::from(limits.max_identity_component_bytes)
            || name_len > usize::from(limits.max_identity_component_bytes)
        {
            return Err(PackError::ResourceLimitExceeded);
        }
        let flags = read_u16(header, 14)?;
        if flags != 0 {
            return Err(PackError::ReservedNotZero);
        }
        let mut content_hash = [0_u8; 32];
        content_hash.copy_from_slice(&header[16..48]);
        if content_hash.iter().all(|byte| *byte == 0) {
            return Err(PackError::ZeroDependencyHash);
        }

        let publisher_start = header_end;
        let publisher_end = publisher_start
            .checked_add(publisher_len)
            .ok_or(PackError::IntegerOverflow)?;
        let name_end = publisher_end
            .checked_add(name_len)
            .ok_or(PackError::IntegerOverflow)?;
        let publisher_bytes = self
            .bytes
            .get(publisher_start..publisher_end)
            .ok_or(PackError::InvalidLockGraphLength)?;
        let name_bytes = self
            .bytes
            .get(publisher_end..name_end)
            .ok_or(PackError::InvalidLockGraphLength)?;
        let publisher =
            str::from_utf8(publisher_bytes).map_err(|_| PackError::InvalidIdentityUtf8)?;
        let name = str::from_utf8(name_bytes).map_err(|_| PackError::InvalidIdentityUtf8)?;
        let next = align4(name_end).ok_or(PackError::IntegerOverflow)?;
        let padding = self
            .bytes
            .get(name_end..next)
            .ok_or(PackError::InvalidLockGraphLength)?;
        if padding.iter().any(|byte| *byte != 0) {
            return Err(PackError::ReservedNotZero);
        }

        Ok((
            ResolvedPackIdentityRef {
                publisher,
                name,
                semver_major: read_u16(header, 4)?,
                semver_minor: read_u16(header, 6)?,
                semver_patch: read_u16(header, 8)?,
                pack_format_major: read_u16(header, 10)?,
                pack_format_minor: read_u16(header, 12)?,
                content_hash: PackHash(content_hash),
            },
            next,
        ))
    }
}

fn align4(value: usize) -> Option<usize> {
    value.checked_add(3).map(|adjusted| adjusted & !3)
}

fn read_u16(bytes: &[u8], offset: usize) -> Result<u16, PackError> {
    let data = bytes.get(offset..offset + 2).ok_or(PackError::TooShort)?;
    Ok(u16::from_le_bytes([data[0], data[1]]))
}

fn read_u32(bytes: &[u8], offset: usize) -> Result<u32, PackError> {
    let data = bytes.get(offset..offset + 4).ok_or(PackError::TooShort)?;
    Ok(u32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}
