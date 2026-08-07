use crate::{PackError, PackValidationLimits};

pub const UNICODE_MAGIC: [u8; 4] = *b"MSUC";
pub const UNICODE_HEADER_LEN: usize = 48;
const PROPERTY_RANGE_LEN: usize = 12;
const EP_RANGE_LEN: usize = 8;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PropertyRange {
    pub start: u32,
    pub end: u32,
    pub value: u8,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CodepointRange {
    pub start: u32,
    pub end: u32,
}

#[derive(Clone, Copy, Debug)]
pub struct UnicodeDataView<'a> {
    bytes: &'a [u8],
    gcb_count: u32,
    incb_count: u32,
    ep_count: u32,
    gcb_offset: usize,
    incb_offset: usize,
    ep_offset: usize,
}

impl<'a> UnicodeDataView<'a> {
    pub fn parse(bytes: &'a [u8], limits: PackValidationLimits) -> Result<Self, PackError> {
        if bytes.len() < UNICODE_HEADER_LEN || bytes[..4] != UNICODE_MAGIC {
            return Err(PackError::InvalidUnicodeHeader);
        }
        let version = read_u16(bytes, 4)?;
        let flags = read_u16(bytes, 6)?;
        let major = read_u16(bytes, 8)?;
        let minor = read_u16(bytes, 10)?;
        let patch = read_u16(bytes, 12)?;
        let reserved = read_u16(bytes, 14)?;
        let gcb_count = read_u32(bytes, 16)?;
        let incb_count = read_u32(bytes, 20)?;
        let ep_count = read_u32(bytes, 24)?;
        let gcb_offset = usize::try_from(read_u32(bytes, 28)?).map_err(|_| PackError::IntegerOverflow)?;
        let incb_offset = usize::try_from(read_u32(bytes, 32)?).map_err(|_| PackError::IntegerOverflow)?;
        let ep_offset = usize::try_from(read_u32(bytes, 36)?).map_err(|_| PackError::IntegerOverflow)?;
        let reserved2 = read_u64(bytes, 40)?;

        if version != 1 || (major, minor, patch) != (17, 0, 0) {
            return Err(PackError::UnsupportedUnicodeVersion);
        }
        if flags != 0 || reserved != 0 || reserved2 != 0 {
            return Err(PackError::ReservedNotZero);
        }
        let total_ranges = gcb_count
            .checked_add(incb_count)
            .and_then(|value| value.checked_add(ep_count))
            .ok_or(PackError::IntegerOverflow)?;
        if total_ranges > limits.max_unicode_ranges {
            return Err(PackError::ResourceLimitExceeded);
        }
        if gcb_offset != UNICODE_HEADER_LEN {
            return Err(PackError::InvalidUnicodeLayout);
        }
        let gcb_end = gcb_offset
            .checked_add(
                usize::try_from(gcb_count)
                    .map_err(|_| PackError::IntegerOverflow)?
                    .checked_mul(PROPERTY_RANGE_LEN)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        let incb_end = incb_offset
            .checked_add(
                usize::try_from(incb_count)
                    .map_err(|_| PackError::IntegerOverflow)?
                    .checked_mul(PROPERTY_RANGE_LEN)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        let ep_end = ep_offset
            .checked_add(
                usize::try_from(ep_count)
                    .map_err(|_| PackError::IntegerOverflow)?
                    .checked_mul(EP_RANGE_LEN)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        if !(gcb_end <= incb_offset && incb_end <= ep_offset && ep_end == bytes.len()) {
            return Err(PackError::InvalidUnicodeLayout);
        }
        if bytes[gcb_end..incb_offset].iter().any(|value| *value != 0)
            || bytes[incb_end..ep_offset].iter().any(|value| *value != 0)
        {
            return Err(PackError::NonCanonicalPadding);
        }
        let view = Self {
            bytes,
            gcb_count,
            incb_count,
            ep_count,
            gcb_offset,
            incb_offset,
            ep_offset,
        };
        view.validate_ranges()?;
        Ok(view)
    }

    #[must_use]
    pub const fn version(self) -> (u16, u16, u16) {
        (17, 0, 0)
    }

    pub fn grapheme_break(self, scalar: u32) -> Result<u8, PackError> {
        self.property_lookup(self.gcb_offset, self.gcb_count, scalar)
    }

    pub fn indic_conjunct_break(self, scalar: u32) -> Result<u8, PackError> {
        self.property_lookup(self.incb_offset, self.incb_count, scalar)
    }

    pub fn is_extended_pictographic(self, scalar: u32) -> Result<bool, PackError> {
        let mut low = 0_u32;
        let mut high = self.ep_count;
        while low < high {
            let mid = low + (high - low) / 2;
            let range = self.ep_range(mid)?;
            if scalar < range.start {
                high = mid;
            } else if scalar >= range.end {
                low = mid + 1;
            } else {
                return Ok(true);
            }
        }
        Ok(false)
    }

    pub fn gcb_range(self, index: u32) -> Result<PropertyRange, PackError> {
        self.property_range(self.gcb_offset, self.gcb_count, index)
    }

    pub fn incb_range(self, index: u32) -> Result<PropertyRange, PackError> {
        self.property_range(self.incb_offset, self.incb_count, index)
    }

    pub fn ep_range(self, index: u32) -> Result<CodepointRange, PackError> {
        if index >= self.ep_count {
            return Err(PackError::UnicodeRangeIndexOutOfBounds);
        }
        let offset = self
            .ep_offset
            .checked_add(
                usize::try_from(index)
                    .map_err(|_| PackError::IntegerOverflow)?
                    .checked_mul(EP_RANGE_LEN)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        Ok(CodepointRange {
            start: read_u32(self.bytes, offset)?,
            end: read_u32(self.bytes, offset + 4)?,
        })
    }

    fn property_lookup(self, offset: usize, count: u32, scalar: u32) -> Result<u8, PackError> {
        let mut low = 0_u32;
        let mut high = count;
        while low < high {
            let mid = low + (high - low) / 2;
            let range = self.property_range(offset, count, mid)?;
            if scalar < range.start {
                high = mid;
            } else if scalar >= range.end {
                low = mid + 1;
            } else {
                return Ok(range.value);
            }
        }
        Ok(0)
    }

    fn property_range(self, offset: usize, count: u32, index: u32) -> Result<PropertyRange, PackError> {
        if index >= count {
            return Err(PackError::UnicodeRangeIndexOutOfBounds);
        }
        let base = offset
            .checked_add(
                usize::try_from(index)
                    .map_err(|_| PackError::IntegerOverflow)?
                    .checked_mul(PROPERTY_RANGE_LEN)
                    .ok_or(PackError::IntegerOverflow)?,
            )
            .ok_or(PackError::IntegerOverflow)?;
        let reserved_start = base.checked_add(9).ok_or(PackError::IntegerOverflow)?;
        let reserved_end = base.checked_add(12).ok_or(PackError::IntegerOverflow)?;
        if self
            .bytes
            .get(reserved_start..reserved_end)
            .ok_or(PackError::InvalidUnicodeLayout)?
            .iter()
            .any(|value| *value != 0)
        {
            return Err(PackError::ReservedNotZero);
        }
        Ok(PropertyRange {
            start: read_u32(self.bytes, base)?,
            end: read_u32(self.bytes, base + 4)?,
            value: *self.bytes.get(base + 8).ok_or(PackError::InvalidUnicodeLayout)?,
        })
    }

    fn validate_ranges(self) -> Result<(), PackError> {
        for (offset, count, max_value) in [
            (self.gcb_offset, self.gcb_count, 13_u8),
            (self.incb_offset, self.incb_count, 3_u8),
        ] {
            let mut previous_end = 0_u32;
            for index in 0..count {
                let range = self.property_range(offset, count, index)?;
                if range.start >= range.end
                    || range.end > 0x11_0000
                    || (index != 0 && range.start < previous_end)
                    || range.value == 0
                    || range.value > max_value
                {
                    return Err(PackError::InvalidUnicodeRange);
                }
                previous_end = range.end;
            }
        }
        let mut previous_end = 0_u32;
        for index in 0..self.ep_count {
            let range = self.ep_range(index)?;
            if range.start >= range.end
                || range.end > 0x11_0000
                || (index != 0 && range.start < previous_end)
            {
                return Err(PackError::InvalidUnicodeRange);
            }
            previous_end = range.end;
        }
        Ok(())
    }
}

fn read_u16(bytes: &[u8], offset: usize) -> Result<u16, PackError> {
    let end = offset.checked_add(2).ok_or(PackError::IntegerOverflow)?;
    let data = bytes.get(offset..end).ok_or(PackError::InvalidUnicodeLayout)?;
    Ok(u16::from_le_bytes([data[0], data[1]]))
}
fn read_u32(bytes: &[u8], offset: usize) -> Result<u32, PackError> {
    let end = offset.checked_add(4).ok_or(PackError::IntegerOverflow)?;
    let data = bytes.get(offset..end).ok_or(PackError::InvalidUnicodeLayout)?;
    Ok(u32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}
fn read_u64(bytes: &[u8], offset: usize) -> Result<u64, PackError> {
    let end = offset.checked_add(8).ok_or(PackError::IntegerOverflow)?;
    let data = bytes.get(offset..end).ok_or(PackError::InvalidUnicodeLayout)?;
    Ok(u64::from_le_bytes([
        data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7],
    ]))
}
