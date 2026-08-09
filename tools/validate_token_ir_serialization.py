#!/usr/bin/env python3
from __future__ import annotations

import argparse
import ctypes
import hashlib
import struct
from pathlib import Path

INTEGRITY = 12
OK = 0
HASH_OFF = 112
HASH_LEN = 32
HEADER = 256
ENTRY = 32
SECTIONS = 9


def u32(b: bytes | bytearray, off: int) -> int:
    return struct.unpack_from('<I', b, off)[0]


def u64(b: bytes | bytearray, off: int) -> int:
    return struct.unpack_from('<Q', b, off)[0]


def w32(b: bytearray, off: int, v: int) -> None:
    struct.pack_into('<I', b, off, v)


def w64(b: bytearray, off: int, v: int) -> None:
    struct.pack_into('<Q', b, off, v)


def rehash(b: bytearray) -> None:
    tmp = bytearray(b)
    tmp[HASH_OFF:HASH_OFF + HASH_LEN] = b'\0' * HASH_LEN
    b[HASH_OFF:HASH_OFF + HASH_LEN] = hashlib.sha256(tmp).digest()


def section(b: bytes | bytearray, i: int) -> tuple[int, int, int, int]:
    off = HEADER + i * ENTRY
    return u32(b, off), u64(b, off + 8), u64(b, off + 16), u64(b, off + 24)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('library')
    ap.add_argument('record')
    args = ap.parse_args()
    lib = ctypes.CDLL(str(Path(args.library).resolve()))
    lib.mosaic_token_document_deserialize.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p)]
    lib.mosaic_token_document_deserialize.restype = ctypes.c_int
    lib.mosaic_token_document_free.argtypes = [ctypes.c_void_p]
    lib.mosaic_token_document_free.restype = None

    valid = bytearray(Path(args.record).read_bytes())
    if valid[:8] != b'MSTIRD01' or len(valid) < HEADER + SECTIONS * ENTRY:
        raise SystemExit('not a valid Token IR fixture')

    def status(data: bytes | bytearray) -> int:
        raw = bytes(data)
        buf = ctypes.create_string_buffer(raw, len(raw))
        doc = ctypes.c_void_p()
        st = lib.mosaic_token_document_deserialize(buf, len(raw), ctypes.byref(doc))
        if doc.value:
            lib.mosaic_token_document_free(doc)
        return st

    if status(valid) != OK:
        raise SystemExit('valid Token IR record rejected')

    cases: list[tuple[str, bytearray]] = []
    def case(name: str, mutate, do_rehash: bool = True) -> None:
        b = bytearray(valid)
        mutate(b)
        if do_rehash:
            rehash(b)
        cases.append((name, b))

    case('bad-section-count', lambda b: w32(b, 44, 8))
    case('bad-detection-boolean', lambda b: w32(b, 144, 2))
    case('available-without-match', lambda b: (w32(b, 144, 0), w32(b, 148, 1)))
    case('unterminated-language', lambda b: b.__setitem__(slice(168, 232), b'x' * 64))
    case('nonzero-header-reserved', lambda b: b.__setitem__(232, 1))

    source_off = section(valid, 0)[1]
    case('source-hash-mismatch', lambda b: b.__setitem__(source_off, b[source_off] ^ 1))

    model_off = section(valid, 1)[1]
    case('model-zero-length', lambda b: w64(b, model_off + 16, 0))
    case('model-reserved', lambda b: w32(b, model_off + 4, 1))

    security_off = section(valid, 3)[1]
    if security_off:
        case('security-kind', lambda b: w32(b, security_off, 99))

    lex_off = section(valid, 7)[1]
    if lex_off:
        case('lex-kind', lambda b: w32(b, lex_off, 99))

    sem_off = section(valid, 8)[1]
    if sem_off:
        case('semantic-kind', lambda b: w32(b, sem_off, 99))
        case('semantic-lex-index', lambda b: w64(b, sem_off + 8, (1 << 63)))

    # Move the model section by eight bytes. Whole-record hash is valid, layout is not canonical.
    case('noncanonical-section-offset', lambda b: w64(b, HEADER + ENTRY + 8, model_off + 8))

    # The source length is intentionally not 8-byte aligned in the fixture, so there is canonical zero padding.
    source_end = source_off + section(valid, 0)[2]
    aligned = (source_end + 7) & ~7
    if aligned > source_end:
        case('nonzero-padding', lambda b: b.__setitem__(source_end, 0xA5))

    # Authenticated trailing bytes are still noncanonical because no section owns them.
    def append_trailing(b: bytearray) -> None:
        b.extend(b'\0' * 8)
        w64(b, 24, len(b))
    case('trailing-bytes', append_trailing)

    for name, bad in cases:
        st = status(bad)
        if st != INTEGRITY:
            raise SystemExit(f'{name}: expected integrity({INTEGRITY}), got {st}')

    print(f'OK: Token IR serializer rejected {len(cases)} authenticated malformed records')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
