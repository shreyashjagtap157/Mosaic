#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct SourceId(pub [u8; 16]);

#[derive(Clone, Copy, Debug, Default, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(transparent)]
pub struct SourceVersion(pub u64);

#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(C)]
pub struct SourceIdentity {
    pub id: SourceId,
    pub version: SourceVersion,
}

impl SourceIdentity {
    #[must_use]
    pub const fn new(id: [u8; 16], version: u64) -> Self {
        Self {
            id: SourceId(id),
            version: SourceVersion(version),
        }
    }
}
