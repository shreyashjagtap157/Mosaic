#!/usr/bin/env python3
from __future__ import annotations
import ctypes as C, os, statistics, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
LIB=Path(os.environ.get('MOSAIC_LIB',ROOT/'build/libmosaic.so'));MODEL=ROOT/'fixtures/packs/model-v2.mpack';UNI=ROOT/'fixtures/packs/unicode17-v1.mpack';LANG=ROOT/'fixtures/packs/language';DET=ROOT/'fixtures/packs/detector/reference-v1.mpack'
P8=C.POINTER(C.c_uint8);P32=C.POINTER(C.c_uint32)
class Detection(C.Structure):_fields_=[('matched',C.c_uint32),('available',C.c_uint32),('score',C.c_int64),('margin',C.c_int64),('language',C.c_char*64)]
def setup():
 l=C.CDLL(str(LIB));l.mosaic_tokenizer_load_files.argtypes=[C.c_char_p,C.c_char_p,C.POINTER(C.c_void_p)];l.mosaic_tokenizer_load_files.restype=C.c_int;l.mosaic_tokenizer_add_language_file.argtypes=[C.c_void_p,C.c_char_p];l.mosaic_tokenizer_add_language_file.restype=C.c_int;l.mosaic_tokenizer_set_detector_file.argtypes=[C.c_void_p,C.c_char_p];l.mosaic_tokenizer_set_detector_file.restype=C.c_int;l.mosaic_tokenizer_encode.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(P32),C.POINTER(C.c_size_t)];l.mosaic_tokenizer_encode.restype=C.c_int;l.mosaic_tokenizer_encode_auto.argtypes=[C.c_void_p,P8,C.c_size_t,C.POINTER(P32),C.POINTER(C.c_size_t),C.POINTER(Detection)];l.mosaic_tokenizer_encode_auto.restype=C.c_int;l.mosaic_tokenizer_free.argtypes=[C.c_void_p];l.mosaic_free.argtypes=[C.c_void_p];return l
def load(l,tags,det=False):
 t=C.c_void_p();assert l.mosaic_tokenizer_load_files(os.fsencode(MODEL),os.fsencode(UNI),C.byref(t))==0
 for tag in tags:assert l.mosaic_tokenizer_add_language_file(t,os.fsencode(LANG/f'{tag}-v1.mpack'))==0
 if det:assert l.mosaic_tokenizer_set_detector_file(t,os.fsencode(DET))==0
 return t
def ptr(data):a=(C.c_uint8*len(data)).from_buffer_copy(data);return a,C.cast(a,P8)
def run_encode(l,t,data,auto):
 a,p=ptr(data);ids=P32();n=C.c_size_t();d=Detection();start=time.perf_counter();
 if auto:st=l.mosaic_tokenizer_encode_auto(t,p,len(data),C.byref(ids),C.byref(n),C.byref(d))
 else:st=l.mosaic_tokenizer_encode(t,p,len(data),C.byref(ids),C.byref(n))
 elapsed=time.perf_counter()-start;count=n.value;l.mosaic_free(ids);assert st==0;return elapsed,count,d
def main():
 l=setup();samples={'en':b'tokenizer hello world ','hi':('नमस्ते दुनिया '.encode()),'ja':('こんにちは世界 '.encode())};worst=-1.0
 for tag,chunk in samples.items():
  data=(chunk*((2*1024*1024//len(chunk))+1))[:2*1024*1024];explicit=load(l,(tag,));auto=load(l,('en','hi','ja'),True)
  try:
   run_encode(l,explicit,data,False);run_encode(l,auto,data,True)
   es=[];aus=[];count=None
   for _ in range(3):
    e,n,_=run_encode(l,explicit,data,False);a,an,d=run_encode(l,auto,data,True);assert n==an and d.matched and d.available and d.language.decode()==tag;es.append(e);aus.append(a);count=n
   em=statistics.median(es);am=statistics.median(aus);over=(am/em-1)*100;worst=max(worst,over);print(f'{tag}: tokens={count} explicit={em:.4f}s auto={am:.4f}s routing_overhead={over:.1f}%')
  finally:l.mosaic_tokenizer_free(explicit);l.mosaic_tokenizer_free(auto)
 if worst>50:raise SystemExit(f'FAIL: detector routing overhead {worst:.1f}% > 50%')
 print(f'PASS detector benchmark: worst median routing overhead={worst:.1f}%')
if __name__=='__main__':raise SystemExit(main())
