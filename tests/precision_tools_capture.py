"""Capture actual Studio precision tools over a copy of the shipped world.
Run only under TURBO supervision; no author project is modified.
"""
import argparse,copy,hashlib,json,os,subprocess,time
from pathlib import Path
from editor_restart import close_window,xdo,wait_for

def prepare(root):
    source=Path(__file__).resolve().parents[1]/'assets/defaults/factory/world.loblox.json'
    world=json.loads(source.read_text());objects=world['objects'];next_id=max(o['id'] for o in objects)+1
    def part(name,position,size,rotation=None):
        nonlocal next_id
        p=dict(id=next_id,parent=1,name=name,**{'class':'Part'},shape='Block',position=position,size=size,rotation=rotation or [0,0,0],color=[163,162,165,255],material='SmoothPlastic',anchored=True,canCollide=True)
        next_id+=1;objects.append(p);return p
    wall=part('Parede 191 — teste real de construção',[0,5,0],[191,10,1]);objects.remove(wall);objects.insert(2,wall)
    part('Parede adjacente',[12,5,8],[1,10,15]);part('Peça girada',[7,2,8],[4,4,1],[0,30,0])
    for o in objects:
        if o['class']=='Lighting':o.update(skyEnabled=False,sunDirection=[-.45,-1,.55],sunIntensity=1,brightness=1,fogDensity=0)
    # Real shipped rig, also present while building; use its authored data.
    character=json.loads((source.parent/'character.loblox.json').read_text())
    world['game']['rigs']=copy.deepcopy(character['game']['rigs']);world['game']['defaultRig']=character['game']['defaultRig']
    rigs=world['game']['rigs']
    if rigs:rigs[0]['root']['position']=[8,3,6]
    objects.append(dict(id=next_id,parent=wall['id'],name='Decal de construção',**{'class':'Decal'},surface=dict(image=str(Path(__file__).resolve().parents[1]/'tests/fixtures/surface-test.png'),face=1)))
    next_id+=1
    world['nextId']=next_id
    path=root/'construction.json';path.write_text(json.dumps(world,indent=2)+'\n')
    (root/'source.json').write_text(json.dumps(dict(source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),wall=wall['id'],rigs=len(rigs)),indent=2))
    return path

def capture(args):
    root=args.output.resolve();root.mkdir(parents=True,exist_ok=True)
    project=root/'construction.json'
    if not project.exists():project=prepare(root)
    before=project.read_bytes();output=root/args.stage;output.mkdir(exist_ok=True)
    env=dict(os.environ,HOME=str(output),XDG_DATA_HOME=str(output/'data'),XDG_CONFIG_HOME=str(output/'config'),XDG_STATE_HOME=str(output/'state'),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2')
    def click(x,y):
        xdo('mousemove',x,y);xdo('mousedown',1);time.sleep(.25);xdo('mouseup',1);time.sleep(.4)
    with (output/'studio.log').open('w') as log:
        proc=subprocess.Popen([str(args.binary.resolve()),'--project',str(project)],cwd=output,env=env,stdout=log,stderr=log)
        try:
            def find_window():
                r=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(proc.pid),'--name','^LINBLUX Studio$'],text=True,capture_output=True,timeout=3)
                return r.stdout.splitlines()[-1] if r.returncode==0 else None
            window=wait_for(find_window,proc,25)
            for width,height in ((1366,768),(1920,1080)):
                xdo('windowsize',window,width,height);xdo('windowmove',window,0,0);xdo('windowfocus','--sync',window);time.sleep(2)
                click(width-220,243)
                for distance,zoom in [('close',0),('far',-4)]:
                    if zoom:
                        xdo('mousemove',int(width*.45),int(height*.5));xdo('click','--repeat',4,'--delay',120,5);time.sleep(.5)
                    for name,x in [('move',133 if width==1366 else 145),('scale',214 if width==1366 else 233),('rotate',295 if width==1366 else 320)]:
                        click(x,92);xdo('mousemove',5,5);time.sleep(.7)
                        subprocess.run(['scrot','-o','-u',str(output/f'{name}-{distance}-{width}.png')],check=True,timeout=5)
                # Return to the same camera before resizing.
                xdo('mousemove',int(width*.45),int(height*.5));xdo('click','--repeat',4,'--delay',120,4);time.sleep(.5)
            close_window(window);assert proc.wait(timeout=10)==0
        finally:
            if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
    assert project.read_bytes()==before
    (output/'manifest.json').write_text(json.dumps(dict(project_sha256=hashlib.sha256(before).hexdigest(),binary_sha256=hashlib.sha256(args.binary.read_bytes()).hexdigest(),images=sorted(p.name for p in output.glob('*.png'))),indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--output',type=Path,required=True);p.add_argument('--stage',required=True);capture(p.parse_args())
