#[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
#[repr(C)]
pub struct PackHash(pub [u8; 32]);

impl PackHash {
    pub const ZERO: Self = Self([0; 32]);
}

// Small, safe, no_std SHA-256 implementation used for the first canonical
// executable pack profile. BLAKE3 remains a later supported/default profile.
const K: [u32; 64] = [
    0x428a_2f98,
    0x7137_4491,
    0xb5c0_fbcf,
    0xe9b5_dba5,
    0x3956_c25b,
    0x59f1_11f1,
    0x923f_82a4,
    0xab1c_5ed5,
    0xd807_aa98,
    0x1283_5b01,
    0x2431_85be,
    0x550c_7dc3,
    0x72be_5d74,
    0x80de_b1fe,
    0x9bdc_06a7,
    0xc19b_f174,
    0xe49b_69c1,
    0xefbe_4786,
    0x0fc1_9dc6,
    0x240c_a1cc,
    0x2de9_2c6f,
    0x4a74_84aa,
    0x5cb0_a9dc,
    0x76f9_88da,
    0x983e_5152,
    0xa831_c66d,
    0xb003_27c8,
    0xbf59_7fc7,
    0xc6e0_0bf3,
    0xd5a7_9147,
    0x06ca_6351,
    0x1429_2967,
    0x27b7_0a85,
    0x2e1b_2138,
    0x4d2c_6dfc,
    0x5338_0d13,
    0x650a_7354,
    0x766a_0abb,
    0x81c2_c92e,
    0x9272_2c85,
    0xa2bf_e8a1,
    0xa81a_664b,
    0xc24b_8b70,
    0xc76c_51a3,
    0xd192_e819,
    0xd699_0624,
    0xf40e_3585,
    0x106a_a070,
    0x19a4_c116,
    0x1e37_6c08,
    0x2748_774c,
    0x34b0_bcb5,
    0x391c_0cb3,
    0x4ed8_aa4a,
    0x5b9c_ca4f,
    0x682e_6ff3,
    0x748f_82ee,
    0x78a5_636f,
    0x84c8_7814,
    0x8cc7_0208,
    0x90be_fffa,
    0xa450_6ceb,
    0xbef9_a3f7,
    0xc671_78f2,
];

#[derive(Clone, Debug)]
pub struct Sha256 {
    state: [u32; 8],
    block: [u8; 64],
    block_len: usize,
    total_len: u64,
}

impl Default for Sha256 {
    fn default() -> Self {
        Self::new()
    }
}

impl Sha256 {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            state: [
                0x6a09_e667,
                0xbb67_ae85,
                0x3c6e_f372,
                0xa54f_f53a,
                0x510e_527f,
                0x9b05_688c,
                0x1f83_d9ab,
                0x5be0_cd19,
            ],
            block: [0; 64],
            block_len: 0,
            total_len: 0,
        }
    }

    /// Adds bytes to the current SHA-256 state.
    ///
    /// # Panics
    ///
    /// Panics if the total input length exceeds SHA-256's u64 byte accounting.
    pub fn update(&mut self, mut input: &[u8]) {
        self.total_len = self
            .total_len
            .checked_add(
                u64::try_from(input.len()).expect("slice length must fit Mosaic u64 coordinates"),
            )
            .expect("SHA-256 input length exceeds u64 byte accounting");

        if self.block_len != 0 {
            let take = core::cmp::min(64 - self.block_len, input.len());
            self.block[self.block_len..self.block_len + take].copy_from_slice(&input[..take]);
            self.block_len += take;
            input = &input[take..];
            if self.block_len == 64 {
                let block = self.block;
                self.compress(&block);
                self.block_len = 0;
            } else {
                return;
            }
        }

        while input.len() >= 64 {
            let (block, rest) = input.split_at(64);
            let array: &[u8; 64] = block.try_into().expect("64-byte split must convert");
            self.compress(array);
            input = rest;
        }

        self.block[..input.len()].copy_from_slice(input);
        self.block_len = input.len();
    }

    #[must_use]
    /// Finishes the digest and returns the 32-byte hash.
    ///
    /// # Panics
    ///
    /// Panics if the accumulated byte length cannot be represented as bits.
    pub fn finalize(mut self) -> PackHash {
        let bit_len = self
            .total_len
            .checked_mul(8)
            .expect("SHA-256 bit length overflow");
        self.block[self.block_len] = 0x80;
        self.block_len += 1;

        if self.block_len > 56 {
            self.block[self.block_len..].fill(0);
            let block = self.block;
            self.compress(&block);
            self.block = [0; 64];
            self.block_len = 0;
        }

        self.block[self.block_len..56].fill(0);
        self.block[56..64].copy_from_slice(&bit_len.to_be_bytes());
        let block = self.block;
        self.compress(&block);

        let mut output = [0_u8; 32];
        for (chunk, word) in output.chunks_exact_mut(4).zip(self.state) {
            chunk.copy_from_slice(&word.to_be_bytes());
        }
        PackHash(output)
    }

    fn compress(&mut self, block: &[u8; 64]) {
        let mut schedule = [0_u32; 64];
        for (index, chunk) in block.chunks_exact(4).enumerate() {
            schedule[index] = u32::from_be_bytes([chunk[0], chunk[1], chunk[2], chunk[3]]);
        }
        for index in 16..64 {
            let s0 = schedule[index - 15].rotate_right(7)
                ^ schedule[index - 15].rotate_right(18)
                ^ (schedule[index - 15] >> 3);
            let s1 = schedule[index - 2].rotate_right(17)
                ^ schedule[index - 2].rotate_right(19)
                ^ (schedule[index - 2] >> 10);
            schedule[index] = schedule[index - 16]
                .wrapping_add(s0)
                .wrapping_add(schedule[index - 7])
                .wrapping_add(s1);
        }

        let [
            mut state0,
            mut state1,
            mut state2,
            mut state3,
            mut state4,
            mut state5,
            mut state6,
            mut state7,
        ] = self.state;
        for index in 0..64 {
            let s1 = state4.rotate_right(6) ^ state4.rotate_right(11) ^ state4.rotate_right(25);
            let ch = (state4 & state5) ^ ((!state4) & state6);
            let temp1 = state7
                .wrapping_add(s1)
                .wrapping_add(ch)
                .wrapping_add(K[index])
                .wrapping_add(schedule[index]);
            let s0 = state0.rotate_right(2) ^ state0.rotate_right(13) ^ state0.rotate_right(22);
            let maj = (state0 & state1) ^ (state0 & state2) ^ (state1 & state2);
            let temp2 = s0.wrapping_add(maj);

            state7 = state6;
            state6 = state5;
            state5 = state4;
            state4 = state3.wrapping_add(temp1);
            state3 = state2;
            state2 = state1;
            state1 = state0;
            state0 = temp1.wrapping_add(temp2);
        }

        self.state[0] = self.state[0].wrapping_add(state0);
        self.state[1] = self.state[1].wrapping_add(state1);
        self.state[2] = self.state[2].wrapping_add(state2);
        self.state[3] = self.state[3].wrapping_add(state3);
        self.state[4] = self.state[4].wrapping_add(state4);
        self.state[5] = self.state[5].wrapping_add(state5);
        self.state[6] = self.state[6].wrapping_add(state6);
        self.state[7] = self.state[7].wrapping_add(state7);
    }
}

#[must_use]
pub fn sha256(input: &[u8]) -> PackHash {
    let mut hash = Sha256::new();
    hash.update(input);
    hash.finalize()
}

#[cfg(test)]
mod tests {
    use super::{PackHash, sha256};

    fn hex(value: &str) -> PackHash {
        let bytes = value.as_bytes();
        assert_eq!(bytes.len(), 64);
        let mut out = [0_u8; 32];
        for index in 0..32 {
            let hi = nibble(bytes[index * 2]);
            let lo = nibble(bytes[index * 2 + 1]);
            out[index] = (hi << 4) | lo;
        }
        PackHash(out)
    }

    fn nibble(value: u8) -> u8 {
        match value {
            b'0'..=b'9' => value - b'0',
            b'a'..=b'f' => value - b'a' + 10,
            _ => panic!("invalid test hex"),
        }
    }

    #[test]
    fn known_vectors() {
        assert_eq!(
            sha256(b""),
            hex("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")
        );
        assert_eq!(
            sha256(b"abc"),
            hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        );

        let mut split = super::Sha256::new();
        split.update(b"a");
        split.update(b"b");
        split.update(b"c");
        assert_eq!(
            split.finalize(),
            hex("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")
        );

        let mut million_a = super::Sha256::new();
        for _ in 0..1_000_000 {
            million_a.update(b"a");
        }
        assert_eq!(
            million_a.finalize(),
            hex("cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")
        );
    }
}
