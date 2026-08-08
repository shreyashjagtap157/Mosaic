#!/usr/bin/env python3
"""Deterministic authoring tool for Mosaic v1 model/language/detector/security/normalization-aware packs.

This tool is intentionally dependency-free. It produces the same checked MOSPACK
container consumed by the native runtime. Training is compression-first and
fully deterministic; it is a practical tokenizer authoring baseline, not a
claim of state-of-the-art statistical Unigram training.
"""
from __future__ import annotations

import argparse
import base64
import collections
import hashlib
import json
import math
import re
import struct
import sys
from pathlib import Path

MAGIC = b"MOSPACK\0"
HEADER = 96
SECTION_ENTRY = 32
FIRST_BYTE_INDEX_COUNT = 257


def align(value: int, boundary: int = 8) -> int:
    return (value + boundary - 1) // boundary * boundary


def build_lock() -> bytes:
    return b"MSLK" + struct.pack("<HHII", 1, 48, 0, 0)


def build_manifest(lock: bytes) -> bytes:
    prefix = b"MSMF" + struct.pack("<HHIIIIII", 1, 0, 1, 1, 1, 1, 1, 0)
    return prefix + hashlib.sha256(lock).digest()


def wrap_pack(section_kind: int, section: bytes, format_minor: int) -> bytes:
    lock = build_lock()
    manifest = build_manifest(lock)
    sections = [(1, manifest), (2, lock), (section_kind, section)]
    cursor = align(HEADER + SECTION_ENTRY * len(sections))
    payload = bytearray(cursor)
    entries: list[bytes] = []
    for kind, data in sections:
        cursor = align(cursor)
        payload.extend(bytes(cursor - len(payload)))
        off = cursor
        payload.extend(data)
        cursor += len(data)
        entries.append(struct.pack("<IIQQIHBB", kind, 0, off, len(data), 0, 0, 3, 0))
    header = bytearray(HEADER)
    header[:8] = MAGIC
    struct.pack_into("<HHHHQIHHQII", header, 8,
                     1, format_minor, HEADER, 0, len(payload), len(sections), SECTION_ENTRY,
                     1, HEADER, 0, 1)
    payload[:HEADER] = header
    for i, entry in enumerate(entries):
        payload[HEADER + i * SECTION_ENTRY:HEADER + (i + 1) * SECTION_ENTRY] = entry
    canonical = bytearray(payload)
    canonical[48:80] = bytes(32)
    payload[48:80] = hashlib.sha256(canonical).digest()
    return bytes(payload)


def surface_from_obj(obj: dict) -> bytes:
    has_text = "text" in obj
    has_hex = "hex" in obj
    if has_text == has_hex:
        raise ValueError("piece must specify exactly one of text or hex")
    if has_text:
        return str(obj["text"]).encode("utf-8")
    try:
        return bytes.fromhex(str(obj["hex"]))
    except ValueError as exc:
        raise ValueError(f"invalid hex surface: {obj['hex']!r}") from exc


def canonical_model_rows(pieces: list[dict], byte_cost: int) -> list[tuple[int, int, bytes]]:
    if not (1 <= byte_cost <= 2_000_000_000):
        raise ValueError("byte_cost must be between 1 and 2,000,000,000")
    rows: list[tuple[int, int, bytes]] = [(i, byte_cost, bytes([i])) for i in range(256)]
    parsed: list[tuple[int | None, int, bytes]] = []
    seen_surface = {bytes([i]) for i in range(256)}
    seen_ids = set(range(256))
    for obj in pieces:
        surface = surface_from_obj(obj)
        if not surface:
            raise ValueError("empty vocabulary surface")
        if len(surface) > 65535:
            raise ValueError("surface exceeds v1 65535-byte length")
        if surface in seen_surface:
            raise ValueError(f"duplicate vocabulary surface: {surface!r}")
        seen_surface.add(surface)
        cost = int(obj.get("cost", max(1, byte_cost - 1)))
        if not (-2_000_000_000 <= cost <= 2_000_000_000):
            raise ValueError("piece cost outside supported i32 range")
        tid = obj.get("id")
        if tid is not None:
            tid = int(tid)
            if not (256 <= tid <= 0xFFFFFFFF) or tid in seen_ids:
                raise ValueError(f"invalid/duplicate token id: {tid}")
            seen_ids.add(tid)
        parsed.append((tid, cost, surface))
    # Missing IDs are assigned after sorting by surface, so JSON input order cannot affect the pack.
    next_id = 256
    for tid, cost, surface in sorted(parsed, key=lambda x: (x[2], x[1], x[0] if x[0] is not None else 0x1_0000_0000)):
        if tid is None:
            while next_id in seen_ids:
                next_id += 1
            tid = next_id
            seen_ids.add(tid)
            next_id += 1
        rows.append((tid, cost, surface))
    return rows


def build_vocab_section(rows: list[tuple[int, int, bytes]], algorithm: int = 0) -> bytes:
    if len(rows) > 2_000_000:
        raise ValueError("vocabulary exceeds authoring safety limit")
    ordered = sorted(rows, key=lambda r: (r[2], r[0]))
    if len({r[0] for r in ordered}) != len(ordered):
        raise ValueError("token IDs must be unique")
    if len({r[2] for r in ordered}) != len(ordered):
        raise ValueError("surfaces must be unique")
    for value in range(256):
        matches = [r for r in ordered if r[2] == bytes([value])]
        if len(matches) != 1:
            raise ValueError(f"missing mandatory byte fallback 0x{value:02x}")
    blob = bytearray()
    entries: list[bytes] = []
    max_surface = 0
    for token_id, cost, surface in ordered:
        off = len(blob)
        blob += surface
        max_surface = max(max_surface, len(surface))
        entries.append(struct.pack("<IiIHH", token_id, cost, off, len(surface), 0))
    id_sorted = sorted(range(len(ordered)), key=lambda i: ordered[i][0])
    first = [0] * FIRST_BYTE_INDEX_COUNT
    cur = 0
    for b in range(256):
        first[b] = cur
        while cur < len(ordered) and ordered[cur][2][0] == b:
            cur += 1
    first[256] = cur
    entries_off = 40
    id_off = align(entries_off + len(entries) * 16, 4)
    first_off = align(id_off + len(id_sorted) * 4, 4)
    blob_off = align(first_off + FIRST_BYTE_INDEX_COUNT * 4, 8)
    out = bytearray(blob_off + len(blob))
    out[:4] = b"MSVC"
    # Last u32 was reserved in v1; keep it zero. Runtime derives max surface length during validation.
    if algorithm not in (0, 1):
        raise ValueError("unsupported model algorithm")
    struct.pack_into("<HHIHHIIIIII", out, 4, 1, 0, len(entries), 16, 0,
                     entries_off, id_off, first_off, blob_off, len(blob), algorithm)
    for i, entry in enumerate(entries):
        out[entries_off + i * 16:entries_off + (i + 1) * 16] = entry
    for i, idx in enumerate(id_sorted):
        struct.pack_into("<I", out, id_off + i * 4, idx)
    for i, value in enumerate(first):
        struct.pack_into("<I", out, first_off + i * 4, value)
    out[blob_off:] = blob
    return bytes(out)


def canonical_bpe_rows(pieces: list[dict]) -> list[tuple[int, int, bytes]]:
    rows=[(i,i,bytes([i])) for i in range(256)]
    seen_surface={bytes([i]) for i in range(256)}; seen_ids=set(range(256)); seen_ranks=set(range(256))
    parsed=[]
    for obj in pieces:
        surface=surface_from_obj(obj)
        if not surface or len(surface)>65535 or surface in seen_surface:
            raise ValueError("invalid/duplicate BPE surface")
        seen_surface.add(surface)
        rank=obj.get("rank",obj.get("cost")); tid=obj.get("id")
        parsed.append((surface,rank,tid))
    next_rank=256
    for surface,rank,tid in sorted(parsed,key=lambda x:x[0]):
        if rank is None:
            while next_rank in seen_ranks: next_rank+=1
            rank=next_rank;next_rank+=1
        rank=int(rank)
        if rank<0 or rank>2_000_000_000 or rank in seen_ranks: raise ValueError("invalid/duplicate BPE rank")
        if tid is None: tid=rank
        tid=int(tid)
        if tid<0 or tid>0xffffffff or tid in seen_ids: raise ValueError("invalid/duplicate BPE token id")
        seen_ranks.add(rank);seen_ids.add(tid);rows.append((tid,rank,surface))
    return rows

def build_model_pack(pieces: list[dict], byte_cost: int, algorithm: int = 0) -> bytes:
    rows=canonical_bpe_rows(pieces) if algorithm==1 else canonical_model_rows(pieces, byte_cost)
    return wrap_pack(4, build_vocab_section(rows, algorithm), 2)

def build_model_pack_rows(rows: list[tuple[int, int, bytes]], algorithm: int) -> bytes:
    return wrap_pack(4, build_vocab_section(rows, algorithm), 2)


def text_segments(data: bytes) -> list[bytes]:
    """Extract deterministic Unicode-aware candidate segments, preserving arbitrary bytes via fallback."""
    try:
        text = data.decode("utf-8", "strict")
    except UnicodeDecodeError:
        # For mixed/binary corpora, mine conservative raw runs around ASCII whitespace.
        return [part for part in re.split(rb"([\x09-\x0d\x20]+)", data) if part]
    return [m.group(0).encode("utf-8") for m in re.finditer(r"\w+|[^\w\s]+|\s+", text, re.UNICODE)]


def mine_candidates(paths: list[Path], max_piece_bytes: int, min_frequency: int, max_candidates: int) -> collections.Counter[bytes]:
    counts: collections.Counter[bytes] = collections.Counter()

    def process_record(data: bytes) -> None:
        segments = text_segments(data)
        for i, seg in enumerate(segments):
            if 2 <= len(seg) <= max_piece_bytes:
                counts[seg] += 1
            # Neighbor compositions capture leading-space words and common word+punctuation phrases.
            total = bytearray()
            for width in (2, 3):
                if i + width > len(segments):
                    break
                total.clear()
                for part in segments[i:i + width]:
                    total += part
                if 2 <= len(total) <= max_piece_bytes:
                    counts[bytes(total)] += 1
            # Long word-like segments get deterministic character-boundary prefixes/suffixes.
            if len(seg) > max_piece_bytes:
                try:
                    chars = seg.decode("utf-8", "strict")
                    encoded_prefix = bytearray()
                    for ch in chars:
                        encoded_prefix += ch.encode("utf-8")
                        if 2 <= len(encoded_prefix) <= max_piece_bytes:
                            counts[bytes(encoded_prefix)] += 1
                        if len(encoded_prefix) > max_piece_bytes:
                            break
                    encoded_suffix = bytearray()
                    for ch in reversed(chars):
                        encoded_suffix[:0] = ch.encode("utf-8")
                        if 2 <= len(encoded_suffix) <= max_piece_bytes:
                            counts[bytes(encoded_suffix)] += 1
                        if len(encoded_suffix) > max_piece_bytes:
                            break
                except UnicodeDecodeError:
                    pass
        if len(counts) > max_candidates:
            raise ValueError(
                f"candidate map exceeded --max-candidates={max_candidates}; "
                "lower max piece size or shard the corpus"
            )

    for path in sorted(paths, key=lambda p: str(p.resolve())):
        # Record-wise processing bounds source memory to the longest record instead of the whole corpus.
        # Training pieces intentionally do not cross record/newline boundaries.
        had_record = False
        with path.open("rb") as fh:
            for data in fh:
                had_record = True
                process_record(data)
        if not had_record:
            process_record(b"")
    for key in list(counts):
        if counts[key] < min_frequency or len(key) < 2:
            del counts[key]
    return counts

def train_pieces(paths: list[Path], vocab_size: int, max_piece_bytes: int,
                 min_frequency: int, byte_cost: int, max_candidates: int) -> tuple[list[dict], list[dict]]:
    if vocab_size < 256 or vocab_size > 1_000_000:
        raise ValueError("vocab_size must be in [256, 1,000,000]")
    if not (2 <= max_piece_bytes <= 65535):
        raise ValueError("max_piece_bytes must be in [2, 65535]")
    if min_frequency < 1:
        raise ValueError("min_frequency must be >= 1")
    if not (10_000 <= max_candidates <= 20_000_000):
        raise ValueError("max_candidates must be in [10,000, 20,000,000]")
    counts = mine_candidates(paths, max_piece_bytes, min_frequency, max_candidates)
    ranked = sorted(
        counts.items(),
        key=lambda kv: (-(kv[1] * (len(kv[0]) - 1)), -kv[1], -len(kv[0]), kv[0]),
    )[:max(0, vocab_size - 256)]
    pieces: list[dict] = []
    report: list[dict] = []
    for surface, freq in ranked:
        # Frequency chooses membership; deterministic integer serving cost mildly favors frequent/long pieces.
        bonus = min(byte_cost - 1, int(80 * math.log2(freq + 1) + 25 * (len(surface) - 1)))
        cost = max(1, byte_cost - bonus)
        pieces.append({"hex": surface.hex(), "cost": cost})
        report.append({"hex": surface.hex(), "utf8": surface.decode("utf-8", "replace"),
                       "frequency": freq, "gain": freq * (len(surface) - 1), "cost": cost})
    return pieces, report


def build_language_section(tag: str, rows: list[tuple[bytes, int]]) -> bytes:
    tagb = tag.encode("ascii")
    if not tagb or len(tagb) > 63 or not re.fullmatch(rb"[A-Za-z0-9-]+", tagb):
        raise ValueError("language tag must be 1..63 ASCII BCP47-style characters")
    ordered = sorted(rows, key=lambda x: x[0])
    if len({s for s, _ in ordered}) != len(ordered):
        raise ValueError("duplicate language surface")
    blob = bytearray(); entries = []
    maxlen = 0
    for surface, delta in ordered:
        if not surface or len(surface) > 65535:
            raise ValueError("invalid language surface length")
        if not (-2_000_000_000 <= delta <= 2_000_000_000):
            raise ValueError("language delta outside supported i32 range")
        off = len(blob); blob += surface; maxlen = max(maxlen, len(surface))
        entries.append(struct.pack("<IHHii", off, len(surface), 0, delta, 0))
    entries_off = 40
    tag_off = entries_off + len(entries) * 16
    blob_off = align(tag_off + len(tagb), 8)
    out = bytearray(blob_off + len(blob)); out[:4] = b"MSLG"
    struct.pack_into("<HHIHHIIIIII", out, 4, 1, 0, len(entries), 16, len(tagb),
                     entries_off, tag_off, blob_off, len(blob), maxlen, 0)
    for i, entry in enumerate(entries):
        out[entries_off + i * 16:entries_off + (i + 1) * 16] = entry
    out[tag_off:tag_off + len(tagb)] = tagb
    out[blob_off:] = blob
    return bytes(out)


def build_language_pack(config: dict) -> bytes:
    tag = str(config["language"])
    rows = [(surface_from_obj(x), int(x["delta"])) for x in config.get("adjustments", [])]
    return wrap_pack(5, build_language_section(tag, rows), 2)


def build_detector_section(config: dict) -> bytes:
    profiles_cfg = config.get("profiles", {})
    if not isinstance(profiles_cfg, dict) or not profiles_cfg:
        raise ValueError("detector requires non-empty profiles object")
    tags = sorted(profiles_cfg)
    if len(tags) > 256:
        raise ValueError("v1 detector supports at most 256 profiles")
    min_margin = int(config.get("min_margin", 20))
    blob = bytearray(); tag_locs = {}
    for tag in tags:
        tb = tag.encode("ascii")
        if not tb or len(tb) > 63 or not re.fullmatch(rb"[A-Za-z0-9-]+", tb):
            raise ValueError(f"invalid detector language tag: {tag!r}")
        off = len(blob); blob += tb; tag_locs[tag] = (off, len(tb))
    features = []
    for pidx, tag in enumerate(tags):
        cfg = profiles_cfg[tag]
        for item in cfg.get("features", []):
            surface = surface_from_obj(item); weight = int(item["weight"])
            if not surface or len(surface) > 65535 or weight <= 0:
                raise ValueError("detector features require non-empty <=65535-byte surface and positive weight")
            off = len(blob); blob += surface
            features.append((surface[0], surface, pidx, weight, off))
    features.sort(key=lambda x: (x[0], x[1], x[2]))
    if len({(x[1], x[2]) for x in features}) != len(features):
        raise ValueError("duplicate detector feature for profile")
    profiles_off = 48
    features_off = align(profiles_off + len(tags) * 16, 8)
    first_off = align(features_off + len(features) * 16, 4)
    blob_off = align(first_off + 257 * 4, 8)
    out = bytearray(blob_off + len(blob)); out[:4] = b"MSDT"
    max_feature = max((len(x[1]) for x in features), default=0)
    struct.pack_into("<HHIIHHIIIIIII", out, 4, 1, 0, len(tags), len(features), 16, 16,
                     profiles_off, features_off, first_off, blob_off, len(blob), max_feature, min_margin)
    for i, tag in enumerate(tags):
        off, length = tag_locs[tag]
        min_score = int(profiles_cfg[tag].get("min_score", 1))
        struct.pack_into("<IHHii", out, profiles_off + i * 16, off, length, 0, min_score, 0)
    for i, (_, surface, pidx, weight, off) in enumerate(features):
        struct.pack_into("<IHHii", out, features_off + i * 16, off, len(surface), pidx, weight, 0)
    first = [0] * 257; cur = 0
    for b in range(256):
        first[b] = cur
        while cur < len(features) and features[cur][0] == b:
            cur += 1
    first[256] = cur
    for i, value in enumerate(first):
        struct.pack_into("<I", out, first_off + i * 4, value)
    out[blob_off:] = blob
    return bytes(out)


def build_detector_pack(config: dict) -> bytes:
    return wrap_pack(6, build_detector_section(config), 3)


def write_pack(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    print(json.dumps({"output": str(path), "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}, sort_keys=True))


def read_outer(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < HEADER or data[:8] != MAGIC:
        raise ValueError("not a MOSPACK v1 container")
    major, minor, hsize, flags = struct.unpack_from("<HHHH", data, 8)
    file_len = struct.unpack_from("<Q", data, 16)[0]
    count = struct.unpack_from("<I", data, 24)[0]
    entry_size = struct.unpack_from("<H", data, 28)[0]
    directory = struct.unpack_from("<Q", data, 32)[0]
    if major != 1 or hsize != HEADER or flags != 0 or file_len != len(data) or entry_size != SECTION_ENTRY:
        raise ValueError("unsupported/invalid MOSPACK header")
    canonical = bytearray(data); expected = bytes(canonical[48:80]); canonical[48:80] = bytes(32)
    actual = hashlib.sha256(canonical).digest()
    if directory < HEADER or directory > len(data):
        raise ValueError("invalid section-directory offset")
    directory_bytes = count * SECTION_ENTRY
    if count > 1_000_000 or directory_bytes > len(data) - directory:
        raise ValueError("section directory exceeds file bounds")
    sections = []
    for i in range(count):
        off = directory + i * SECTION_ENTRY
        kind, sflags, start, length = struct.unpack_from("<IIQQ", data, off)
        if sflags != 0 or start > len(data) or length > len(data) - start:
            raise ValueError(f"invalid section directory entry {i}")
        sections.append({"kind": kind, "flags": sflags, "offset": start, "length": length})
    return {"format": f"{major}.{minor}", "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest(),
            "canonical_hash_valid": expected == actual, "sections": sections}


def load_json(path: Path) -> dict:
    obj = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(obj, dict):
        raise ValueError("configuration root must be an object")
    return obj


def build_lexer_pack(cfg: dict) -> bytes:
    name=str(cfg.get("name","")).encode("utf-8")
    if not name or len(name)>63 or b"\0" in name: raise ValueError("lexer name must be 1..63 UTF-8 bytes without NUL")
    flags=1 if bool(cfg.get("dollar_identifiers",False)) else 0
    blob=bytearray(); offsets={}
    def intern(raw: bytes) -> tuple[int,int]:
        if not raw or len(raw)>65535: raise ValueError("lexer delimiter/keyword length outside v1 range")
        if raw in offsets:return offsets[raw]
        off=len(blob);blob.extend(raw);offsets[raw]=(off,len(raw));return off,len(raw)
    def text(x) -> bytes:
        if not isinstance(x,str): raise ValueError("lexer strings must be JSON strings")
        return x.encode("utf-8")
    line=[]
    for raw in sorted({text(x) for x in cfg.get("line_comments",[]) }):
        off,n=intern(raw);line.append(struct.pack("<IHH",off,n,0))
    blocks=[]
    parsed=[]
    for obj in cfg.get("block_comments",[]):
        a=text(obj["start"]);z=text(obj["end"]); parsed.append((a,z,1 if obj.get("nested",False) else 0))
    for a,z,nested in sorted(parsed,key=lambda x:(x[0],x[1],x[2])):
        ao,an=intern(a);zo,zn=intern(z);blocks.append(struct.pack("<IIHHHH",ao,zo,an,zn,nested,0))
    strings=[];parsed=[]
    for obj in cfg.get("strings",[]):
        d=text(obj["delimiter"]);esc=obj.get("escape","")
        if esc is None: esc=""
        eb=text(esc)
        if len(eb)>1: raise ValueError("lexer escape must encode to zero or one byte")
        parsed.append((d,eb[0] if eb else 0))
    for d,esc in sorted(parsed,key=lambda x:(x[0],x[1])):
        off,n=intern(d);strings.append(struct.pack("<IHBBI",off,n,esc,0,0))
    keywords=[]
    for raw in sorted({text(x) for x in cfg.get("keywords",[]) }):
        off,n=intern(raw);keywords.append(struct.pack("<IHH",off,n,0))
    name_off,name_len=intern(name)
    maxd=max([0]+[len(x) for x in {text(v) for v in cfg.get("line_comments",[])}])
    for obj in cfg.get("block_comments",[]):maxd=max(maxd,len(text(obj["start"])),len(text(obj["end"])))
    for obj in cfg.get("strings",[]):maxd=max(maxd,len(text(obj["delimiter"])))
    line_off=64; block_off=align(line_off+len(line)*8,8); string_off=align(block_off+len(blocks)*16,8)
    keyword_off=align(string_off+len(strings)*12,8); blob_off=align(keyword_off+len(keywords)*8,8)
    out=bytearray(blob_off+len(blob));out[:4]=b"MSLX"
    struct.pack_into("<HHIIIIIIIIIHHIIII",out,4,1,0,len(line),len(blocks),len(strings),len(keywords),line_off,block_off,string_off,keyword_off,name_off,name_len,flags,blob_off,len(blob),maxd,0)
    for i,r in enumerate(line):out[line_off+i*8:line_off+(i+1)*8]=r
    for i,r in enumerate(blocks):out[block_off+i*16:block_off+(i+1)*16]=r
    for i,r in enumerate(strings):out[string_off+i*12:string_off+(i+1)*12]=r
    for i,r in enumerate(keywords):out[keyword_off+i*8:keyword_off+(i+1)*8]=r
    out[blob_off:]=blob
    return wrap_pack(9,bytes(out),8)


def cmd_model(args) -> None:
    cfg = load_json(args.config)
    algorithm_name = str(cfg.get("algorithm", "viterbi")).lower()
    algorithm = {"viterbi": 0, "bpe": 1}.get(algorithm_name)
    if algorithm is None:
        raise ValueError("algorithm must be viterbi or bpe")
    write_pack(args.output, build_model_pack(cfg.get("pieces", []), int(cfg.get("byte_cost", 1000)), algorithm))

def parse_tiktoken(path: Path) -> list[tuple[int, int, bytes]]:
    rows=[]; seen_surface=set(); seen_rank=set()
    for lineno, raw in enumerate(path.read_bytes().splitlines(), 1):
        if not raw.strip(): continue
        parts=raw.split()
        if len(parts)!=2: raise ValueError(f"invalid .tiktoken line {lineno}")
        try:
            surface=base64.b64decode(parts[0],validate=True);rank=int(parts[1])
        except Exception as exc:
            raise ValueError(f"invalid .tiktoken line {lineno}") from exc
        if not surface or len(surface)>65535 or rank<0 or rank>2_000_000_000:
            raise ValueError(f"invalid .tiktoken token at line {lineno}")
        if surface in seen_surface or rank in seen_rank:
            raise ValueError(f"duplicate .tiktoken surface/rank at line {lineno}")
        seen_surface.add(surface);seen_rank.add(rank);rows.append((rank,rank,surface))
    for byte in range(256):
        if bytes([byte]) not in seen_surface: raise ValueError(f".tiktoken vocabulary missing byte fallback 0x{byte:02x}")
    return rows

def model_rows_from_pack(path: Path) -> tuple[int,list[tuple[int,int,bytes]]]:
    data=path.read_bytes();info=read_outer(path)
    vocab=[x for x in info["sections"] if x["kind"]==4]
    if len(vocab)!=1: raise ValueError("model pack must contain exactly one vocabulary section")
    sec=vocab[0]; start=sec["offset"]; length=sec["length"]
    blob=data[start:start+length]
    if len(blob)<40 or blob[:4]!=b"MSVC" or struct.unpack_from("<H",blob,4)[0]!=1: raise ValueError("unsupported vocabulary section")
    count=struct.unpack_from("<I",blob,8)[0]; esize=struct.unpack_from("<H",blob,12)[0]
    entries,id_off,first_off,blob_off,blob_len,algorithm=struct.unpack_from("<IIIIII",blob,16)
    if esize!=16 or algorithm not in (0,1) or entries!=40 or entries+count*16>len(blob) or blob_off+blob_len!=len(blob): raise ValueError("invalid vocabulary layout")
    rows=[]
    for i in range(count):
        off=entries+i*16;tid,cost,surf_off,surf_len,res=struct.unpack_from("<IiIHH",blob,off)
        if res or not surf_len or surf_off>blob_len or surf_len>blob_len-surf_off: raise ValueError("invalid vocabulary entry")
        rows.append((tid,cost,bytes(blob[blob_off+surf_off:blob_off+surf_off+surf_len])))
    return algorithm,rows

def cmd_import_tiktoken(args) -> None:
    rows=parse_tiktoken(args.input)
    write_pack(args.output,build_model_pack_rows(rows,1))

def cmd_export_tiktoken(args) -> None:
    algorithm,rows=model_rows_from_pack(args.input)
    if algorithm!=1: raise ValueError("only raw-BPE model packs can be exported as .tiktoken")
    if any(tid!=rank for tid,rank,_ in rows): raise ValueError(".tiktoken export requires token id == BPE rank for every piece")
    out=[]
    for tid,rank,surface in sorted(rows,key=lambda r:r[0]):
        out.append(base64.b64encode(surface)+b" "+str(rank).encode("ascii")+b"\n")
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(b"".join(out))
    print(json.dumps({"output":str(args.output),"tokens":len(rows),"sha256":hashlib.sha256(args.output.read_bytes()).hexdigest()},sort_keys=True))


def cmd_train(args) -> None:
    paths = [Path(x) for x in args.corpus]
    for p in paths:
        if not p.is_file():
            raise ValueError(f"corpus file not found: {p}")
    pieces, report = train_pieces(paths, args.vocab_size, args.max_piece_bytes, args.min_frequency, args.byte_cost, args.max_candidates)
    data = build_model_pack(pieces, args.byte_cost)
    write_pack(args.output, data)
    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps({"settings": vars_without_paths(args), "pieces": report}, indent=2, ensure_ascii=False, sort_keys=True) + "\n", encoding="utf-8")


def vars_without_paths(args) -> dict:
    out = {}
    for k, v in vars(args).items():
        if k in {"func", "corpus", "output", "report"}:
            continue
        out[k] = v
    return out


def cmd_language(args) -> None:
    write_pack(args.output, build_language_pack(load_json(args.config)))


def cmd_detector(args) -> None:
    write_pack(args.output, build_detector_pack(load_json(args.config)))

def cmd_lexer(args) -> None:
    write_pack(args.output, build_lexer_pack(load_json(args.config)))


def cmd_inspect(args) -> None:
    info=read_outer(args.pack)
    kinds={section["kind"] for section in info["sections"]}
    if 8 in kinds: info["pack_class"]="unicode-normalization"
    elif 9 in kinds: info["pack_class"]="lexer"
    elif 7 in kinds: info["pack_class"]="unicode-security"
    elif 6 in kinds: info["pack_class"]="detector"
    elif 5 in kinds: info["pack_class"]="language-or-unicode"
    if 4 in kinds:
        algorithm,rows=model_rows_from_pack(args.pack)
        info["model_algorithm"]={0:"viterbi",1:"raw-bpe"}[algorithm]
        info["vocabulary_size"]=len(rows)
    print(json.dumps(info, indent=2, sort_keys=True))


TRUST_DOMAIN = b"MOSAIC-PACK-SIGNATURE-v1"

def _load_crypto():
    try:
        from cryptography.hazmat.primitives.serialization import load_pem_private_key, load_pem_public_key, Encoding, PublicFormat
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
    except ImportError as exc:
        raise ValueError("trust authoring requires the optional 'cryptography' package") from exc
    return load_pem_private_key, load_pem_public_key, Encoding, PublicFormat, Ed25519PrivateKey, Ed25519PublicKey

def cmd_sign_pack(args) -> None:
    load_priv, _, Encoding, PublicFormat, Ed25519PrivateKey, _ = _load_crypto()
    key = load_priv(args.private_key.read_bytes(), password=None)
    if not isinstance(key, Ed25519PrivateKey): raise ValueError("private key must be Ed25519 PEM")
    public = key.public_key().public_bytes(Encoding.Raw, PublicFormat.Raw)
    key_id = hashlib.sha256(public).digest(); pack_hash = hashlib.sha256(args.pack.read_bytes()).digest()
    sig = key.sign(TRUST_DOMAIN + key_id + pack_hash)
    record = b"MSSIGV01" + struct.pack("<IIII", 1, 160, 0, 1) + key_id + pack_hash + sig + b"\0" * 8
    if len(record) != 160: raise ValueError("internal signature record size mismatch")
    args.output.write_bytes(record)
    print(json.dumps({"output": str(args.output), "key_id": key_id.hex(), "pack_sha256": pack_hash.hex(), "signature_record_sha256": hashlib.sha256(record).hexdigest()}, sort_keys=True))

def cmd_key_id(args) -> None:
    _, load_pub, Encoding, PublicFormat, _, Ed25519PublicKey = _load_crypto()
    key = load_pub(args.public_key.read_bytes())
    if not isinstance(key, Ed25519PublicKey): raise ValueError("public key must be Ed25519 PEM")
    raw = key.public_bytes(Encoding.Raw, PublicFormat.Raw)
    print(hashlib.sha256(raw).hexdigest())

def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(prog="mosaic-author", description="Deterministic Mosaic pack authoring")
    p.add_argument("--version", action="version", version="mosaic-author 0.26.0")
    sub = p.add_subparsers(dest="command", required=True)
    m = sub.add_parser("model", help="compile model vocabulary from JSON")
    m.add_argument("config", type=Path); m.add_argument("output", type=Path); m.set_defaults(func=cmd_model)
    t = sub.add_parser("train-model", help="train a deterministic compression-first model from corpora")
    t.add_argument("corpus", nargs="+"); t.add_argument("-o", "--output", required=True, type=Path)
    t.add_argument("--report", type=Path); t.add_argument("--vocab-size", type=int, default=4096)
    t.add_argument("--max-piece-bytes", type=int, default=32); t.add_argument("--min-frequency", type=int, default=2)
    t.add_argument("--byte-cost", type=int, default=1000); t.add_argument("--max-candidates", type=int, default=2_000_000)
    t.set_defaults(func=cmd_train)
    tk = sub.add_parser("import-tiktoken", help="import a .tiktoken mergeable-ranks file as raw-byte BPE")
    tk.add_argument("input", type=Path); tk.add_argument("output", type=Path); tk.set_defaults(func=cmd_import_tiktoken)
    tke = sub.add_parser("export-tiktoken", help="export an exactly rank-compatible raw-BPE model to .tiktoken")
    tke.add_argument("input", type=Path); tke.add_argument("output", type=Path); tke.set_defaults(func=cmd_export_tiktoken)
    l = sub.add_parser("language", help="compile language specialization JSON")
    l.add_argument("config", type=Path); l.add_argument("output", type=Path); l.set_defaults(func=cmd_language)
    d = sub.add_parser("detector", help="compile detector JSON")
    d.add_argument("config", type=Path); d.add_argument("output", type=Path); d.set_defaults(func=cmd_detector)
    lx = sub.add_parser("lexer", help="compile declarative lexer profile JSON")
    lx.add_argument("config", type=Path); lx.add_argument("output", type=Path); lx.set_defaults(func=cmd_lexer)
    sp = sub.add_parser("sign-pack", help="sign exact pack identity with an Ed25519 PEM private key")
    sp.add_argument("pack", type=Path); sp.add_argument("private_key", type=Path); sp.add_argument("output", type=Path); sp.set_defaults(func=cmd_sign_pack)
    ki = sub.add_parser("key-id", help="print SHA-256 key id for an Ed25519 PEM public key")
    ki.add_argument("public_key", type=Path); ki.set_defaults(func=cmd_key_id)
    i = sub.add_parser("inspect", help="inspect MOSPACK container metadata")
    i.add_argument("pack", type=Path); i.set_defaults(func=cmd_inspect)
    return p


def main() -> int:
    try:
        args = parser().parse_args(); args.func(args); return 0
    except (ValueError, KeyError, OSError, json.JSONDecodeError, struct.error) as exc:
        print(f"mosaic-author: error: {exc}", file=sys.stderr); return 2


if __name__ == "__main__":
    raise SystemExit(main())
