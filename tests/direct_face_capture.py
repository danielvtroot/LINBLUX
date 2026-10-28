"""Real Studio acceptance, supervised by TURBO. Only isolated copies are edited."""
import argparse,copy,hashlib,json,os,subprocess,time
from pathlib import Path
from editor_restart import xdo,wait_for,close_window

def prepare(root):
    repo=Path(__file__).resolve().parents[1]
    world=json.loads((repo/'assets/defaults/factory/world.loblox.json').read_text())
    objects=world['objects'];next_id=max(x['id'] for x in objects)+1
    def part(name,pos,size,rotation=[0,0,0]):
        nonlocal next_id
        value=dict(id=next_id,parent=1,name=name,**{'class':'Part'},shape='Block',position=pos,size=size,rotation=rotation,color=[163,162,165,255],material='SmoothPlastic',anchored=True,canCollide=True)
        next_id+=1;objects.append(value);return value
    wall=part('Parede 191',[0,5,0],[191,10,1]);objects.remove(wall);objects.insert(2,wall)
    part('Parede adjacente',[20,5,8],[1,10,15]);small=part('Peca pequena com decal',[10,2,6],[3,4,1],[0,30,0])
    objects.append(dict(id=next_id,parent=small['id'],name='Decal',**{'class':'Decal'},surface=dict(image=str(repo/'tests/fixtures/surface-test.png'),face=1)));next_id+=1
    for item in objects:
        if item['class']=='SpawnLocation':item['position']=[0,.5,12]
        if item['class']=='Lighting':item.update(skyEnabled=False,fogDensity=0)
    world['nextId']=next_id;root.mkdir(parents=True,exist_ok=True)
    (root/'construction.json').write_text(json.dumps(world,indent=2)+'\n')

def capture(args):
    from PIL import Image
    root=args.output.resolve();root.mkdir(parents=True,exist_ok=True)
    if not (root/'construction.json').exists():prepare(root)
    original=(root/'construction.json').read_bytes();out=root/args.stage;out.mkdir(exist_ok=True)
    work=out/'world.json';work.write_bytes(original)
    env=dict(os.environ,HOME=str(out),XDG_DATA_HOME=str(out/'data'),XDG_CONFIG_HOME=str(out/'config'),XDG_STATE_HOME=str(out/'state'),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2')
    records=[]
    def wall():return next(v for v in json.loads(work.read_text())['objects'] if v['name']=='Parede 191')
    def same(a,b):return max(abs(x-y) for k in ['size','position'] for x,y in zip(a[k],b[k]))<.005
    with (out/'studio.log').open('w') as log:
        proc=subprocess.Popen([str(args.binary.resolve()),'--project',str(work)],cwd=out,env=env,stdout=log,stderr=log)
        try:
            def window():
                r=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(proc.pid),'--name','^LINBLUX Studio$'],text=True,capture_output=True,timeout=3)
                return r.stdout.splitlines()[-1] if r.returncode==0 else None
            wid=wait_for(window,proc,25)
            def click(x,y):
                xdo('mousemove','--sync',x,y);time.sleep(delay);xdo('mousedown',1);time.sleep(delay);xdo('mouseup',1);time.sleep(delay)
            def key(name):xdo('keydown',name);time.sleep(.3);xdo('keyup',name);time.sleep(.7)
            def save():
                before=work.stat().st_mtime_ns;key('ctrl+s');wait_for(lambda:work.stat().st_mtime_ns!=before,proc,8);return wall()
            def shot(name):
                time.sleep(.6);path=out/f'{name}-{width}.png';subprocess.run(['scrot','-o','-u',str(path)],check=True,timeout=5)
                assert Image.open(path).size==(width,height)
            for width,height in ((1366,768),(1920,1080)):
                delay=.4 if width==1366 else .9
                xdo('windowsize',wid,width,height);xdo('windowmove',wid,0,0);xdo('windowfocus','--sync',wid);time.sleep(4)
                click(width-210,243);click(50 if width==1366 else 60,92);xdo('mousemove',5,5);shot('select')
                click(133 if width==1366 else 145,92);xdo('mousemove',5,5);shot('move')
                click(214 if width==1366 else 233,92);xdo('mousemove',5,5);shot('scale-idle')
                hx=int(210+(width-510)*.46);hy=int(171+(height-195)*.40)
                xdo('mousemove',hx,hy);time.sleep(1);shot('scale-hover')
                if args.stage=='after':
                    start=wall();xdo('mousedown',1);time.sleep(delay);xdo('mousemove',hx-70,hy+35);time.sleep(1);shot('scale-drag');xdo('mouseup',1);time.sleep(.8)
                    changed=save();assert changed['size'][2]>start['size'][2]+.25,(start,changed)
                    assert abs((changed['position'][2]-changed['size'][2]/2)-(start['position'][2]-start['size'][2]/2))<.005
                    assert abs(changed['size'][2]*2-round(changed['size'][2]*2))<.005
                    shot('scale-result');key('ctrl+z');undone=save();assert same(undone,start),'undo failed';shot('undo')
                    key('ctrl+y');redone=save();assert same(redone,changed),'redo failed';shot('redo')
                    # Whole-face drag, cancellation, then release; no tiny target.
                    xdo('mousemove',hx,hy);time.sleep(.8);xdo('mousedown',1);time.sleep(delay);xdo('mousemove',hx-30,hy+20);time.sleep(.5);key('Escape');xdo('mouseup',1);time.sleep(.5)
                    cancelled=save();assert same(cancelled,changed),'escape failed';shot('cancel')
                    records.append(dict(resolution=[width,height],before=start,after=changed,undo=True,redo=True,escape=True,snap=True,opposite_face_fixed=True))
                    key('ctrl+z');restored=save();assert same(restored,start),'restore for next resolution failed'
                click(295 if width==1366 else 320,92);xdo('mousemove',5,5);shot('rotate')
                click(50 if width==1366 else 60,92);xdo('mousemove',5,5);shot('final')
            close_window(wid);assert proc.wait(timeout=10)==0
        finally:
            if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
    assert (root/'construction.json').read_bytes()==original
    (out/'manifest.json').write_text(json.dumps(dict(source_sha256=hashlib.sha256(original).hexdigest(),binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),functional=records,images=sorted(p.name for p in out.glob('*.png'))),indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--binary',required=True,type=Path);p.add_argument('--output',required=True,type=Path);p.add_argument('--stage',required=True);capture(p.parse_args())
