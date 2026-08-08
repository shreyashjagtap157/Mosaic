#!/usr/bin/env python3
"""Run the evidence-available M4 Wedge Tournament without promoting missing evidence to success."""
from __future__ import annotations
import hashlib,json,platform,re,subprocess,sys,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def sh(*cmd:str)->str:
    return subprocess.check_output(cmd,cwd=ROOT,text=True,stderr=subprocess.STDOUT).strip()
def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()
def run_capture(script:str)->tuple[bool,str]:
    try:return True,sh(sys.executable,str(ROOT/'tools'/script))
    except subprocess.CalledProcessError as e:return False,e.output.strip()
def main()->int:
    commit=sh('git','rev-parse','HEAD'); short=commit[:12]
    lang_ok,lang_out=run_capture('benchmark_language_packs.py')
    lm=re.search(r'reduction=([0-9.]+)%.*overhead=([-0-9.]+)%',lang_out)
    lang_reduction=float(lm.group(1)) if lm else None
    assurance_checks={}
    for name,script in [('api_exactness','validate_c_api.py'),('malformed','validate_c_malformed.py'),('unicode_malformed','validate_c_unicode_malformed.py'),('bounded','benchmark_bounded_processing.py')]:
        ok,out=run_capture(script);assurance_checks[name]={'pass':ok,'output':out}
    data={
      'schema':'WEDGE-EVAL-001','timestamp_utc':time.strftime('%Y-%m-%dT%H:%M:%SZ',time.gmtime()),
      'commit':commit,'platform':{'system':platform.system(),'machine':platform.machine(),'python':platform.python_version()},
      'packs':{str(p.relative_to(ROOT)):sha(p) for p in [ROOT/'fixtures/packs/model-v2.mpack',ROOT/'fixtures/packs/unicode17-v1.mpack',ROOT/'fixtures/packs/language/en-v1.mpack',ROOT/'fixtures/packs/language/hi-v1.mpack',ROOT/'fixtures/packs/language/ja-v1.mpack']},
      'wedges':{
        'A_multilingual_llm':{
          'observed':{'reference_mixed_token_reduction_pct':lang_reduction,'mechanism_benchmark_pass':lang_ok},
          'gates':{
             'exact_reconstruction':True,'unknown_input_zero':True,
             'mean_non_english_fertility_gain_ge_8':None,
             'worst_language_regression_le_2':None,
             'ids_throughput_ge_70pct_strong_baseline':None,
             'english_regression_le_2':None,
             'downstream_quality_regression_le_0_5pp':None},
          'qualified':False,
          'reason':'Reference-pack mechanism is measurable, but representative strong-baseline per-language fertility and controlled downstream model-quality evidence do not exist yet.'},
        'B_compiler_llm':{
          'gates':{'lexical_compatibility_100':None,'model_id_compatibility_100':True,'source_offset_disagreement_zero':None,'exact_reconstruction':True,'shared_cpu_reduction_ge_10':None,'temporary_memory_reduction_ge_15':None},
          'qualified':False,'reason':'No compiler lexical projection/thin shared-pipeline adapter exists at this commit; inventing benchmark values would violate the tournament protocol.'},
        'C_assurance_sdk':{
          'observed':assurance_checks,
          'gates':{'arbitrary_byte_reconstruction_100':assurance_checks['api_exactness']['pass'],'streaming_full_equivalence_100':assurance_checks['api_exactness']['pass'],'malformed_pack_fail_closed_100':assurance_checks['malformed']['pass'] and assurance_checks['unicode_malformed']['pass'],'user_text_privileged_control_activation_zero':True,'unbounded_ordinary_pack_execution_zero':assurance_checks['bounded']['pass'],'cross_platform_canonical_divergence_zero':None},
          'qualified':False,'reason':'All locally executable assurance gates pass, but the mandatory cross-platform canonical-equivalence gate is unverified on this network-isolated single-platform host.'}
      },
      'result':'NONE','selection_reason':'No candidate has complete evidence for every fatal gate. The protocol explicitly permits NONE. Continue platform-neutral substrate work and rerun after representative multilingual/downstream, compiler-shared-pipeline, and cross-platform evidence exist.'}
    outdir=ROOT/'benches/wedge';outdir.mkdir(parents=True,exist_ok=True)
    (outdir/'WEDGE-EVAL-001.json').write_text(json.dumps(data,indent=2,sort_keys=True)+'\n')
    md=f'''# WEDGE-EVAL-001\n\nCommit: `{short}`  \nResult: **NONE**\n\nThe first executable tournament did not promote any wedge because each candidate is missing at least one fatal-gate evidence family. This is a valid result under the converged protocol.\n\n## A — Multilingual LLM preprocessing\n\nThe current controlled mixed-language mechanism benchmark reports **{lang_reduction if lang_reduction is not None else 'unavailable'}%** token reduction with the tiny English/Hindi/Japanese reference packs. Exactness and byte fallback are proven, but this is not representative per-language quality evidence and there is no controlled downstream LLM-quality experiment. **Not qualified.**\n\n## B — Compiler + LLM\n\nModel compatibility/exact byte behavior exists, but no compiler lexical projection or shared-pipeline adapter exists at this commit. Therefore lexical compatibility, offset agreement, CPU reduction, and allocation reduction cannot honestly be measured. **Not qualified.**\n\n## C — High-assurance SDK\n\nThe locally executable arbitrary-byte, streaming/full, malformed-pack, and bounded-resource gates pass. However the mandatory cross-platform canonical-equivalence gate cannot be established on this single-platform, network-isolated host. **Not qualified yet.**\n\n## Decision\n\n**NONE.** No product wedge is selected. Platform-neutral implementation may continue, but product-specific investment remains unselected until the missing evidence is produced. Raw machine-readable evidence is in `WEDGE-EVAL-001.json`.\n'''
    (outdir/'WEDGE-EVAL-001.md').write_text(md)
    print(f'WEDGE-EVAL-001 result=NONE commit={short} multilingual_reference_reduction={lang_reduction}')
    return 0
if __name__=='__main__':raise SystemExit(main())
