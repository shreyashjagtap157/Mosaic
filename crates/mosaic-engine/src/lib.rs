#![forbid(unsafe_code)]

use mosaic_core::Source;
use mosaic_ir::{NamespaceId, ProjectedToken, TokenId, TokenKind};

pub const BYTE_NAMESPACE: NamespaceId = NamespaceId(0);
pub const BYTE_KIND: TokenKind = TokenKind(0);

pub fn project_bytes(source: &impl Source) -> Vec<ProjectedToken> {
    (0..source.len())
        .map(|offset| {
            let byte = source
                .read_byte(offset)
                .expect("offset is bounded by source length");
            ProjectedToken {
                source: mosaic_core::ByteRange::new(offset, 1),
                namespace: BYTE_NAMESPACE,
                kind: BYTE_KIND,
                id: TokenId(u32::from(byte)),
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use mosaic_core::BorrowedSource;

    #[test]
    fn optimized_byte_projection_matches_reference() {
        let mut state = 0x9e3779b97f4a7c15_u64;
        for len in 0..1024_usize {
            let mut bytes = vec![0_u8; len];
            for byte in &mut bytes {
                state = state
                    .wrapping_mul(6364136223846793005)
                    .wrapping_add(1442695040888963407);
                *byte = (state >> 32) as u8;
            }
            let source = BorrowedSource::new(&bytes);
            assert_eq!(
                super::project_bytes(&source),
                mosaic_reference::project_bytes(&source)
            );
        }
    }
}

pub fn run_dfa(
    dfa: mosaic_pack::DfaView<'_>,
    input: &[u8],
) -> Result<Option<mosaic_ir::DfaRun>, mosaic_pack::PackError> {
    let mut state = dfa.start_state();
    let mut cost = 0_i64;
    for symbol in input {
        let mut low = 0_u32;
        let mut high = dfa.transition_count();
        let mut matched = None;
        while low < high {
            let middle = low + (high - low) / 2;
            let transition = dfa.transition(middle)?;
            match (transition.from, transition.symbol).cmp(&(state, *symbol)) {
                core::cmp::Ordering::Less => low = middle + 1,
                core::cmp::Ordering::Greater => high = middle,
                core::cmp::Ordering::Equal => {
                    matched = Some(transition);
                    break;
                }
            }
        }
        let Some(transition) = matched else { return Ok(None); };
        cost = cost
            .checked_add(i64::from(transition.cost))
            .ok_or(mosaic_pack::PackError::CostOverflow)?;
        state = transition.to;
    }

    let mut low = 0_u32;
    let mut high = dfa.accept_count();
    while low < high {
        let middle = low + (high - low) / 2;
        let accept = dfa.accept(middle)?;
        match accept.state.cmp(&state) {
            core::cmp::Ordering::Less => low = middle + 1,
            core::cmp::Ordering::Greater => high = middle,
            core::cmp::Ordering::Equal => {
                cost = cost
                    .checked_add(i64::from(accept.cost))
                    .ok_or(mosaic_pack::PackError::CostOverflow)?;
                return Ok(Some(mosaic_ir::DfaRun { token_id: accept.token_id, cost }));
            }
        }
    }
    Ok(None)
}

pub fn compare_paths(
    left: &[mosaic_ir::CandidateEdge],
    right: &[mosaic_ir::CandidateEdge],
) -> Result<core::cmp::Ordering, mosaic_ir::PathComparisonError> {
    let left_cost = sum_cost(left)?;
    let right_cost = sum_cost(right)?;
    if left_cost != right_cost {
        return Ok(left_cost.cmp(&right_cost));
    }
    if left.len() != right.len() {
        return Ok(left.len().cmp(&right.len()));
    }
    for index in 0..left.len() {
        let a = left[index];
        let b = right[index];
        if a == b { continue; }
        let a_len = a.key.end.checked_sub(a.key.start).ok_or(mosaic_ir::PathComparisonError::InvalidEdgeRange)?;
        let b_len = b.key.end.checked_sub(b.key.start).ok_or(mosaic_ir::PathComparisonError::InvalidEdgeRange)?;
        if a_len != b_len { return Ok(b_len.cmp(&a_len)); }
        if a.key.namespace != b.key.namespace { return Ok(a.key.namespace.cmp(&b.key.namespace)); }
        if a.key.token_id != b.key.token_id { return Ok(a.key.token_id.cmp(&b.key.token_id)); }
        return Ok(a.key.cmp(&b.key));
    }
    Ok(core::cmp::Ordering::Equal)
}

fn sum_cost(edges: &[mosaic_ir::CandidateEdge]) -> Result<i64, mosaic_ir::PathComparisonError> {
    let mut total = 0_i64;
    for edge in edges {
        total = total.checked_add(i64::from(edge.cost.0)).ok_or(mosaic_ir::PathComparisonError::CostOverflow)?;
    }
    Ok(total)
}

#[cfg(test)]
mod m2_tests {
    use core::cmp::Ordering;
    use mosaic_ir::{CandidateEdge, CanonicalEdgeKey, ContentHash, EdgeCost, NamespaceId, TokenId, TokenKind};
    use mosaic_pack::{DfaView, PackHash, PackValidationLimits, PackView};

    const DEPENDENCY: PackHash = PackHash([
        0xe9,0x73,0xe5,0xd7,0xe8,0xc5,0x22,0xeb,0x89,0x11,0x6f,0x21,0x09,0x0d,0xfc,0x8b,
        0x96,0x57,0xa2,0x9b,0xc2,0x20,0xa3,0x70,0x2e,0x87,0xf1,0xbb,0x32,0x51,0x52,0x2c,
    ]);

    fn edge(start: u64, end: u64, id: u32, cost: i32) -> CandidateEdge {
        CandidateEdge {
            key: CanonicalEdgeKey {
                start,
                end,
                namespace: NamespaceId(7),
                kind: TokenKind(3),
                token_id: TokenId(id),
                source_pack_hash: ContentHash([9; 32]),
            },
            cost: EdgeCost(cost),
        }
    }

    #[test]
    fn optimized_dfa_matches_linear_reference() {
        let bytes = include_bytes!("../../../fixtures/packs/m2-v1.mpack");
        let limits = PackValidationLimits::DEFAULT;
        let pack = PackView::parse(bytes, limits).expect("pack");
        pack.validate_canonical_dependencies(limits, &[DEPENDENCY]).expect("dependencies");
        let dfa = DfaView::parse(pack.section_bytes(2).expect("dfa bytes"), limits).expect("dfa");
        for input in [b"".as_slice(), b"M".as_slice(), b"X".as_slice(), b"MM".as_slice()] {
            assert_eq!(super::run_dfa(dfa, input), mosaic_reference::run_dfa(dfa, input));
        }
        assert_eq!(super::run_dfa(dfa, b"M").expect("run").expect("accept").cost, 5);
    }

    #[test]
    fn path_order_matches_reference_on_adversarial_ties() {
        let cases: &[(Vec<CandidateEdge>, Vec<CandidateEdge>, Ordering)] = &[
            (vec![edge(0, 1, 1, 1), edge(1, 3, 2, 1)], vec![edge(0, 2, 3, 1), edge(2, 3, 4, 1)], Ordering::Greater),
            (vec![edge(0, 3, 1, 2)], vec![edge(0, 1, 2, 1), edge(1, 3, 3, 1)], Ordering::Less),
            (vec![edge(0, 2, 8, 1), edge(2, 4, 9, 1)], vec![edge(0, 2, 7, 1), edge(2, 4, 10, 1)], Ordering::Greater),
        ];
        for (left, right, expected) in cases {
            assert_eq!(super::compare_paths(left, right).expect("engine compare"), *expected);
            assert_eq!(mosaic_reference::compare_paths(left, right).expect("reference compare"), *expected);
        }
    }
}

pub fn tokenize_viterbi(
    vocabulary: mosaic_pack::VocabularyView<'_>,
    input: &[u8],
) -> Result<Vec<mosaic_model::EncodedToken>, mosaic_model::ModelError> {
    let mut best: Vec<Option<Vec<mosaic_model::EncodedToken>>> = vec![None; input.len() + 1];
    best[0] = Some(Vec::new());

    for start in 0..input.len() {
        let Some(prefix) = best[start].clone() else {
            continue;
        };
        let range = vocabulary.candidate_range_for_first_byte(input[start])?;
        for index in range {
            let entry = vocabulary.entry(index)?;
            if input[start..].starts_with(entry.surface) {
                let end = start
                    .checked_add(entry.surface.len())
                    .ok_or(mosaic_model::ModelError::InputTooLarge)?;
                let mut candidate = prefix.clone();
                candidate.push(mosaic_model::EncodedToken {
                    start,
                    end,
                    token_id: mosaic_ir::TokenId(entry.token_id),
                    cost: entry.cost,
                });
                if best[end]
                    .as_ref()
                    .is_none_or(|current| compare_model_paths(&candidate, current).is_lt())
                {
                    best[end] = Some(candidate);
                }
            }
        }
    }

    best[input.len()]
        .take()
        .ok_or(mosaic_model::ModelError::InvalidVocabularySectionCount)
}

pub fn compare_model_paths(
    left: &[mosaic_model::EncodedToken],
    right: &[mosaic_model::EncodedToken],
) -> core::cmp::Ordering {
    let left_cost = model_path_cost(left);
    let right_cost = model_path_cost(right);
    match (left_cost, right_cost) {
        (Ok(a), Ok(b)) if a != b => return a.cmp(&b),
        (Err(_), Ok(_)) => return core::cmp::Ordering::Greater,
        (Ok(_), Err(_)) => return core::cmp::Ordering::Less,
        _ => {}
    }
    if left.len() != right.len() {
        return left.len().cmp(&right.len());
    }
    for index in 0..left.len() {
        let a = left[index];
        let b = right[index];
        if a == b {
            continue;
        }
        let a_len = a.end.saturating_sub(a.start);
        let b_len = b.end.saturating_sub(b.start);
        if a_len != b_len {
            return b_len.cmp(&a_len);
        }
        if a.token_id != b.token_id {
            return a.token_id.cmp(&b.token_id);
        }
        if a.start != b.start {
            return a.start.cmp(&b.start);
        }
        if a.end != b.end {
            return a.end.cmp(&b.end);
        }
        return a.cost.cmp(&b.cost);
    }
    core::cmp::Ordering::Equal
}

fn model_path_cost(path: &[mosaic_model::EncodedToken]) -> Result<i64, mosaic_model::ModelError> {
    let mut total = 0_i64;
    for token in path {
        total = total
            .checked_add(i64::from(token.cost))
            .ok_or(mosaic_model::ModelError::CostOverflow)?;
    }
    Ok(total)
}

#[cfg(test)]
mod m3_model_tests {
    use mosaic_model::{decode_ids, vocabulary_from_pack};
    use mosaic_pack::{PackValidationLimits, PackView};

    #[test]
    fn reference_and_engine_viterbi_match_and_round_trip() {
        let bytes = include_bytes!("../../../fixtures/packs/m3-model-v1.mpack");
        let limits = PackValidationLimits::DEFAULT;
        let pack = PackView::parse(bytes, limits).expect("pack");
        pack.validate_canonical_dependencies(limits, &[]).expect("self-contained pack");
        let vocabulary = vocabulary_from_pack(pack, limits).expect("vocabulary");
        let cases: &[&[u8]] = &[
            b"",
            b"hello",
            b"hello world",
            b"the tokenizer",
            b"\x00\xffhello\x80world",
        ];
        for input in cases {
            let reference = mosaic_reference::tokenize_viterbi(vocabulary, input).expect("reference");
            let engine = super::tokenize_viterbi(vocabulary, input).expect("engine");
            assert_eq!(engine, reference);
            let ids: Vec<_> = engine.iter().map(|token| token.token_id).collect();
            assert_eq!(decode_ids(vocabulary, &ids).expect("decode"), *input);
        }
    }
}
