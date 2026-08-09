#![forbid(unsafe_code)]

use mosaic_core::Source;
use mosaic_ir::{NamespaceId, ProjectedToken, TokenId, TokenKind};

pub const BYTE_NAMESPACE: NamespaceId = NamespaceId(0);
pub const BYTE_KIND: TokenKind = TokenKind(0);

/// Deliberately simple correctness oracle for the byte projection.
///
/// This implementation materializes output for clarity. Production code is
/// free to stream/fill caller buffers as long as it is differential-equivalent.
pub fn project_bytes(source: &impl Source) -> Vec<ProjectedToken> {
    let mut tokens = Vec::with_capacity(usize::try_from(source.len()).unwrap_or(0));
    for leaf in source.canonical_leaves() {
        let byte = source
            .read_byte(leaf.offset().0)
            .expect("canonical leaf must refer to an existing source byte");
        tokens.push(ProjectedToken {
            source: leaf.range(),
            namespace: BYTE_NAMESPACE,
            kind: BYTE_KIND,
            id: TokenId(u32::from(byte)),
        });
    }
    tokens
}


pub fn run_dfa(
    dfa: mosaic_pack::DfaView<'_>,
    input: &[u8],
) -> Result<Option<mosaic_ir::DfaRun>, mosaic_pack::PackError> {
    let mut state = dfa.start_state();
    let mut cost = 0_i64;

    for symbol in input {
        let mut matched = None;
        for index in 0..dfa.transition_count() {
            let transition = dfa.transition(index)?;
            if transition.from == state && transition.symbol == *symbol {
                matched = Some(transition);
                break;
            }
        }
        let Some(transition) = matched else {
            return Ok(None);
        };
        cost = cost
            .checked_add(i64::from(transition.cost))
            .ok_or(mosaic_pack::PackError::CostOverflow)?;
        state = transition.to;
    }

    for index in 0..dfa.accept_count() {
        let accept = dfa.accept(index)?;
        if accept.state == state {
            cost = cost
                .checked_add(i64::from(accept.cost))
                .ok_or(mosaic_pack::PackError::CostOverflow)?;
            return Ok(Some(mosaic_ir::DfaRun { token_id: accept.token_id, cost }));
        }
    }
    Ok(None)
}


pub fn compare_paths(
    left: &[mosaic_ir::CandidateEdge],
    right: &[mosaic_ir::CandidateEdge],
) -> Result<core::cmp::Ordering, mosaic_ir::PathComparisonError> {
    let left_cost = path_cost(left)?;
    let right_cost = path_cost(right)?;
    let by_cost = left_cost.cmp(&right_cost);
    if by_cost != core::cmp::Ordering::Equal {
        return Ok(by_cost);
    }
    let by_count = left.len().cmp(&right.len());
    if by_count != core::cmp::Ordering::Equal {
        return Ok(by_count);
    }
    for (left_edge, right_edge) in left.iter().zip(right) {
        if left_edge == right_edge {
            continue;
        }
        let left_len = edge_len(*left_edge)?;
        let right_len = edge_len(*right_edge)?;
        if left_len != right_len {
            return Ok(right_len.cmp(&left_len));
        }
        let by_namespace = left_edge.key.namespace.cmp(&right_edge.key.namespace);
        if by_namespace != core::cmp::Ordering::Equal {
            return Ok(by_namespace);
        }
        let by_id = left_edge.key.token_id.cmp(&right_edge.key.token_id);
        if by_id != core::cmp::Ordering::Equal {
            return Ok(by_id);
        }
        return Ok(left_edge.key.cmp(&right_edge.key));
    }
    Ok(core::cmp::Ordering::Equal)
}

fn path_cost(edges: &[mosaic_ir::CandidateEdge]) -> Result<i64, mosaic_ir::PathComparisonError> {
    edges.iter().try_fold(0_i64, |total, edge| {
        total
            .checked_add(i64::from(edge.cost.0))
            .ok_or(mosaic_ir::PathComparisonError::CostOverflow)
    })
}

fn edge_len(edge: mosaic_ir::CandidateEdge) -> Result<u64, mosaic_ir::PathComparisonError> {
    edge.key.end.checked_sub(edge.key.start).ok_or(mosaic_ir::PathComparisonError::InvalidEdgeRange)
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
        for index in 0..vocabulary.len() {
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
    for (a, b) in left.iter().zip(right) {
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
    path.iter().try_fold(0_i64, |total, token| {
        total
            .checked_add(i64::from(token.cost))
            .ok_or(mosaic_model::ModelError::CostOverflow)
    })
}
