#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json, os, re, sqlite3, struct, sys, tempfile, time
from contextlib import closing
from dataclasses import asdict, dataclass
from pathlib import Path

SCHEMA=1
SEMVER_RE=re.compile(r'^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$')
DOMAIN=b'MOSAIC-PACK-SIGNATURE-v1'

def canon(obj)->bytes:return (json.dumps(obj,sort_keys=True,separators=(',',':'),ensure_ascii=False)+'\n').encode()
def sha256_bytes(b:bytes)->str:return hashlib.sha256(b).hexdigest()
def sha256_file(p:Path)->str:
    h=hashlib.sha256()
    with p.open('rb') as f:
        for chunk in iter(lambda:f.read(1<<20),b''):h.update(chunk)
    return h.hexdigest()
def semver(s:str)->tuple[int,int,int]:
    m=SEMVER_RE.fullmatch(s)
    if not m:raise ValueError(f'version must be numeric SemVer MAJOR.MINOR.PATCH: {s!r}')
    return tuple(map(int,m.groups()))
def check_constraint(version:str,constraint:str)->bool:
    v=semver(version); c=constraint.strip()
    if not c or c in {'*','latest'}:raise ValueError('mutable/unbounded aliases are not valid registry constraints')
    for part in [x.strip() for x in c.split(',') if x.strip()]:
        if part.startswith('^'):
            b=semver(part[1:]); upper=(b[0]+1,0,0) if b[0] else ((0,b[1]+1,0) if b[1] else (0,0,b[2]+1))
            if not (v>=b and v<upper):return False
            continue
        op='=='
        for x in ('>=','<=','==','>','<'):
            if part.startswith(x):op=x;part=part[len(x):].strip();break
        b=semver(part)
        if not {'==':v==b,'>=':v>=b,'<=':v<=b,'>':v>b,'<':v<b}[op]:return False
    return True

def inspect_pack(path:Path)->dict:
    data=path.read_bytes()
    if len(data)<96 or data[:8]!=b"MOSPACK\0":raise ValueError("not a MOSPACK v1 container")
    major,minor,hsize,flags=struct.unpack_from("<HHHH",data,8);file_len=struct.unpack_from("<Q",data,16)[0]
    count=struct.unpack_from("<I",data,24)[0];entry_size=struct.unpack_from("<H",data,28)[0];directory=struct.unpack_from("<Q",data,32)[0]
    if major!=1 or hsize!=96 or flags!=0 or file_len!=len(data) or entry_size!=32:raise ValueError("unsupported/invalid MOSPACK header")
    canonical=bytearray(data);expected=bytes(canonical[48:80]);canonical[48:80]=bytes(32);actual=hashlib.sha256(canonical).digest()
    if expected!=actual:raise ValueError("pack canonical hash invalid")
    if directory<96 or directory>len(data) or count>1_000_000 or count*32>len(data)-directory:raise ValueError("invalid section directory")
    kinds=set()
    for i in range(count):
        off=directory+i*32;kind,sflags,begin,length=struct.unpack_from("<IIQQ",data,off)
        if sflags!=0 or begin>len(data) or length>len(data)-begin:raise ValueError(f"invalid section directory entry {i}")
        kinds.add(kind)
    cls=('model' if 4 in kinds else 'lexer' if 9 in kinds else 'normalization' if 8 in kinds else 'security' if 7 in kinds else 'detector' if 6 in kinds else 'language-or-unicode' if 5 in kinds else 'generic')
    return {'pack_class':cls,'format':f'{major}.{minor}','canonical_hash_valid':True}

def verify_ed25519_signature(pack:bytes,signature:bytes,public_key_bytes:bytes)->str:
    try:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
        from cryptography.hazmat.primitives.serialization import load_pem_public_key,Encoding,PublicFormat
    except ImportError as exc:raise ValueError("signature verification requires optional 'cryptography' package") from exc
    if len(signature)!=160 or signature[:8]!=b'MSSIGV01':raise ValueError('invalid Mosaic signature record')
    version,header,flags,algorithm=struct.unpack_from('<IIII',signature,8)
    if (version,header,flags,algorithm)!=(1,160,0,1) or any(signature[152:]):raise ValueError('invalid Mosaic signature metadata')
    if public_key_bytes.startswith(b'-----BEGIN'):
        key=load_pem_public_key(public_key_bytes)
        if not isinstance(key,Ed25519PublicKey):raise ValueError('public key must be Ed25519')
        raw=key.public_bytes(Encoding.Raw,PublicFormat.Raw)
    else:
        if len(public_key_bytes)!=32:raise ValueError('raw Ed25519 public key must be 32 bytes')
        raw=public_key_bytes;key=Ed25519PublicKey.from_public_bytes(raw)
    key_id=hashlib.sha256(raw).digest();pack_hash=hashlib.sha256(pack).digest()
    if signature[24:56]!=key_id or signature[56:88]!=pack_hash:raise ValueError('signature key/pack identity mismatch')
    try:key.verify(signature[88:152],DOMAIN+key_id+pack_hash)
    except Exception as exc:raise ValueError('Ed25519 signature verification failed') from exc
    return key_id.hex()

@dataclass(frozen=True)
class PackRow:
    publisher:str;name:str;version:str;sha256:str;size:int;pack_class:str;format:str;trust_status:str;key_id:str

class Registry:
    def __init__(self,root:Path):
        self.root=root.resolve();self.objects=self.root/'objects'/'sha256';self.tmp=self.root/'tmp';self.db_path=self.root/'registry.sqlite3'
    def connect(self):
        db=sqlite3.connect(self.db_path,timeout=30,isolation_level=None);db.execute('PRAGMA foreign_keys=ON');db.execute('PRAGMA busy_timeout=30000');return db
    def init(self):
        self.objects.mkdir(parents=True,exist_ok=True);self.tmp.mkdir(parents=True,exist_ok=True)
        with closing(self.connect()) as db:
            db.executescript('''PRAGMA journal_mode=WAL; PRAGMA foreign_keys=ON;
CREATE TABLE IF NOT EXISTS meta(key TEXT PRIMARY KEY,value TEXT NOT NULL);
CREATE TABLE IF NOT EXISTS packs(
 publisher TEXT NOT NULL,name TEXT NOT NULL,version TEXT NOT NULL,sha256 TEXT NOT NULL,size INTEGER NOT NULL,
 pack_class TEXT NOT NULL,format TEXT NOT NULL,trust_status TEXT NOT NULL,key_id TEXT NOT NULL,installed_unix INTEGER NOT NULL,
 PRIMARY KEY(publisher,name,version));
CREATE INDEX IF NOT EXISTS packs_hash ON packs(sha256);''')
            db.execute("INSERT OR IGNORE INTO meta(key,value) VALUES('schema',?)",(str(SCHEMA),))
    def object_path(self,h:str)->Path:return self.objects/h[:2]/h
    def install(self,pack:Path,publisher:str,name:str,version:str,signature:Path|None=None,public_key:Path|None=None,require_signature:bool=False)->PackRow:
        if not publisher or '/' in publisher or not name or '/' in name:raise ValueError('publisher/name must be non-empty and slash-free')
        semver(version);info=inspect_pack(pack)
        if not info['canonical_hash_valid']:raise ValueError('pack canonical hash invalid')
        data=pack.read_bytes();h=sha256_bytes(data);key_id='';trust_status='unverified'
        if bool(signature)!=bool(public_key):raise ValueError('signature and public key must be supplied together')
        if signature and public_key:key_id=verify_ed25519_signature(data,signature.read_bytes(),public_key.read_bytes());trust_status='verified'
        elif require_signature:raise ValueError('signature required by install policy')
        target=self.object_path(h);target.parent.mkdir(parents=True,exist_ok=True)
        fd,tmpname=tempfile.mkstemp(prefix='install-',dir=self.tmp);tmp=Path(tmpname)
        try:
            with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
            if target.exists():
                if sha256_file(target)!=h:raise ValueError('existing content-addressed object is corrupt')
                tmp.unlink()
            else:os.replace(tmp,target)
        finally:tmp.unlink(missing_ok=True)
        row=PackRow(publisher,name,version,h,len(data),info['pack_class'],info['format'],trust_status,key_id)
        with closing(self.connect()) as db:
            db.execute('BEGIN IMMEDIATE')
            old=db.execute('SELECT sha256,key_id FROM packs WHERE publisher=? AND name=? AND version=?',(publisher,name,version)).fetchone()
            if old and (old[0]!=h or (old[1] and key_id and old[1]!=key_id)):
                db.execute('ROLLBACK');raise ValueError('immutable pack identity conflict for publisher/name/version')
            db.execute('''INSERT OR IGNORE INTO packs(publisher,name,version,sha256,size,pack_class,format,trust_status,key_id,installed_unix)
VALUES(?,?,?,?,?,?,?,?,?,?)''',(publisher,name,version,h,len(data),info['pack_class'],info['format'],trust_status,key_id,int(time.time())))
            db.execute('COMMIT')
        return row
    def rows(self)->list[PackRow]:
        with closing(self.connect()) as db:rows=db.execute('SELECT publisher,name,version,sha256,size,pack_class,format,trust_status,key_id FROM packs').fetchall()
        return [PackRow(*r) for r in rows]
    def catalog_hash(self)->str:
        rows=[asdict(r) for r in sorted(self.rows(),key=lambda x:(x.publisher,x.name,semver(x.version),x.sha256))]
        return sha256_bytes(canon({'schema':SCHEMA,'packs':rows}))
    def resolve(self,requirements:dict)->dict:
        if requirements.get('schema')!=1 or not isinstance(requirements.get('requirements'),list):raise ValueError('requirements schema must be 1')
        rows=self.rows();locked=[];roles=set()
        for req in requirements['requirements']:
            role,pub,name,constraint=(req.get(k) for k in ('role','publisher','name','constraint'))
            if not all(isinstance(x,str) and x for x in (role,pub,name,constraint)):raise ValueError('role/publisher/name/constraint required')
            if role in roles:raise ValueError(f'duplicate requirement role: {role}')
            roles.add(role);candidates=[r for r in rows if r.publisher==pub and r.name==name and check_constraint(r.version,constraint)]
            if not candidates:raise ValueError(f'no pack satisfies {pub}/{name} {constraint}')
            candidates.sort(key=lambda r:(semver(r.version),r.sha256),reverse=True);r=candidates[0]
            locked.append({'role':role,**asdict(r)})
        locked.sort(key=lambda x:x['role']);base={'schema':1,'catalog_sha256':self.catalog_hash(),'packs':locked}
        return {**base,'lock_sha256':sha256_bytes(canon(base))}
    def verify_lock(self,lock:dict)->list[str]:
        errors=[]
        if lock.get('schema')!=1:errors.append('bad schema')
        base={k:v for k,v in lock.items() if k!='lock_sha256'}
        if lock.get('lock_sha256')!=sha256_bytes(canon(base)):errors.append('lock hash mismatch')
        roles=set()
        for item in lock.get('packs',[]):
            role=item.get('role');h=item.get('sha256')
            if role in roles:errors.append(f'duplicate role {role}')
            roles.add(role)
            if not isinstance(h,str) or len(h)!=64:errors.append(f'invalid hash for {role}');continue
            p=self.object_path(h)
            if not p.exists():errors.append(f'missing object {h}')
            elif sha256_file(p)!=h:errors.append(f'corrupt object {h}')
        return errors
    def repair(self,pack:Path)->str:
        info=inspect_pack(pack)
        if not info['canonical_hash_valid']:raise ValueError('pack canonical hash invalid')
        data=pack.read_bytes();h=sha256_bytes(data)
        with closing(self.connect()) as db:
            count=db.execute('SELECT COUNT(*) FROM packs WHERE sha256=?',(h,)).fetchone()[0]
        if not count:raise ValueError('repair source is not referenced by registry metadata')
        target=self.object_path(h);target.parent.mkdir(parents=True,exist_ok=True)
        fd,tmpname=tempfile.mkstemp(prefix='repair-',dir=self.tmp);tmp=Path(tmpname)
        try:
            with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
            os.replace(tmp,target)
        finally:tmp.unlink(missing_ok=True)
        if sha256_file(target)!=h:raise ValueError('repaired object failed identity verification')
        return h
    def audit(self)->list[str]:
        errors=[]
        try:
            with closing(self.connect()) as db:
                checks=[r[0] for r in db.execute('PRAGMA integrity_check').fetchall()]
                if checks!=['ok']:errors.extend(f'sqlite integrity: {x}' for x in checks)
                schema=db.execute("SELECT value FROM meta WHERE key='schema'").fetchone()
                if not schema or schema[0]!=str(SCHEMA):errors.append('registry schema metadata mismatch')
        except sqlite3.Error as exc:errors.append(f'sqlite error: {exc}')
        for r in self.rows():
            p=self.object_path(r.sha256)
            if not p.exists():errors.append(f'missing {r.publisher}/{r.name}@{r.version}')
            elif p.stat().st_size!=r.size or sha256_file(p)!=r.sha256:errors.append(f'corrupt {r.publisher}/{r.name}@{r.version}')
        return errors
    def gc(self)->int:
        live={r.sha256 for r in self.rows()};removed=0
        for p in self.objects.glob('*/*') if self.objects.exists() else []:
            if p.is_file() and p.name not in live:p.unlink();removed+=1
        return removed

def write_atomic(path:Path,data:bytes):
    path.parent.mkdir(parents=True,exist_ok=True);fd,name=tempfile.mkstemp(prefix=path.name+'.',dir=path.parent);tmp=Path(name)
    try:
        with os.fdopen(fd,'wb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        os.replace(tmp,path)
    finally:tmp.unlink(missing_ok=True)

def main()->int:
    ap=argparse.ArgumentParser(prog='mosaic-registry');ap.add_argument('--version',action='version',version='mosaic-registry 0.1.3.1');sub=ap.add_subparsers(dest='cmd',required=True)
    q=sub.add_parser('init');q.add_argument('registry',type=Path)
    q=sub.add_parser('install');q.add_argument('registry',type=Path);q.add_argument('pack',type=Path);q.add_argument('--publisher',required=True);q.add_argument('--name',required=True);q.add_argument('--version',required=True);q.add_argument('--signature',type=Path);q.add_argument('--public-key',type=Path);q.add_argument('--require-signature',action='store_true')
    q=sub.add_parser('list');q.add_argument('registry',type=Path)
    q=sub.add_parser('resolve');q.add_argument('registry',type=Path);q.add_argument('requirements',type=Path);q.add_argument('-o','--output',required=True,type=Path)
    q=sub.add_parser('verify-lock');q.add_argument('registry',type=Path);q.add_argument('lock',type=Path)
    q=sub.add_parser('audit');q.add_argument('registry',type=Path)
    q=sub.add_parser('repair');q.add_argument('registry',type=Path);q.add_argument('pack',type=Path)
    q=sub.add_parser('gc');q.add_argument('registry',type=Path)
    a=ap.parse_args();r=Registry(a.registry);r.init()
    if a.cmd=='init':print(json.dumps({'registry':str(r.root),'schema':SCHEMA},sort_keys=True))
    elif a.cmd=='install':print(json.dumps(asdict(r.install(a.pack,a.publisher,a.name,a.version,a.signature,a.public_key,a.require_signature)),sort_keys=True))
    elif a.cmd=='list':print(json.dumps([asdict(x) for x in sorted(r.rows(),key=lambda z:(z.publisher,z.name,semver(z.version)))],indent=2,sort_keys=True))
    elif a.cmd=='resolve':
        lock=r.resolve(json.loads(a.requirements.read_text(encoding="utf-8")));write_atomic(a.output,canon(lock));print(json.dumps({'output':str(a.output),'lock_sha256':lock['lock_sha256']},sort_keys=True))
    elif a.cmd=='verify-lock':
        e=r.verify_lock(json.loads(a.lock.read_text(encoding="utf-8")));print(json.dumps({'ok':not e,'errors':e},sort_keys=True));return 0 if not e else 1
    elif a.cmd=='audit':
        e=r.audit();print(json.dumps({'ok':not e,'errors':e,'catalog_sha256':r.catalog_hash()},sort_keys=True));return 0 if not e else 1
    elif a.cmd=='repair':print(json.dumps({'sha256':r.repair(a.pack)},sort_keys=True))
    elif a.cmd=='gc':print(json.dumps({'removed':r.gc()},sort_keys=True))
    return 0
if __name__=='__main__':
    try:raise SystemExit(main())
    except (ValueError,OSError,sqlite3.Error,json.JSONDecodeError) as exc:print(f'mosaic-registry: error: {exc}',file=sys.stderr);raise SystemExit(2)
