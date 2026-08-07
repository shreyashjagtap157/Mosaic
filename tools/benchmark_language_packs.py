#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C, os, time, statistics
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB',ROOT/'build/libmosaic.so'))
MODEL=ROOT/'fixtures/packs/model-v2.mpack'; UNI=ROOT/'fixtures/packs/unicode17-v1.mpack'; LANG=ROOT/'fixtures/packs/language'
P8=C.POINTER(C.c_uint8); P32=C.POINTER(C.c_uint32)

def main():
    l=C.CDLL(str(LIB));
    l.mosaic_tokenizer_load_files.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)];l.mosaic_tokenizer_load_files.restype=C.c_int
    l.mosaic_tokenizer_add_language_file.argtypes=[C.c_void_p,C.c_char_p];l.mosaic_tokenizer_add_language_file.restype=C.c_int
    l.mosaic_tokenizer_encode.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(P32),C.POINTER(C.c_size_t)];l.mosaic_tokenizer_encode.restype=C.c_int
    l.mosaic_tokenizer_free.argtypes=[C.c_void_p];l.mosaic_free.argtypes=[C.c_void_p]
    chunk=b'tokenizer '+ 'नमस्ते दुनिया'.encode()+b' '+ 'こんにちは世界'.encode()+b' hello world\n'
    n=5*1024*1024; data=(chunk*(n//len(chunk)+1))[:n]; arr=(C.c_uint8*len(data)).from_buffer_copy(data); ptr=C.cast(arr,P8)
    def load():
        t=C.c_void_p();assert l.mosaic_tokenizer_load_files(os.fsencode(MODEL),os.fsencode(UNI),C.byref(t))==0;return t
    def timed(t):
        samples=[]; count=0
        # One unmeasured warm-up makes the gate less sensitive to first-touch/page-cache noise.
        ids=P32();nn=C.c_size_t();assert l.mosaic_tokenizer_encode(t,ptr,len(data),C.byref(ids),C.byref(nn))==0;l.mosaic_free(ids)
        for _ in range(3):
            ids=P32();nn=C.c_size_t();start=time.perf_counter();assert l.mosaic_tokenizer_encode(t,ptr,len(data),C.byref(ids),C.byref(nn))==0;samples.append(time.perf_counter()-start)
            count=nn.value;l.mosaic_free(ids)
        return statistics.median(samples),count
    base=load(); spec=load()
    try:
        for tag in ('en','hi','ja'): assert l.mosaic_tokenizer_add_language_file(spec,os.fsencode(LANG/f'{tag}-v1.mpack'))==0
        bt,bc=timed(base);st,sc=timed(spec)
    finally:l.mosaic_tokenizer_free(base);l.mosaic_tokenizer_free(spec)
    token_reduction=1-sc/bc; overhead=st/bt-1
    if token_reduction < .20: raise SystemExit(f'FAIL: specialization token reduction {token_reduction:.1%} < 20% controlled floor')
    if overhead > .50: raise SystemExit(f'FAIL: specialization encoder overhead {overhead:.1%} > 50% floor')
    print(f'PASS language benchmark: base_tokens={bc} specialized_tokens={sc} reduction={token_reduction:.1%} base={bt:.3f}s specialized={st:.3f}s overhead={overhead:.1%}')
    return 0
if __name__=='__main__':raise SystemExit(main())
