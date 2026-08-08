# Mosaic Declarative Lexer Pack v1

Status: stable format for Mosaic Tokenizer 0.14.x

## Purpose

Lexer pack v1 adds exact, deterministic lexical projections for compiler, IDE, formatter, and structured-text consumers without adding native plug-ins or a general scanner VM. The source bytes remain authoritative; lexer tokens are derived byte spans and always form an exact ordered partition of the input.

## Outer pack contract

Lexer packs use the ordinary checked Mosaic pack container with section kind `9`. The outer pack remains subject to canonical hashing, manifest/lock validation, bounds checking, canonical zero padding, and resource limits.

## Inner payload

The lexer section starts with `MSLX`, format version 1, followed by fixed-width little-endian metadata for:

- line-comment delimiters;
- block-comment start/end delimiter pairs and optional nesting;
- string delimiters and optional one-byte escape;
- sorted keyword surfaces;
- profile name;
- identifier policy flags;
- canonical byte blob containing all variable-length surfaces;
- maximum delimiter length.

All reserved bits/fields MUST be zero. Delimiter and keyword tables are canonically sorted in the pack. Runtime recognition nevertheless applies longest-prefix selection at the current source byte so prefix-related delimiters such as `"` and `"""` behave deterministically.

## Lexical kinds

1. whitespace
2. newline
3. identifier
4. keyword
5. number
6. string
7. comment
8. punctuation
9. error

`ERROR` is an exact source span used for unterminated string or block-comment constructs. The lexer never discards source bytes to recover.

## Exactness

For input length `N`, returned lexical spans MUST satisfy:

- first start = 0;
- every token end equals the next token start;
- final end = N;
- every token length is non-zero for non-empty input;
- concatenating the referenced source spans reconstructs the input byte-for-byte.

## v1 behavior

- CRLF is one newline token; lone CR or LF is one newline token.
- Consecutive non-newline ASCII whitespace is grouped.
- Identifiers accept ASCII letters and `_`; `$` is optionally enabled by profile. Continuations also accept digits. Bytes >= 0x80 are accepted as identifier bytes rather than destructively decoded/replaced.
- Keyword lookup applies only to complete identifier spans.
- Number recognition is intentionally lexical rather than numeric-semantic. Numeric semantic enrichment belongs above this layer.
- Block comments may be nested only when the profile declares it.
- Strings support arbitrary non-empty byte delimiters and one optional escape byte.
- All unmatched bytes become one-byte punctuation spans.

## Security/resource model

Lexer packs contain no native code. v1 is bounded by pack counts, pack byte lengths, delimiter lengths, and source length. Malformed v1 payloads fail before attachment. Mosaic ships dedicated malformed lexer fixtures for magic/version/reserved/count/order/flag/max-delimiter failures.

## Deliberate non-features

v1 does not include:

- arbitrary regex execution at runtime;
- native callbacks;
- parser feedback;
- indentation synthesis;
- heredoc terminators dependent on captured runtime strings;
- context-sensitive scanner bytecode.

Those mechanisms may be introduced by a future version only after reference language profiles prove declarative v1 insufficient and the alternative passes a new security/resource ADR.
