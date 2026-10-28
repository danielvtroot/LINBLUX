"""Actual Studio capture of the shipped rig/ground; run via TURBO supervision."""
import argparse,hashlib,json,os,subprocess,time
from pathlib import Path
from editor_restart import close_window,wait_for,xdo
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--project',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
a.output=a.output.resolve();a.output.mkdir(parents=True,exist_ok=True);a.project=a.project.resolve();original=a.project.read_bytes()
env=dict(os.environ,HOME=str(a.output),XDG_DATA_HOME=str(a.output/'data'),XDG_CONFIG_HOME=str(a.output/'config'),XDG_STATE_HOME=str(a.output/'state'),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',LINBLUX_RENDER_QUALITY='medium')
with (a.output/'studio.log').open('w') as log:
 proc=subprocess.Popen([str(a.binary.resolve()),'--project',str(a.project)],cwd=a.output,env=env,stdout=log,stderr=log)
 try:
  def find_window():
   r=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(proc.pid),'--name','^LINBLUX Studio$'],text=True,capture_output=True,timeout=3)
   return r.stdout.splitlines()[-1] if r.returncode==0 else None
  w=wait_for(find_window,proc,25);xdo('windowfocus','--sync',w);time.sleep(4)
  for width,height in ((1366,768),(1920,1080)):
   xdo('windowmove',w,0,0);xdo('windowsize',w,width,height);time.sleep(3)
   if width==1366:
    xdo('mousemove',600,400);time.sleep(.4);xdo('click','--repeat',8,'--delay',350,4);time.sleep(2)
   xdo('mousemove',5,5);subprocess.run(['scrot','-o','-u',str(a.output/f'character-{width}.png')],check=True,timeout=5)
  close_window(w);assert proc.wait(timeout=10)==0
 finally:
  if proc.poll() is None:proc.terminate();proc.wait(timeout=5)
assert a.project.read_bytes()==original
(a.output/'manifest.json').write_text(json.dumps(dict(project_sha256=hashlib.sha256(original).hexdigest(),binary_sha256=hashlib.sha256(a.binary.read_bytes()).hexdigest(),quality='medium',camera='initial orbit, 8 scroll up',images=['character-1366.png','character-1920.png']),indent=2))
