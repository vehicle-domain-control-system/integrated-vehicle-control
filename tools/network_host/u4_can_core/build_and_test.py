"""Build and run the private C11 Host tests with fixed independent fixtures."""
from pathlib import Path
import argparse,hashlib,json,os,platform,shutil,subprocess,sys,time
ROOT=Path(__file__).resolve().parent
REPO=ROOT.parents[2]
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--compiler',default='gcc');ap.add_argument('--output',required=True,type=Path);ap.add_argument('--sanitize',action='store_true');args=ap.parse_args()
    compiler=shutil.which(args.compiler)
    if not compiler:raise RuntimeError('Compiler unavailable: '+args.compiler)
    out=args.output.resolve()
    if out.exists():raise RuntimeError('Use a fresh output directory; prior results must be preserved')
    out.mkdir(parents=True);events=[]
    env={**os.environ,'PATH':str(Path(compiler).parent)+os.pathsep+os.environ.get('PATH','')}
    def run(command,label):
        argv=list(map(str,command));started=time.monotonic()
        try:
            p=subprocess.run(argv,cwd=REPO,env=env,capture_output=True,timeout=90)
            item=dict(command=argv,cwd=str(REPO),exit_code=p.returncode,stdout=p.stdout.decode('utf8',errors='replace'),stderr=p.stderr.decode('utf8',errors='replace'),seconds=time.monotonic()-started)
        except subprocess.TimeoutExpired as error:
            item=dict(command=argv,cwd=str(REPO),exit_code=None,timeout=True,stdout=(error.stdout or b'').decode('utf8',errors='replace'),stderr=(error.stderr or b'').decode('utf8',errors='replace'),seconds=time.monotonic()-started)
        events.append(dict(label=label,**item))
        (out/(label+'.json')).write_text(json.dumps(item,indent=2)+'\n',encoding='utf8')
        (out/(label+'.stdout.log')).write_text(item['stdout'],encoding='utf8',newline='\n');(out/(label+'.stderr.log')).write_text(item['stderr'],encoding='utf8',newline='\n')
        (out/'commands.json').write_text(json.dumps(events,indent=2)+'\n',encoding='utf8')
        if item['exit_code']!=0:
            (out/'results.json').write_text(json.dumps(dict(status='FAIL',compiler=compiler,failed_command=label,execution=item),indent=2)+'\n',encoding='utf8')
            raise RuntimeError(label+' failed; see '+str(out))
        return item
    version=run([compiler,'--version'],'compiler_version')['stdout'].splitlines()[0]
    target=run([compiler,'-dumpmachine'],'compiler_target')['stdout'].strip()
    vectors=json.loads((ROOT/'tests/test_vectors.json').read_text(encoding='utf8'))
    header='#include <stdint.h>\n#define GOLDEN_COUNT 9\n#define BRANCH_IDS '+','.join(json.dumps(x) for x in vectors['branch_ids'])+'\n'
    assert len(vectors['branch_ids'])==len(set(vectors['branch_ids']))==123
    header+='static const uint8_t golden_bytes[9][20]={'+','.join('{'+','.join(str(x) for x in bytes.fromhex(v['expected_hex']))+'}' for v in vectors['golden_vectors'])+'};\n'
    header+='static const CanRawMessage golden_messages[9]={\n'
    for v in vectors['golden_vectors']:
        kind='CAN_REAR' if v['kind']=='CIS_REAR' else 'CAN_PERMISSION';arm='rear' if kind=='CAN_REAR' else 'permission'
        header+='{.kind='+kind+',.fields.'+arm+'={'+','.join('.'+k+'='+str(value)+'u' for k,value in v['fields'].items())+'}},\n'
    header+='};\n';(out/'golden_oracles.h').write_text(header,encoding='utf8',newline='\n')
    flags=['-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-O2','-DCAN_CORE_TRACE']
    if 'mingw' in target:flags+=['-D__USE_MINGW_ANSI_STDIO=1']
    if 'mingw' in target or 'cygwin' in target:flags+=['-Wl,--no-insert-timestamp']
    if args.sanitize:flags+=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    sources=[ROOT/n for n in ['can_codec.c','can_service.c','can_transport.c','can_linkport.c','tests/mock_can_backend.c','tests/test_u4c_can_core.c']]+[ROOT.parent/'u3_raw_diag/i1/i1_host_codec.c']
    def path(p):return os.path.relpath(p,REPO).replace('\\','/')
    runs=[]
    for number in [1,2]:
        d=out/('run'+str(number));d.mkdir();exe=d/('u4c_tests.exe' if os.name=='nt' else 'u4c_tests')
        run([compiler,*flags,'-I'+path(ROOT),'-I'+path(ROOT/'tests'),'-I'+path(out),*[path(p) for p in sources],'-o',path(exe)],'build_'+str(number))
        actual=run([exe],'run_'+str(number))
        records=[json.loads(line) for line in actual['stdout'].splitlines()]
        cases=[x for x in records if x['kind']=='case'];summary=[x for x in records if x['kind']=='summary']
        assert len(cases)==22 and {x['id'] for x in cases}=={f'CAN-D{x:02}' for x in range(1,23)}
        assert all(x['status']=='PASS' and x['failed']==0 and x['assertions']>0 for x in cases) and summary[0]['failed']==0
        runs.append(dict(run_id=f'run{number}',cases=cases,summary=summary[0],binary_sha256=sha(exe),stdout_sha256=hashlib.sha256(actual['stdout'].encode()).hexdigest(),exit_code=actual['exit_code'],binary=path(exe)))
    assert runs[0]['stdout_sha256']==runs[1]['stdout_sha256'],'Non-deterministic observations'
    dependencies=sources+list(ROOT.rglob('*.h'))+[ROOT.parent/'u3_raw_diag/i1/i1_host_codec.h',ROOT/'tests/test_vectors.json',Path(__file__).resolve()]
    result=dict(status='PASS',compiler=version,compiler_path=compiler,target=target,os=platform.platform(),c_standard='C11',flags=flags,sanitizers='EXECUTED_PASS' if args.sanitize else 'NOT_EXECUTED',independent_oracle='Frozen I1 Golden9 + hand-fixed wire bytes and explicit state/return assertions; no production-generated expected bytes',fixture_sha256=sha(ROOT/'tests/test_vectors.json'),test_source_sha256=sha(ROOT/'tests/test_u4c_can_core.c'),sources_sha256={path(p):sha(p) for p in dependencies},runs=runs,can_d_actual=22,can_d_pass=22,golden_vectors=9,binary_reproducible=runs[0]['binary_sha256']==runs[1]['binary_sha256'],G4C_INDEPENDENT='PENDING')
    (out/'results.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
    print(json.dumps(dict(status='PASS',compiler=version,runs=2,cases=22,assertions=runs[0]['summary']['assertions'],output=str(out)),indent=2))
if __name__=='__main__':main()
