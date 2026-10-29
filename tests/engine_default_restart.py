"""Restart the actual Studio, use the fixed card, create and play inherited defaults."""
import json
import os
from pathlib import Path
import re
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
 LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2',LOBLOX_TRACE_UI='1')
trace=root/'state/linblux/linblux.jsonl'
def events():
 return [json.loads(line) for line in trace.read_text().splitlines()] if trace.exists() else []
def xdo(*args):
 return subprocess.run(['xdotool',*map(str,args)],capture_output=True,text=True,check=True,timeout=5).stdout.strip()
def click(window,x,y):
 # Loading a project compiles materials on the first rendered frames. Give
 # the pointer a frame to enter the new UI, then keep down/up in separate
 # llvmpipe frames; an event burst immediately after 'open' is not a click.
 xdo('mousemove','--window',window,x,y);time.sleep(.3)
 xdo('mousedown',1);time.sleep(.5);xdo('mouseup',1);time.sleep(.4)
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
def brand_tile(path,min_x,min_y,max_x,max_y,min_pixels=60):
 # Locate the LINBLUX red brand tile; every card/logo uses the same mark.
 image=Image.open(path).convert('RGB');points=[]
 for y in range(min_y,max_y):
  for x in range(min_x,max_x):
   r,g,b=image.getpixel((x,y))
   if r>110 and r>g*1.7 and r>b*1.3:points.append((x,y))
 if len(points)<min_pixels:return None
 return (sum(x for x,_ in points)//len(points),sum(y for _,y in points)//len(points),image.width)
def edit_default_target(path):
 tile=brand_tile(path,60,280,130,380)
 if not tile:return None
 # The card action is right-aligned; keep it relative to the window width so
 # resizing or a shifted card cannot send the click into the copy area.
 return (tile[2]-128,tile[1])
def dialog_primary(path):
 # The confirmation dialog is centred; keep the scan in its lower half so a
 # red object in the 3D viewport cannot be mistaken for the primary action.
 image=Image.open(path).convert('RGB');rows=[]
 x0=max(8,image.width//2-180);x1=min(image.width-8,image.width//2+300)
 y0=max(0,image.height//2-140);y1=min(image.height-8,image.height//2+260)
 for y in range(y0,y1):
  start=None
  for x in range(x0,x1):
   r,g,b=image.getpixel((x,y));red=abs(r-186)<18 and abs(g-40)<15 and abs(b-56)<18
   if red and start is None:start=x
   if (not red or x==x1-1) and start is not None:
    if x-start>70:rows.append((y,start,x))
    start=None
 if not rows:return None
 bottom=max(y for y,_,_ in rows);group=[r for r in rows if bottom-24<=r[0]<=bottom]
 left=min(r[1] for r in group);right=max(r[2] for r in group)
 return ((left+right)//2,sum(y for y,_,_ in group)//len(group))
def traced_control(log,element_id):
 # The Studio reports its real element geometry after every resize when
 # LOBLOX_TRACE_UI is set; target the control by id, never by a guess.
 pattern=re.compile(r'#'+re.escape(element_id)+r' \S*: ([\d.]+)x([\d.]+) at ([\d.]+),([\d.]+)')
 found=pattern.findall(log.read_text())
 if not found:return None
 width,height,x,y=(float(value) for value in found[-1])
 return (x+width/2,y+height/2)
def footer_primary(path):
 image=Image.open(path).convert('RGB')
 rows=[]
 for y in range(image.height-130,image.height-8):
  start=None
  for x in range(image.width//2,image.width-8):
   r,g,b=image.getpixel((x,y));red=abs(r-186)<18 and abs(g-40)<15 and abs(b-56)<18
   if red and start is None:start=x
   if (not red or x==image.width-9) and start is not None:
    if x-start>80:rows.append((y,start,x))
    start=None
 if not rows:return None
 bottom=max(y for y,_,_ in rows);group=[r for r in rows if bottom-24<=r[0]<=bottom]
 left=min(r[1] for r in group);right=max(r[2] for r in group)
 return ((left+right)//2,sum(y for y,_,_ in group)//len(group))

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
    target=wait(lambda:edit_default_target(shot('actual-home-card')),15)
    click(window,*target)
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
     confirm=dialog_primary(shot('actual-default-confirm'))
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
    # Home primary action: the solid LINBLUX red button in the hero area.
    create=wait(lambda:primary_block(shot('actual-home-create'),min_y=150,max_y=300,min_x=20,max_x=400),15)
    click(window,*create)
    # The create page keeps its default template (engine-default) and the
    # primary action pinned in the footer; locate it by colour, never by a
    # hard-coded pair of coordinates.
    submit=None
    deadline=time.monotonic()+20
    while time.monotonic()<deadline:
     submit=footer_primary(shot('actual-create-form'))
     if submit:break
     time.sleep(.2)
    assert submit,f'Create action not visible: {root}'
    click(window,*submit)
    opened=wait(lambda:next((e for e in events()[before:] if e.get('event')=='open' and '/experiences/' in e.get('file','')),None))
    project=Path(opened['file']);manifest=json.loads((project/'experience.json').read_text());place=project/manifest['mainPlace'];original=place.read_bytes();world=json.loads(original)
    assert world['game']['defaultRig']==expected['rig']
    assert any(o.get('name')=='MarcadorPadrao' for o in world['objects'])
    # Start Play through the editor's own play control, located by element id
    # in the Studio's geometry trace.  The F5 shortcut only reaches the editor
    # when the pointer owns the viewport, and this flow ends on the form footer.
    play=None
    deadline=time.monotonic()+20
    while time.monotonic()<deadline:
     play=traced_control(root/(phase+'.log'),'play')
     if play:break
     time.sleep(.2)
    assert play,f'Editor play control was not traced: {root}'
    click(window,*play)
    start=wait(lambda:next((e for e in events()[before:] if e.get('subsystem')=='runtime' and e.get('event')=='start'),None))
    assert start.get('context',{}).get('rig')==expected['rig'],start
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
