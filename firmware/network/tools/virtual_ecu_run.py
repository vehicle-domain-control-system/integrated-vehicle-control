"""Build all isolated Host ECU profiles, then run the real Network demo.

Example (toolchain arguments are optional when already on PATH):
  python tools/virtual_ecu_run.py --output /tmp/network-virtual \
    --domain-c2-include /path/to/pinned/domain/interface
No ARM, SDK, Flash, hardware operation or Git mutation is performed.
"""
from __future__ import annotations
import argparse, hashlib, json, pathlib, subprocess, sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',required=True,type=pathlib.Path)
    parser.add_argument('--domain-c2-include',required=True,type=pathlib.Path)
    parser.add_argument('--cmake',default='cmake')
    parser.add_argument('--cc')
    parser.add_argument('--make')
    parser.add_argument('--generator')
    parser.add_argument('--jobs',type=int,default=4)
    parser.add_argument('--scenario',choices=['all','rear','mobile','permission','vision','query','faults'],default='all')
    args=parser.parse_args()
    source=pathlib.Path(__file__).resolve().parents[1]
    output=args.output.resolve()
    if output.exists() and any(output.iterdir()):parser.error('--output must be a fresh empty directory')
    if not (args.domain_c2_include/'Domain_Interface_v0.5_C2_target.h').is_file():parser.error('actual pinned C2 header is missing')
    output.mkdir(parents=True,exist_ok=True)
    # Exclude output from the source authority boundary if caller places it
    # below the source tree; generated build files are never source inputs.
    def source_hashes():
        return {p.relative_to(source).as_posix():hashlib.sha256(p.read_bytes()).hexdigest()
                for p in sorted(source.rglob('*')) if p.is_file() and output not in p.parents}
    before=source_hashes(); records=[]
    (output/'SOURCE_SHA_BEFORE.json').write_text(json.dumps(before,indent=2),encoding='utf8')
    for profile in ('HOST','CIS','BCM','WINDOW','DOMAIN','VSS'):
        build=output/('VIRTUAL_'+profile)
        config=[args.cmake,'-S',str(source),'-B',str(build),'-DNETWORK_TARGET=host',
                '-DCMAKE_BUILD_TYPE=Debug','-DBUILD_TESTING=OFF',
                '-DNETWORK_NODE_PROFILE='+profile,'-DNETWORK_BUILD_WIRE_CANDIDATE=ON',
                '-DNETWORK_BUILD_UPPER_STACK=ON','-DNETWORK_BUILD_HAL_PORT_ADAPTER=ON',
                '-DNETWORK_BUILD_VIRTUAL_ECU=ON','-DNETWORK_BUILD_DOMAIN_C2='+('ON' if profile=='DOMAIN' else 'OFF'),
                '-DNETWORK_DOMAIN_C2_INCLUDE_DIR='+str(args.domain_c2_include.resolve())]
        if args.generator:config.extend(['-G',args.generator])
        if args.cc:config.append('-DCMAKE_C_COMPILER='+args.cc)
        if args.make:config.append('-DCMAKE_MAKE_PROGRAM='+args.make)
        for label,command in [('configure',config),('build',[args.cmake,'--build',str(build),'-j',str(args.jobs)])]:
            result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,encoding='utf8',errors='replace')
            (output/(profile+'_'+label+'.log')).write_text(result.stdout,encoding='utf8')
            records.append(dict(profile=profile,stage=label,command=command,exit_code=result.returncode))
            (output/'COMMANDS.json').write_text(json.dumps(records,indent=2),encoding='utf8')
            if result.returncode:
                print(result.stdout,file=sys.stderr);return result.returncode
        print(profile+' built with its own product-library profile',flush=True)
    after=source_hashes()
    (output/'SOURCE_SHA_AFTER.json').write_text(json.dumps(after,indent=2),encoding='utf8')
    if before!=after:raise RuntimeError('source changed during build; do not combine profile artifacts')
    command=[sys.executable,'-B',str(source/'tests/virtual_ecu/network_virtual_demo.py'),
             '--bin-dir',str(output),'--scenario',args.scenario,'--trace',str(output/'TRACE.jsonl')]
    with (output/'DEMO.log').open('w',encoding='utf8') as log:
        result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    records.append(dict(stage='demo',command=command,exit_code=result.returncode))
    (output/'COMMANDS.json').write_text(json.dumps(records,indent=2),encoding='utf8')
    if result.returncode:
        print((output/'DEMO.log').read_text(encoding='utf8')[-6000:],file=sys.stderr)
    else:
        for line in (output/'TRACE.jsonl').read_text(encoding='utf8').splitlines():
            item=json.loads(line)
            if item['event']=='scenario_result':print(json.dumps(item,ensure_ascii=False))
    return result.returncode
if __name__=='__main__':raise SystemExit(main())
