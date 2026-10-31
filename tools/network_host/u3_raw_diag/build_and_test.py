#!/usr/bin/env python3
"""Build/run real C11 binaries and verify frozen bytes without calling a codec oracle."""
from pathlib import Path
import argparse, hashlib, json, platform, re, shutil, struct, subprocess, sys

ROOT=Path(__file__).resolve().parent
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def c_bytes(name, data):
    return 'static const uint8_t '+name+'[] = {'+','.join(str(b) for b in data)+'};\n'
def independent_can(kind, f):
    keys=(['boot_id','tx_sequence','update_sequence','source_age_ms','rear_distance_deci_cm','validity','quality_reason','proximity_status'] if kind=='CIS_REAR' else ['boot_id','tx_sequence','vehicle_power_permission','power_permission_quality','power_operation_state','operation_allowed','operation_quality','update_sequence','source_age_ms'])
    return struct.pack('<IIIHHBBB' if kind=='CIS_REAR' else '<IIBBBBBIH',*(f[k] for k in keys))+b'\0'
def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--compiler',default='gcc'); ap.add_argument('--output',required=True,type=Path)
    a=ap.parse_args(); out=a.output.resolve()
    if out.exists(): raise RuntimeError('Use a new output directory; existing evidence is preserved')
    cc=shutil.which(a.compiler)
    if not cc: raise RuntimeError('Compiler unavailable: '+a.compiler)
    out.mkdir(parents=True); events=[]; checks=[]
    def run(argv, label):
        argv=list(map(str,argv))
        p=subprocess.run(argv,cwd=ROOT,capture_output=True,text=True,encoding='utf-8',errors='replace')
        item={'label':label,'command':argv,'cwd':str(ROOT),'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
        events.append(item); (out/(label+'.log')).write_text(json.dumps(item,indent=2)+'\n',encoding='utf-8',newline='\n')
        (out/'commands.json').write_text(json.dumps(events,indent=2)+'\n',encoding='utf-8',newline='\n')
        if p.returncode: raise RuntimeError(label+' failed; see '+str(out/(label+'.log')))
        return p.stdout
    version=run([cc,'--version'],'compiler_version').splitlines()[0]
    target=run([cc,'-dumpmachine'],'compiler_target').strip()
    provenance=json.loads((ROOT/'i1_provenance.json').read_text(encoding='utf-8'))
    for item in provenance['files']: assert sha(ROOT/item['path'])==item['sha256'], 'I1 source drift'
    vectors=json.loads((ROOT/'independent_vectors.json').read_text(encoding='utf-8'))
    header='#include <stdint.h>\n'
    for v,name in zip(vectors['vectors'],['ref_rear','ref_permission']):
        data=bytes.fromhex(v['hex_bytes']); f=v['expected_fields']
        if v['message']=='CIS_REAR':
            canonical=dict(f,boot_id=int(f['boot_id'],16),tx_sequence=int(f['tx_sequence'],16),rear_distance_deci_cm=f['rear_distance_raw_0_1cm'])
        else:
            canonical={k.removesuffix('_raw'):value for k,value in f.items()}
            canonical.update(boot_id=int(f['boot_id'],16),tx_sequence=int(f['tx_sequence'],16))
        assert independent_can(v['message'],canonical)==data
        assert hashlib.sha256(data).hexdigest()==vectors['expected_sha256_of_bytes'][v['id']]
        header+=c_bytes(name,data)
        checks.append({'id':v['id'],'independent_python_struct_oracle':'PASS','frozen_bytes_sha256':hashlib.sha256(data).hexdigest()})
    (out/'independent_for_c.h').write_text(header,encoding='utf-8',newline='\n')
    flags=['-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-O2']
    if 'mingw' in target: flags+=['-D__USE_MINGW_ANSI_STDIO=1']
    sources=[ROOT/'u3_raw_diag.c',ROOT/'i1/i1_host_codec.c',ROOT/'test_u3_raw_diag.c']
    test_runs=[]
    for repeat in [1,2]:
        build=out/('run'+str(repeat)); build.mkdir()
        exe=build/('u3_tests.exe' if sys.platform=='win32' else 'u3_tests')
        run([cc,*flags,'-I',ROOT,'-I',out,*sources,'-o',exe],'build_'+str(repeat))
        output=run([exe],'run_'+str(repeat))
        cases=[{'id':m[0],'result':m[1],'checks':int(m[2]),'failures':int(m[3])} for m in re.findall(r'CASE (T\d+) (PASS|FAIL) checks=(\d+) failures=(\d+)',output)]
        assert len(cases)==17 and all(c['result']=='PASS' for c in cases)
        test_runs.append({'cases':cases,'summary':re.search(r'SUMMARY.*',output).group(),'stdout_sha256':hashlib.sha256(output.encode()).hexdigest()})
    assert test_runs[0]==test_runs[1], 'Non-deterministic test output'
    # Relevant I1 frozen CAN fixtures: native direct encoder and decoder, with
    # field expectations independently supplied by the original JSON.
    legacy=json.loads((ROOT/'i1_can_vectors.json').read_text(encoding='utf-8'))
    c='#include "i1_host_codec.h"\n#include <stdint.h>\n#include <stdio.h>\n#include <string.h>\nint main(void) { unsigned failed=0; size_t n; uint8_t out[20];\n'
    for idx,v in enumerate(legacy['vectors']):
        data=bytes.fromhex(v['expected_hex']); assert data==independent_can(v['kind'],v['fields'])
        typ,fn,canid=('i1_cis_rear','cis_rear',0xA0) if v['kind']=='CIS_REAR' else ('i1_domain_permission','domain_permission',0x80)
        c+='{ '+c_bytes('golden'+str(idx),data)+typ+' f={0}, decoded={0}; unsigned before=failed;\n'
        for k,value in v['fields'].items(): c+='f.'+k+' = '+str(value)+'u;\n'
        c+='if(i1_'+fn+'_encode(&f,out,20,&n)!=I1_OK || n!=20 || memcmp(out,golden'+str(idx)+',20)) ++failed;\n'
        c+='if(i1_'+fn+'_decode('+str(canid)+',11,golden'+str(idx)+',20,&decoded)!=I1_OK) ++failed;\n'
        for k in v['fields']: c+='if(decoded.'+k+'!=f.'+k+') ++failed;\n'
        c+='printf("I1_CAN '+v['id']+' %s\\n",failed==before?"PASS":"FAIL"); }\n'
    c+='printf("I1_CAN_SUMMARY vectors='+str(len(legacy['vectors']))+' failures=%u\\n",failed);return failed?1:0;}\n'
    (out/'i1_can_regression.c').write_text(c,encoding='utf-8',newline='\n')
    exe=out/('i1_can_regression.exe' if sys.platform=='win32' else 'i1_can_regression')
    run([cc,*flags,'-I',ROOT/'i1',ROOT/'i1/i1_host_codec.c',out/'i1_can_regression.c','-o',exe],'i1_can_build')
    legacy_output=run([exe],'i1_can_run')
    deps=run([cc,'-std=c11','-M','-I',ROOT,'-I',out,*sources],'compiled_dependencies')
    assert not re.search(r'(FreeRTOS|ESP[-_]IDF|S32K|RTD|[\\/]HAL[\\/]|[\\/]SDK[\\/])',deps,re.I), 'Target dependency leaked'
    result={'result':'PASS','HOST_EXECUTION':'HOST_TESTED_U3_SCOPE','G3_INDEPENDENT':'PENDING','os':platform.platform(),'python':sys.version.split()[0],'compiler':version,'compiler_target':target,'compiler_path':cc,'c_standard':'C11','flags':flags,'compiled_sources':[str(p.relative_to(ROOT)) for p in sources],'runs':test_runs,'independent_vectors':checks,'i1_can_vectors':len(legacy['vectors']),'i1_can_result':legacy_output,'T18':'PASS_TWO_CLEAN_BUILDS_IDENTICAL_STDOUT','T19':'PASS_REAL_I1_LINK_AND_FROZEN_CAN_REGRESSION','T20':'PASS_COMPILED_DEPENDENCIES_NO_MCU_SDK','sanitizers':'NOT_EXECUTED','clang':'NOT_EXECUTED' if 'clang' not in Path(cc).name.lower() else 'EXECUTED','msvc':'NOT_EXECUTED','MOCK_E2E':'NOT_EXECUTED','TARGET_BINDING':'NOT_EXECUTED','BOARD_PHYSICAL':'NOT_EXECUTED','FUNCTION_POLICY':'UNRESOLVED_CONTRACT','files_sha256':{p.relative_to(ROOT).as_posix():sha(p) for p in ROOT.rglob('*') if p.is_file()}}
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8',newline='\n')
    (out/'build_and_test_logs.txt').write_text('\n'.join(e['label']+'\n'+json.dumps(e['command'])+'\nexit_code='+str(e['exit_code'])+'\n'+e['stdout']+e['stderr'] for e in events),encoding='utf-8',newline='\n')
    print(json.dumps({'result':'PASS','compiler':version,'native_runs':2,'cases_per_run':17,'summary':test_runs[0]['summary'],'i1_can_vectors':len(legacy['vectors']),'output':str(out)},indent=2))
if __name__=='__main__': main()
