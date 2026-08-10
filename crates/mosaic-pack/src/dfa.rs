use crate::{PackError, PackValidationLimits};

const HEADER_LEN: usize = 24;
const TRANSITION_LEN: usize = 16;
const ACCEPT_LEN: usize = 12;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct DfaTransition {
    pub from: u32,
    pub to: u32,
    pub cost: i32,
    pub symbol: u8,
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct DfaAccept {
    pub state: u32,
    pub token_id: u32,
    pub cost: i32,
}

#[derive(Clone, Copy, Debug)]
pub struct DfaView<'a> {
    bytes: &'a [u8],
    state_count: u32,
    start_state: u32,
    transition_count: u32,
    accept_count: u32,
}

impl<'a> DfaView<'a> {
    /// Parses and validates a canonical DFA section.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if the section header, layout, limits, state
    /// references, or canonical ordering are invalid.
    pub fn parse(bytes: &'a [u8], limits: PackValidationLimits) -> Result<Self, PackError> {
        if bytes.len() < HEADER_LEN || bytes[..4] != *b"MSDF" {
            return Err(PackError::InvalidDfaHeader);
        }
        if read_u16(bytes, 4)? != 1 {
            return Err(PackError::UnsupportedDfaVersion);
        }
        if read_u16(bytes, 6)? != 0 {
            return Err(PackError::ReservedNotZero);
        }
        let state_count = read_u32(bytes, 8)?;
        let start_state = read_u32(bytes, 12)?;
        let transition_count = read_u32(bytes, 16)?;
        let accept_count = read_u32(bytes, 20)?;
        if state_count == 0 || state_count > limits.max_dfa_states {
            return Err(PackError::ResourceLimitExceeded);
        }
        if transition_count > limits.max_dfa_transitions || accept_count > state_count {
            return Err(PackError::ResourceLimitExceeded);
        }
        if start_state >= state_count {
            return Err(PackError::DfaStateOutOfBounds);
        }
        let transitions_bytes = usize::try_from(transition_count)
            .map_err(|_| PackError::IntegerOverflow)?
            .checked_mul(TRANSITION_LEN)
            .ok_or(PackError::IntegerOverflow)?;
        let accepts_bytes = usize::try_from(accept_count)
            .map_err(|_| PackError::IntegerOverflow)?
            .checked_mul(ACCEPT_LEN)
            .ok_or(PackError::IntegerOverflow)?;
        let expected = HEADER_LEN
            .checked_add(transitions_bytes)
            .and_then(|value| value.checked_add(accepts_bytes))
            .ok_or(PackError::IntegerOverflow)?;
        if expected != bytes.len() {
            return Err(PackError::InvalidDfaLayout);
        }

        let view = Self {
            bytes,
            state_count,
            start_state,
            transition_count,
            accept_count,
        };
        let mut prior_transition: Option<(u32, u8)> = None;
        for index in 0..transition_count {
            let transition = view.transition(index)?;
            if transition.from >= state_count || transition.to >= state_count {
                return Err(PackError::DfaStateOutOfBounds);
            }
            let key = (transition.from, transition.symbol);
            if prior_transition.is_some_and(|prior| prior >= key) {
                return Err(PackError::DfaTransitionsNotStrictlySorted);
            }
            prior_transition = Some(key);
        }
        let mut prior_accept: Option<u32> = None;
        for index in 0..accept_count {
            let accept = view.accept(index)?;
            if accept.state >= state_count {
                return Err(PackError::DfaStateOutOfBounds);
            }
            if prior_accept.is_some_and(|prior| prior >= accept.state) {
                return Err(PackError::DfaAcceptsNotStrictlySorted);
            }
            prior_accept = Some(accept.state);
        }
        Ok(view)
    }

    #[must_use]
    pub const fn state_count(self) -> u32 {
        self.state_count
    }
    #[must_use]
    pub const fn start_state(self) -> u32 {
        self.start_state
    }
    #[must_use]
    pub const fn transition_count(self) -> u32 {
        self.transition_count
    }
    #[must_use]
    pub const fn accept_count(self) -> u32 {
        self.accept_count
    }

    /// Returns the transition at `index`.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if `index` is out of bounds or the transition bytes
    /// are not a supported canonical entry.
    pub fn transition(self, index: u32) -> Result<DfaTransition, PackError> {
        if index >= self.transition_count {
            return Err(PackError::DfaIndexOutOfBounds);
        }
        let index_bytes = usize::try_from(index)
            .map_err(|_| PackError::IntegerOverflow)?
            .checked_mul(TRANSITION_LEN)
            .ok_or(PackError::IntegerOverflow)?;
        let base = HEADER_LEN
            .checked_add(index_bytes)
            .ok_or(PackError::IntegerOverflow)?;
        let end = base
            .checked_add(TRANSITION_LEN)
            .ok_or(PackError::IntegerOverflow)?;
        let data = self
            .bytes
            .get(base..end)
            .ok_or(PackError::InvalidDfaLayout)?;
        if data[13] != 0 {
            return Err(PackError::UnsupportedDfaTransitionFlags);
        }
        if read_u16(data, 14)? != 0 {
            return Err(PackError::ReservedNotZero);
        }
        Ok(DfaTransition {
            from: read_u32(data, 0)?,
            to: read_u32(data, 4)?,
            cost: read_i32(data, 8)?,
            symbol: data[12],
        })
    }

    /// Returns the accepting-state entry at `index`.
    ///
    /// # Errors
    ///
    /// Returns `PackError` if `index` is out of bounds or the entry cannot be
    /// read from the section bytes.
    pub fn accept(self, index: u32) -> Result<DfaAccept, PackError> {
        if index >= self.accept_count {
            return Err(PackError::DfaIndexOutOfBounds);
        }
        let transitions_bytes = usize::try_from(self.transition_count)
            .map_err(|_| PackError::IntegerOverflow)?
            .checked_mul(TRANSITION_LEN)
            .ok_or(PackError::IntegerOverflow)?;
        let accept_bytes = usize::try_from(index)
            .map_err(|_| PackError::IntegerOverflow)?
            .checked_mul(ACCEPT_LEN)
            .ok_or(PackError::IntegerOverflow)?;
        let base = HEADER_LEN
            .checked_add(transitions_bytes)
            .and_then(|value| value.checked_add(accept_bytes))
            .ok_or(PackError::IntegerOverflow)?;
        let end = base
            .checked_add(ACCEPT_LEN)
            .ok_or(PackError::IntegerOverflow)?;
        let data = self
            .bytes
            .get(base..end)
            .ok_or(PackError::InvalidDfaLayout)?;
        Ok(DfaAccept {
            state: read_u32(data, 0)?,
            token_id: read_u32(data, 4)?,
            cost: read_i32(data, 8)?,
        })
    }
}

fn checked_slice(bytes: &[u8], offset: usize, len: usize) -> Result<&[u8], PackError> {
    let end = offset.checked_add(len).ok_or(PackError::IntegerOverflow)?;
    bytes.get(offset..end).ok_or(PackError::TooShort)
}

fn read_u16(bytes: &[u8], offset: usize) -> Result<u16, PackError> {
    let data = checked_slice(bytes, offset, 2)?;
    Ok(u16::from_le_bytes([data[0], data[1]]))
}

fn read_u32(bytes: &[u8], offset: usize) -> Result<u32, PackError> {
    let data = checked_slice(bytes, offset, 4)?;
    Ok(u32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}

fn read_i32(bytes: &[u8], offset: usize) -> Result<i32, PackError> {
    let data = checked_slice(bytes, offset, 4)?;
    Ok(i32::from_le_bytes([data[0], data[1], data[2], data[3]]))
}
