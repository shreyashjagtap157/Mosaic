use crate::{PackError, PackHash};

pub const MANIFEST_V1_LEN: usize = 64;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct ManifestV1 {
    pub runtime_semantics_version: u32,
    pub canonical_leaf_version: u32,
    pub cost_semantics_version: u32,
    pub tie_break_version: u32,
    pub control_protocol_version: u32,
    pub dependency_lock_hash: PackHash,
}

impl ManifestV1 {
    pub fn parse(bytes: &[u8]) -> Result<Self, PackError> {
        if bytes.len() != MANIFEST_V1_LEN {
            return Err(PackError::InvalidManifestLength);
        }
        if bytes[..4] != *b"MSMF" {
            return Err(PackError::InvalidManifestMagic);
        }
        let version = read_u16(bytes, 4)?;
        let flags = read_u16(bytes, 6)?;
        if version != 1 {
            return Err(PackError::UnsupportedManifestVersion);
        }
        if flags != 0 || read_u32(bytes, 28)? != 0 {
            return Err(PackError::ReservedNotZero);
        }
        let mut dependency_lock_hash = [0_u8; 32];
        dependency_lock_hash.copy_from_slice(&bytes[32..64]);
        Ok(Self {
            runtime_semantics_version: read_u32(bytes, 8)?,
            canonical_leaf_version: read_u32(bytes, 12)?,
            cost_semantics_version: read_u32(bytes, 16)?,
            tie_break_version: read_u32(bytes, 20)?,
            control_protocol_version: read_u32(bytes, 24)?,
            dependency_lock_hash: PackHash(dependency_lock_hash),
        })
    }
}

fn read_u16(bytes: &[u8], offset: usize) -> Result<u16, PackError> {
    let data = bytes.get(offset..offset + 2).ok_or(PackError::TooShort)?;
    Ok(u16::from_le_bytes([data[0], data[1]]))
}

fn read_u32(bytes: &[u8], offset: usize) -> Result<u32, PackError> {
    let data = bytes.get(offset..offset + 4).ok_or(PackError::TooShort)?;
    Ok(u32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}
