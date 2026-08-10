use crate::{PackError, PackHash, PackValidationLimits, Sha256};

const DOMAIN: &[u8] = b"MOSAIC-TOKENIZER-MANIFEST-V1\0";

#[derive(Clone, Copy, Debug, Eq, Ord, PartialEq, PartialOrd)]
#[repr(u16)]
pub enum PackRole {
    Unicode = 1,
    Script = 2,
    Language = 3,
    Locale = 4,
    Domain = 5,
    Lexer = 6,
    Model = 7,
    Security = 8,
    Detector = 9,
}

impl PackRole {
    #[must_use]
    pub const fn code(self) -> u16 {
        match self {
            Self::Unicode => 1,
            Self::Script => 2,
            Self::Language => 3,
            Self::Locale => 4,
            Self::Domain => 5,
            Self::Lexer => 6,
            Self::Model => 7,
            Self::Security => 8,
            Self::Detector => 9,
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
#[repr(C)]
pub struct ManifestPackRef {
    pub role: PackRole,
    pub ordinal: u16,
    pub content_hash: PackHash,
}

#[derive(Clone, Copy, Debug)]
pub struct TokenizerManifestV1<'a> {
    pub runtime_semantics_version: u32,
    pub canonical_leaf_version: u32,
    pub cost_semantics_version: u32,
    pub tie_break_version: u32,
    pub control_protocol_version: u32,
    pub routing_policy_version: u32,
    pub normalization_view_id: u32,
    pub resource_policy_hash: PackHash,
    packs: &'a [ManifestPackRef],
}

impl<'a> TokenizerManifestV1<'a> {
    #[allow(clippy::too_many_arguments)]
    /// Builds a canonical tokenizer execution manifest view.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if pack references exceed configured limits, contain
    /// zero hashes, or are not in canonical role/ordinal order.
    pub fn new(
        runtime_semantics_version: u32,
        canonical_leaf_version: u32,
        cost_semantics_version: u32,
        tie_break_version: u32,
        control_protocol_version: u32,
        routing_policy_version: u32,
        normalization_view_id: u32,
        resource_policy_hash: PackHash,
        packs: &'a [ManifestPackRef],
        limits: PackValidationLimits,
    ) -> Result<Self, PackError> {
        let pack_count =
            u32::try_from(packs.len()).map_err(|_| PackError::ResourceLimitExceeded)?;
        if pack_count > limits.max_manifest_packs {
            return Err(PackError::ResourceLimitExceeded);
        }
        if resource_policy_hash == PackHash::ZERO {
            return Err(PackError::ZeroResourcePolicyHash);
        }
        let mut previous: Option<(PackRole, u16)> = None;
        for pack in packs {
            if pack.content_hash == PackHash::ZERO {
                return Err(PackError::ZeroDependencyHash);
            }
            let key = (pack.role, pack.ordinal);
            if previous.is_some_and(|prior| prior >= key) {
                return Err(PackError::ManifestPacksNotCanonical);
            }
            previous = Some(key);
        }
        Ok(Self {
            runtime_semantics_version,
            canonical_leaf_version,
            cost_semantics_version,
            tie_break_version,
            control_protocol_version,
            routing_policy_version,
            normalization_view_id,
            resource_policy_hash,
            packs,
        })
    }

    #[must_use]
    pub const fn packs(self) -> &'a [ManifestPackRef] {
        self.packs
    }

    /// Computes the deterministic manifest identity.
    ///
    /// # Panics
    ///
    /// Panics if the pack count no longer fits in `u32` after construction.
    #[must_use]
    pub fn identity(self) -> PackHash {
        let mut hash = Sha256::new();
        hash.update(DOMAIN);
        for value in [
            self.runtime_semantics_version,
            self.canonical_leaf_version,
            self.cost_semantics_version,
            self.tie_break_version,
            self.control_protocol_version,
            self.routing_policy_version,
            self.normalization_view_id,
        ] {
            hash.update(&value.to_le_bytes());
        }
        hash.update(&self.resource_policy_hash.0);
        let count =
            u32::try_from(self.packs.len()).expect("manifest pack count validated at construction");
        hash.update(&count.to_le_bytes());
        for pack in self.packs {
            hash.update(&pack.role.code().to_le_bytes());
            hash.update(&pack.ordinal.to_le_bytes());
            hash.update(&pack.content_hash.0);
        }
        hash.finalize()
    }
}
