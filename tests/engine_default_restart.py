"""Restart the actual Studio, use the fixed card, create and play inherited defaults."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
from PIL import Image
from editor_restart import close_window

binary=Path(sys.argv[1]).resolve()
fixture=Path(sys.argv[2]).resolve()
root=Path(tempfile.mkdtemp(prefix='linblux-studio-default-restart-'))
subprocess.run([str(fixture),'--author',str(root)],check=True,timeout=30)
expected=json.loads((root/'expected.json').read_text())
(root/'home').mkdir()
# The Studio must resolve the developer's writable defaults from XDG data,
# never from the build/install tree.  Seed that real location explicitly.
custom_defaults=root/'data/linblux/engine-defaults'
shutil.copytree(root/'defaults',custom_defaults)
environment=dict(os.environ,HOME=str(root/'home'),XDG_DATA_HOME=str(root/'data'),
 XDG_CONFIG_HOME=str(root/'config'),XDG_STATE_HOME=str(root/'state'),
 LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2')
trace=root/'state/linblux/linblux.jsonl'
def events():
 return [json.loads(line) for line in trace.read_text().splitlines()] if trace.exists() else []
def xdo(*args):
 return subprocess.run(['xdotool',*map(str,args)],capture_output=True,text=True,check=True,timeout=5).stdout.strip()
def click(window,x,y):
 xdo('mousemove','--window',window,x,y);xdo('mousedown',1);time.sleep(.18);xdo('mouseup',1);time.sleep(.3)
def shot(name):
 path=root/(name+'.png');subprocess.run(['scrot','-o','-u',str(path)],check=True);return path
def primary_block(path,min_y=0,max_y=None,min_x=8,max_x=None):
 image=Image.open(path).convert('RGB');rows=[]
 max_y=image.height-8 if max_y is None else min(max_y,image.height-8)
 max_x=image.width-8 if max_x is None else min(max_x,image.width-8)
 for y in range(min_y,max_y):
  start=None
  for x in range(min_x,max_x):
   r,g,b=image.getpixel((x,y));red=abs(r-186)<18 and abs(g-40)<15 and abs(b-56)<18
   if red and start is None:start=x
   if (not red or x==max_x-1) and start is not None:
    end=x if not red else x+1
    if end-start>60:rows.append((y,start,end))
    start=None
 if not rows:return None
 bottom=max(y for y,_,_ in rows);group=[r for r in rows if bottom-30<=r[0]<=bottom]
 left=min(r[1] for r in group);right=max(r[2] for r in group);top=min(r[0] for r in group)
 return ((left+right)//2,(top+bottom)//2)

def run(phase):
 before=len(events())
 with (root/(phase+'.log')).open('w') as output:
  process=subprocess.Popen([str(binary)],cwd=root,env=environment,stdout=output,stderr=output)
  def wait(predicate,seconds=30):
   end=time.monotonic()+seconds
   while time.monotonic()<end:
    assert process.poll() is None,f'Studio exited; {root}'
    found=predicate()
    if found:return found
    time.sleep(.1)
   shot(phase+'-timeout');raise AssertionError(f'Timeout in {phase}; artifacts: {root}')
  try:
   def window_id():
    result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid),'--name','^LINBLUX Studio$'],capture_output=True,text=True,timeout=3)
    return result.stdout.strip().splitlines()[-1] if result.returncode==0 else None
   window=wait(window_id)
   wait(lambda:any(e.get('event')=='startup_ready' for e in events()[before:]))
   xdo('windowmove',window,0,0);xdo('windowsize',window,1280,900);xdo('windowfocus','--sync',window);time.sleep(3)
   shot(phase+'-home')
   if phase=='first':
    click(window,1130,425)
    opened=wait(lambda:next((e for e in events()[before:] if e.get('event')=='open' and 'engine-workspaces' in e.get('file','')),None))
    draft=Path(opened['file']);manifest=json.loads((draft/'experience.json').read_text());world=json.loads((draft/manifest['mainPlace']).read_text())
    assert world['game']['defaultRig']==expected['rig']
    assert world['game']['player']['gravity']==62
    assert draft.parent.name=='engine-workspaces'
    # Wait until the protected editor itself is rendered, not just until the
    # manager logged the open.  Under software rendering the Home transition
    # can still be visible for a few seconds.
    save_button=None
    deadline=time.monotonic()+30
    while time.monotonic()<deadline:
     save_button=primary_block(shot('actual-default-editor'),max_y=60,min_x=220,max_x=520)
     if save_button:break
     time.sleep(.25)
    assert save_button,f'Default editor did not expose its save action: {root}'
    seeded_index=(custom_defaults/'index.json').read_bytes()
    factory_index=(root/'defaults/index.json').read_bytes()
    click(window,*save_button)
    confirm=None
    deadline=time.monotonic()+15
    while time.monotonic()<deadline:
     confirm=primary_block(shot('actual-default-confirm'),min_y=350)
     if confirm:break
     time.sleep(.2)
    assert confirm,f'Save confirmation did not appear: {root}'
    click(window,*confirm)
    saved=wait(lambda:next((e for e in events()[before:] if e.get('event')=='engine_defaults_saved'),None))
    assert Path(saved['file']).resolve()==custom_defaults.resolve(),saved
    assert (custom_defaults/'backups').is_dir(),'Previous user index was not backed up'
    assert (custom_defaults/'index.json').read_bytes()!=seeded_index,'Save did not publish a new index'
    assert (root/'defaults/index.json').read_bytes()==factory_index,'Save wrote into the read-only factory fixture'
    time.sleep(.5);shot('actual-default-saved')
   else:
    click(window,150,320);time.sleep(2)
    xdo('mousemove','--window',window,730,650);xdo('click','--repeat',25,'--delay',25,5);time.sleep(1)
    image=Image.open(shot('actual-create-form')).convert('RGB')
    # Locate the solid LINBLUX primary action in the lower right of the form.
    rows=[]
    for y in range(image.height//2,image.height-8):
     start=None
     for x in range(image.width*3//5,image.width-8):
      r,g,b=image.getpixel((x,y));red=abs(r-186)<18 and abs(g-40)<15 and abs(b-56)<18
      if red and start is None:start=x
      if not red and start is not None:
       if x-start>70:rows.append((y,start,x))
       start=None
    assert rows,f'Create action not visible: {root}'
    bottom=max(y for y,_,_ in rows);group=[r for r in rows if bottom-24<=r[0]<=bottom]
    _,left,right=group[len(group)//2];click(window,(left+right)//2,sum(y for y,_,_ in group)//len(group))
    opened=wait(lambda:next((e for e in events()[before:] if e.get('event')=='open' and '/experiences/' in e.get('file','')),None))
    project=Path(opened['file']);manifest=json.loads((project/'experience.json').read_text());place=project/manifest['mainPlace'];original=place.read_bytes();world=json.loads(original)
    assert world['game']['defaultRig']==expected['rig']
    assert any(o.get('name')=='MarcadorPadrao' for o in world['objects'])
    # The editor is usable as soon as the open event is logged, but under a
    # loaded software renderer its first frame can still be a moment away.
    # Retry the real header button instead of treating one lost click as a
    # runtime failure; a delayed click can otherwise hit the overlaying Home.
    time.sleep(1.2)
    start=None
    for _ in range(3):
     click(window,1167,16)
     deadline=time.monotonic()+5
     while time.monotonic()<deadline and not start:
      start=next((e for e in events()[before:] if e.get('subsystem')=='runtime' and e.get('event')=='start'),None)
      if not start:time.sleep(.1)
     if start:break
    assert start,'Play button did not start the inherited character';assert start.get('context',{}).get('rig')==expected['rig'],start
    time.sleep(1);shot('actual-new-character-playing')
    xdo('keydown','Shift_L');xdo('keydown','F5');time.sleep(.2);xdo('keyup','F5');xdo('keyup','Shift_L')
    wait(lambda:any(e.get('subsystem')=='runtime' and e.get('event')=='stop' for e in events()[before:]))
    assert place.read_bytes()==original,'Play changed authored experience'
   close_window(window);assert process.wait(timeout=15)==0
  finally:
   if process.poll() is None:process.terminate();process.wait(timeout=10)
run('first');run('restarted')
assert Path(expected['oldPlacePath']).read_text()==expected['oldPlace']
assert (Path(expected['oldRoot'])/'experience.json').read_text()==expected['oldManifest']
print(f'Actual LINBLUX Studio: fixed card, protected default editor, clean exit/restart, creation and runtime character passed. Artifacts: {root}')
