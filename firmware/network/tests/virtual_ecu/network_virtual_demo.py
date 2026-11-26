"""Real product ECU processes connected by a byte-only virtual CAN/UART bus.

No DTO, owner decision, codec or routing policy is implemented in this runner.
Only physical wiring, fragmentation and fault injection live here. Separate
executables carry their own complete consistent compile-time node profile.
"""
from __future__ import annotations
import argparse, collections, json, pathlib, queue, subprocess, threading

NODES=('CIS','BCM','WINDOW','DOMAIN','VSS','ESP32','PI')
PROFILES={'CIS':1,'BCM':3,'WINDOW':4,'DOMAIN':5,'VSS':6,'ESP32':0,'PI':0}

class ECU:
    def __init__(self, name, executable, log):
        self.name=name; self.now=1000; self.epochs=[3,3,3]; self.log=log
        command=[str(executable)]+([name] if name in ('ESP32','PI') else [])
        self.p=subprocess.Popen(command,stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,text=True,bufsize=1)
        self.lines=queue.Queue()
        def reader():
            for line in self.p.stdout: self.lines.put(line)
            self.lines.put(None)
        self.thread=threading.Thread(target=reader,daemon=True); self.thread.start()
        ready=self.read()
        assert ready['event']=='ready' and ready['profile']==PROFILES[name],ready
        self.ready=ready
    def read(self):
        try: line=self.lines.get(timeout=15)
        except queue.Empty: raise RuntimeError(f'{self.name}: worker did not answer')
        if line is None: raise RuntimeError(f'{self.name} exited {self.p.poll()}: {self.p.stderr.read()}')
        item=json.loads(line); self.log(item); return item
    def command(self, text):
        self.log({'event':'driver_command','node':self.name,'command':text})
        self.p.stdin.write(text+'\n'); self.p.stdin.flush(); result=[]
        while True:
            item=self.read(); result.append(item)
            if item['event']=='done': return result
    def close(self):
        if self.p.poll() is None:
            self.p.stdin.write('quit\n'); self.p.stdin.flush()
            self.p.wait(timeout=10)
        if self.p.returncode: raise RuntimeError(f'{self.name} exited {self.p.returncode}: {self.p.stderr.read()}')

def executable(root, role):
    suffix='.exe' if __import__('os').name=='nt' else ''
    name='network_virtual_'+role+suffix
    candidates=list(root.rglob(name))
    if not candidates: raise FileNotFoundError(f'{name}: build profile {role} with NETWORK_BUILD_VIRTUAL_ECU=ON')
    preferred=[p for p in candidates if p.parent.name=='VIRTUAL_'+('HOST' if role=='PEER' else role)]
    if len(preferred)==1: return preferred[0]
    if len(candidates)!=1: raise RuntimeError(f'Ambiguous {name}: {candidates}')
    return candidates[0]

class World:
    def __init__(self,root,scenario,emit):
        self.trace=[]; self.pending=collections.deque(); self.scenario=scenario; self.at=1000
        def log(item):
            item=dict(item,scenario=scenario); self.trace.append(item); emit(item)
        self.log=log; self.nodes={}
        try:
            for name in NODES:
                self.nodes[name]=ECU(name,executable(root,'PEER' if name in ('ESP32','PI') else name),log)
        except BaseException:
            self.close()
            raise
    def command(self,name,text,drain=True):
        items=self.nodes[name].command(text)
        self.pending.extend(i for i in items if i['event']=='wire')
        if text.startswith('tick ') and items[-1]['status']!='HOLD': self.nodes[name].now=int(text.split()[1])
        if text.startswith('link ') and items[-1]['status']=='OK':
            _,kind,link,epoch,_=text.split()
            if int(kind)==4:self.nodes[name].epochs[int(link)]=int(epoch)
        if drain:self.drain()
        return items
    def sync(self,name,at):
        if self.nodes[name].now<at:self.command(name,f'tick {at}',False)
    def drain(self):
        while self.pending:
            frame=self.pending.popleft(); sender=frame['node']; link=frame['link']; at=frame['at']
            if link==0:
                for name in NODES[:5]:
                    if name==sender:continue
                    self.sync(name,at)
                    self.command(name,f"canrx {frame['id']} {frame['dlc']} {frame['flags']} {self.nodes[name].epochs[0]} {at} {frame['hex']}",False)
            else:
                peer={'ESP32':'DOMAIN','DOMAIN':'ESP32'} if link==1 else {'PI':'CIS','CIS':'PI'}
                assert sender in peer,frame
                name=peer[sender]; self.sync(name,at)
                raw=bytes.fromhex(frame['hex']); pos=0; sizes=(1,3,7,2,8); part=0
                while pos<len(raw):
                    fragment=raw[pos:pos+sizes[part%len(sizes)]]; pos+=len(fragment);part+=1
                    self.command(name,f'uartrx {link} {self.nodes[name].epochs[link]} {at} {fragment.hex()}',False)
    def run(self,n=12):
        for _ in range(n):
            self.at+=1
            for name in NODES:self.command(name,f'tick {self.at}',False)
            self.drain()
    def events(self,event,node=None,**fields):
        return [i for i in self.trace if i['event']==event and (node is None or i.get('node')==node) and all(i.get(k)==v for k,v in fields.items())]
    def observe(self,node):
        return next(i for i in self.command(node,'observe') if i['event']=='observation')
    def close(self):
        errors=[]
        for n in self.nodes.values():
            try:n.close()
            except Exception as e:errors.append(str(e))
        if errors:raise RuntimeError('; '.join(errors))

def accepted(items):
    assert items[-1]['status']=='OK',items[-1]

def rear(w):
    accepted(w.command('CIS','rear 123 1 1 0')); w.run()
    accepted(w.command('CIS','rear 987 2 1 0')); w.run()
    frames=w.events('wire','CIS',link=0,id=0xa0)
    assert len(frames)==2
    distances=[int.from_bytes(bytes.fromhex(f['hex'])[14:16],'little') for f in frames]
    assert distances==[123,987],distances
    assert len(w.events('c2_rear'))==2,w.events('c2_rear')
    warnings=w.events('owner_can','VSS',id=0xc0)
    assert len(warnings)>=2,warnings
    assert warnings[0]['rear_state']!=warnings[1]['rear_state'],warnings
    assert w.observe('CIS')['tx_succeeded']==2
    return {'input_mm':distances,'c2_callbacks':2,'warning_changed_from_actual_input':True}

def mobile(w):
    accepted(w.command('ESP32','context 11'));w.run()
    accepted(w.command('ESP32','request 501 1'));w.run(24)
    commands=w.events('owner_can','BCM',id=0x100)
    results=w.events('owner_uart','ESP32',type=0x20)
    assert len(commands)==1,commands
    assert len(results)>=1,results
    result=results[-1]
    assert result['request']==501 and result['command']==commands[0]['command'],(commands,results)
    assert result['result']==5 and result['confirmation']==0,result
    assert w.events('wire','BCM',id=0x140) and w.events('wire','DOMAIN',link=1,type=0x20)
    return {'request':501,'command':commands[0]['command'],'actual_owner_result':5,'fabricated_done':False}

def permission(w):
    accepted(w.command('DOMAIN','permission 1 1'));w.run(20)
    for node in ('CIS','BCM','VSS'):
        assert any(i['power_usable'] for i in w.events('permission_view',node)),(node,w.events('permission_view',node))
    pi=w.events('owner_uart','PI',type=0x62)
    assert pi and pi[-1]['permission']==1,pi
    accepted(w.command('BCM','owner_no_data'))
    assert w.events('permission_view','BCM')[-1]['reason']==9
    w.run(160)
    for node in ('CIS','VSS'):
        view=w.events('permission_view',node)[-1]
        assert not view['power_usable'] and view['raw_power']==1,view
    before=len(w.events('wire','CIS',link=2,type=0x62))
    accepted(w.command('DOMAIN','permission 0 2'));w.run(20)
    pi=w.events('owner_uart','PI',type=0x62)
    assert pi[-1]['permission']==0 and len(pi)>before,pi
    return {'expired_allow_withdrawn':True,'raw_allow_preserved_as_evidence':True,'owner_no_data_withdrawn':True,'pi_reverse_link':True,'permission_policy':'HOST_EXPLICIT_ONLY'}

def vision(w):
    accepted(w.command('PI','vision 42 2'));w.run()
    assert w.events('owner_uart','CIS',type=0x60)[-1]['count']==2
    relays=w.events('wire','CIS',id=0x310);assert len(relays)==1,relays
    assert w.events('c2_occupant'),w.trace[-20:]
    accepted(w.command('PI','visionstatus 91'));w.run()
    assert w.events('owner_uart','CIS',type=0x61)
    assert len(w.events('wire','CIS',id=0x310))==1,'status refreshed observation'
    accepted(w.command('PI','vision 43 1'));w.run()
    assert len(w.events('wire','CIS',id=0x310))==2
    return {'vision_count_inputs':[2,1],'status_did_not_refresh_result':True,'product_occupant_relays':2}

def query(w):
    accepted(w.command('CIS','rear 321 1 1 0'));w.run()
    accepted(w.command('ESP32','context 11'));w.run()
    accepted(w.command('ESP32','request 551 1'));w.run(24)
    counts=[]
    for idx,mode in enumerate(('normal','empty','error'),701):
        accepted(w.command('DOMAIN','query_mode '+mode))
        accepted(w.command('ESP32',f'query {idx} 3'));w.run(40)
        ends=w.events('owner_uart','ESP32',type=0x40)
        selected=[e for e in ends if e['query']==idx]
        assert len(selected)==1,(mode,selected)
        counts.append(selected[0]['count'])
        if mode=='normal':assert selected[0]['count']==2,selected
        else:assert selected[0]['count']==0,selected
    assert len(w.events('owner_uart','ESP32',type=0x40))==3
    return {'query_ids':[701,702,703],'actually_transmitted_item_counts':counts,'one_end_per_query':True}

def faults(w):
    accepted(w.command('CIS','rear 400 1 1 0'));w.run(2)
    admission=w.events('tx_fact','CIS',phase=0)[-1]
    duplicate=w.command('CIS',f"txfact 0 3 {admission['epoch']} {admission['token']} 1 1 0 0")
    assert duplicate[-1]['status']=='DUPLICATE',duplicate[-1]
    w.run(8)
    frame=w.events('wire','CIS',id=0xa0)[-1]
    count=len(w.events('c2_rear'))
    bad=w.command('DOMAIN',f"canrx {frame['id']} 10 {frame['flags']} 3 {w.at} {frame['hex']}")
    assert bad[-1]['status']=='REJECTED' and len(w.events('c2_rear'))==count
    good=w.command('DOMAIN',f"canrx {frame['id']} {frame['dlc']} {frame['flags']} 3 {w.at} {frame['hex']}")
    assert good[-1]['status']=='DUPLICATE' and len(w.events('c2_rear'))==count
    expired=w.command('CIS','rear 500 2 1 10000')
    assert expired[-1]['status']=='EXPIRED',expired[-1]
    w.run()
    assert len(w.events('c2_rear'))==count,'old source AGE accepted'
    accepted(w.command('PI','vision 42 2'));w.run()
    vision_frame=w.events('wire','PI',type=0x60)[-1]; bad_bytes=bytearray.fromhex(vision_frame['hex']);bad_bytes[-1]^=1
    before=len(w.events('owner_uart','CIS',type=0x60))
    for pos in range(0,len(bad_bytes),8):w.command('CIS',f'uartrx 2 3 {w.at} {bad_bytes[pos:pos+8].hex()}')
    assert len(w.events('owner_uart','CIS',type=0x60))==before
    accepted(w.command('PI','vision 43 1'));w.run()
    assert len(w.events('owner_uart','CIS',type=0x60))==before+1
    partial=bytes.fromhex(w.events('wire','PI',type=0x60)[-1]['hex'])
    w.command('CIS',f'uartrx 2 3 {w.at} {partial[:5].hex()}')
    delivered=len(w.events('owner_uart','CIS',type=0x60))
    w.run(101)  # selected Host assembly limit is 100 ms, equality expires
    for pos in range(5,len(partial),8):w.command('CIS',f'uartrx 2 3 {w.at} {partial[pos:pos+8].hex()}')
    assert len(w.events('owner_uart','CIS',type=0x60))==delivered,'timed-out prefix was reused'
    accepted(w.command('PI','vision 44 2'));w.run()
    assert len(w.events('owner_uart','CIS',type=0x60))==delivered+1,'parser did not recover after timeout'
    accepted(w.command('CIS','link 3 2 3 0'))
    lost=w.command('CIS',f'uartrx 2 3 {w.at} a55a')
    assert lost[-1]['status']=='HOLD'
    accepted(w.command('CIS','link 4 2 4 0'))
    overflow=w.command('CIS',f'uartrx 2 4 {w.at} '+('00'*17))
    assert overflow[-1]['status']=='NO_CAPACITY' and w.observe('CIS')['hal_rx_dropped']==1
    accepted(w.command('CIS','link 4 2 5 0'))
    stale=w.command('CIS',f'uartrx 2 4 {w.at} a5')
    assert stale[-1]['status']=='STALE'
    saturated=w.command('CIS','saturate_tx')
    assert saturated[-1]['status']=='NO_CAPACITY'
    check=w.events('tx_saturation','CIS')[-1]
    assert check['queued']==16 and check['output_unchanged']
    accepted(w.command('CIS','timing 1 5 8 10'))
    accepted(w.command('CIS','rear 222 3 1 0'));w.run(2)
    prior=w.observe('CIS')['tx_succeeded'];accepted(w.command('CIS','link 4 0 4 0'));w.run(12)
    assert w.observe('CIS')['tx_succeeded']==prior,'reset invented TX success'
    accepted(w.command('DOMAIN','timing 1 501 502 503'))
    permission_frames=len(w.events('wire','DOMAIN',id=0x080))
    accepted(w.command('DOMAIN','permission 1 900'));w.run(2)
    w.at+=500;w.run(4)
    assert len(w.events('wire','DOMAIN',id=0x080))==permission_frames,'expired ALLOW reached physical wire'
    assert w.events('tx_fact','DOMAIN',phase=1,status='EXPIRED'),'deadline was not enforced at actual wire stage'
    accepted(w.command('WINDOW','window_state'));w.run()
    inspection=w.events('held_can','DOMAIN',id=0xb0)
    assert inspection and inspection[-1]['observation_only'] and not w.events('owner_can','DOMAIN',id=0xb0)
    result=w.command('VSS',f'clock {w.at-1}')
    assert result[-1]['status']=='HOLD' and w.observe('VSS')['hal_clock_holds']>0
    return {'bad_dlc':True,'bad_crc':True,'fragment_timeout_recovery':True,'duplicate_fact':True,'old_age':True,'rx_overflow':True,'tx_queue_full':True,'link_loss':True,'late_completion_after_reset':True,'expired_allow_driver_deadline':True,'old_epoch_rejected':True,'clock_regression':True,'window_inspection_only':True}

SCENARIOS={'rear':rear,'mobile':mobile,'permission':permission,'vision':vision,'query':query,'faults':faults}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bin-dir',required=True,type=pathlib.Path)
    p.add_argument('--scenario',choices=['all',*SCENARIOS],default='all')
    p.add_argument('--trace',type=pathlib.Path)
    args=p.parse_args(); output=args.trace.open('w',encoding='utf8') if args.trace else None
    def emit(item):
        line=json.dumps(item,separators=(',',':')); print(line,flush=True)
        if output:output.write(line+'\n');output.flush()
    try:
        for name in SCENARIOS if args.scenario=='all' else [args.scenario]:
            world=World(args.bin_dir,name,emit)
            try:
                facts=SCENARIOS[name](world)
                emit(dict(event='scenario_result',scenario=name,status='PASS',**facts))
            finally:world.close()
    finally:
        if output:output.close()
    return 0
if __name__=='__main__':raise SystemExit(main())
