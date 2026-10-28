"""Reproducible phase 0/1 fixtures and actual Studio captures (run via TURBO).

No personal projects are read or written. Generated assets stay in the output.
Camera: unchanged EditorCamera reset; fixed sun, disabled animated sky.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import time
import zlib


def fixtures(root):
    root.mkdir(parents=True, exist_ok=True)
    # Author-owned calibration decal, generated without an image dependency.
    def chunk(kind, data):
        return struct.pack('!I', len(data)) + kind + data + struct.pack('!I', zlib.crc32(kind + data))
    pixels = b''.join(b'\0' + b''.join(bytes((225, 40, 150) if (x//8+y//8)%2 else (245, 220, 65))
                                     for x in range(64)) for y in range(64))
    decal = root / 'calibration.png'
    decal.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('!2I5B',64,64,8,2,0,0,0))
                     + chunk(b'IDAT', zlib.compress(pixels)) + chunk(b'IEND', b''))
    for name in ('grey-room', 'material-gallery', 'editor-snap'):
        objects = [dict(id=1, parent=0, name='Workspace', **{'class':'Workspace'}),
                   dict(id=2, parent=0, name='Fixed lighting', **{'class':'Lighting'},
                        skyEnabled=False, cloudSpeed=0, sunDirection=[-.45,-1,.55],
                        sunIntensity=1, brightness=1, skyColor=[120,180,240,255],
                        ambientColor=[150,150,160,255])]
        def part(label, position, size, shape='Block', material='SmoothPlastic', alpha=255, rotation=None):
            i = len(objects)+1
            objects.append(dict(id=i,parent=1,name=label,**{'class':'Part'},shape=shape,
                                position=position,size=size,rotation=rotation or [0,0,0],
                                color=[163,162,165,alpha],material=material,anchored=True,canCollide=True))
            return i
        part('Floor', [0,-.5,0], [48,1,48])
        if name == 'grey-room':
            part('Long back wall', [0,5,-10], [36,10,1])
            part('Side wall', [-15,5,0], [1,10,20])
            target = part('Decal block', [2,3,0], [8,6,2])
            objects.append(dict(id=len(objects)+1,parent=target,name='Calibration decal',
                                **{'class':'Decal'},surface=dict(image=str(decal),face=1)))
            part('Sphere unequal axes', [10,2,2], [8,4,6], 'Sphere')
        elif name == 'material-gallery':
            for i, material in enumerate(('SmoothPlastic','Plastic','Wood','Metal','Concrete','Neon')):
                part(material, [(i%3-1)*7,2,(i//3)*8-4], [4,4,4], material=material)
                part(material+' sphere', [(i%3-1)*7,6,(i//3)*8-4], [3,3,3], 'Sphere', material)
            part('Transparent', [0,3,10], [12,6,1], alpha=100)
        else:
            for i, angle in enumerate((0,15,30,45,90)):
                part('Rotated '+str(angle), [(i-2)*6,2,0], [4,4,1], rotation=[0,angle,0])
            objects.append(dict(id=len(objects)+1,parent=1,name='Distant pivot', **{'class':'Model'},
                                modelPivot=dict(position=[50,2,0],rotation=[0,0,0,1],scale=1)))
            model = objects[-1]['id']
            part('Model child', [0,2,-8], [4,4,2]); objects[-1]['parent'] = model
        world = dict(format='loblox-project',version=6,nextId=len(objects)+1,objects=objects)
        (root/(name+'.json')).write_text(json.dumps(world,indent=2)+'\n')


def capture(binary, root, stage):
    from PIL import Image, ImageChops
    output = root/stage
    output.mkdir(parents=True,exist_ok=True)
    summary = []
    for name in ('grey-room','material-gallery','editor-snap'):
        project = root/'fixtures'/(name+'.json')
        # Reject invalid fixtures before opening Studio (which correctly falls
        # back to its default scene on a failed load).
        validator=Path(__file__).resolve().parent.parent/'build-linblux/linblux_player'
        subprocess.run([str(validator),'--validate',str(project)],check=True,timeout=15,
                       env=dict(os.environ,XDG_STATE_HOME=str(output/'validation-state')))
        original = project.read_bytes()
        isolated = output/name
        isolated.mkdir(exist_ok=True)
        environment = dict(os.environ, HOME=str(isolated), XDG_DATA_HOME=str(isolated/'data'),
                           XDG_CONFIG_HOME=str(isolated/'config'), XDG_STATE_HOME=str(isolated/'state'),
                           LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',LOBLOX_UI_TIMINGS='1')
        log = output/(name+'.log')
        with log.open('w') as stream:
            process = subprocess.Popen([str(binary),'--project',str(project)],cwd=isolated,
                                       env=environment,stdout=stream,stderr=stream)
            def xdo(*args):
                return subprocess.run(['xdotool',*map(str,args)],check=True,capture_output=True,text=True,timeout=5).stdout.strip()
            try:
                deadline = time.monotonic()+25
                window = None
                while time.monotonic()<deadline:
                    assert process.poll() is None, log
                    found = subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid),
                                            '--name','^LINBLUX Studio$'],capture_output=True,text=True,timeout=3)
                    if found.returncode == 0:
                        window = found.stdout.splitlines()[-1]; break
                    time.sleep(.1)
                assert window, log
                for width,height in ((1366,768),(1920,1080)):
                    xdo('windowmove',window,0,0); xdo('windowsize',window,width,height)
                    xdo('windowfocus','--sync',window); xdo('mousemove',5,5)
                    time.sleep(3)
                    shot = output/(name+'-'+str(width)+'.png')
                    subprocess.run(['scrot','-o','-u',str(shot)],check=True,timeout=5)
                    assert Image.open(shot).size == (width,height)
                    timing = re.findall(r'RML timing ms:.*',log.read_text())
                    rss = re.search(r'VmRSS:\s+(\d+) kB',Path('/proc',str(process.pid),'status').read_text())
                    summary.append(dict(fixture=name,resolution=[width,height],image=str(shot),
                                        rss_kib=int(rss[1]),timing=timing[-1] if timing else 'unavailable',
                                        project_sha256=hashlib.sha256(original).hexdigest()))
                    if stage == 'after' and name == 'grey-room':
                        xdo('mousemove',400,190);time.sleep(.5)
                        def f8():
                            xdo('keydown','F8');time.sleep(.3);xdo('keyup','F8');time.sleep(.7)
                        f8()
                        debug = output/('f8-on-'+str(width)+'.png')
                        subprocess.run(['scrot','-o','-u',str(debug)],check=True,timeout=5)
                        crop=(215,200,width-300,height-75)
                        difference=ImageChops.difference(Image.open(shot).convert('RGB'),
                                                        Image.open(debug).convert('RGB')).crop(crop)
                        assert difference.getbbox(), 'F8 did not change the actual viewport'
                        f8();xdo('mousemove',5,5);time.sleep(.5)
                # Xvfb has no window manager to implement Alt+F4.
                from editor_restart import close_window
                close_window(window)
                assert process.wait(timeout=10) == 0
            finally:
                if process.poll() is None:
                    process.terminate(); process.wait(timeout=5)
        assert project.read_bytes() == original, 'Capture changed fixture'
    (output/'measurements.json').write_text(json.dumps(summary,indent=2)+'\n')
    print(json.dumps(summary,indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--binary',type=Path)
    parser.add_argument('--stage',choices=['before','after'])
    args = parser.parse_args()
    root = args.output.resolve()
    if not (root/'fixtures').exists():
        fixtures(root/'fixtures')
    if args.binary:
        assert args.stage
        capture(args.binary.resolve(),root,args.stage)
