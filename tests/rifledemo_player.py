"""RifleDemo in the real Player, with X11 controls and Lua assertions."""
import json, os, shutil, subprocess, sys, tempfile, time
from pathlib import Path
from PIL import Image
player=Path(sys.argv[1]).resolve();demo=Path(__file__).resolve().parent.parent/'assets/examples/rifledemo'
root=Path(tempfile.mkdtemp(prefix='linblux-rifle-player-'));shutil.copytree(demo/'assets',root/'assets')
world=json.loads((demo/'world.loblox.json').read_text())
watchdog='''
local collects,equips,drops,fires,reloads=0,0,0,0,0
local original,damaged,guideStart,walked
linblux.events.on('inventory_changed',function(e)
 if e.action=='collect' then
  collects=collects+1
  if original then assert(e.item.id==original,'identidade perdida') else original=e.item.id end
 elseif e.action=='equip' then equips=equips+1
 elseif e.action=='drop' then drops=drops+1 end
end)
linblux.events.on('item_action',function(e)
 if e.action=='activate' then fires=fires+1;assert(e.ammo<e.magazine,'munição não diminuiu') end
 if e.action=='reload_end' then
  reloads=reloads+1
  linblux.ui.create('panel',{anchor='bottom-left',x=16,y=-50,width=150,height=24,background='#FF00FFFF'})
 end
end)
linblux.events.on('update',function()
 for _,o in ipairs(linblux.scene.root:children()) do
  if o.name=='GuiaLINBLUX' then
   local p=o.position
   if not guideStart then guideStart=p end
   if (p.x-guideStart.x)^2+(p.z-guideStart.z)^2>1 and linblux.npc.animation(o)=='walk' then walked=true end
  end
  if o:get_attribute('alvo') and linblux.npc.health(o)<100 then damaged=true end
 end
end)
task.spawn(function()
 local deadline=linblux.time.now()+15
 repeat task.wait(.1) until (collects>=2 and equips>=2 and drops>=1 and walked and fires>=2 and reloads>=1 and damaged) or linblux.time.now()>=deadline
 assert(collects>=2 and equips>=2 and drops>=1,'E/1/Delete/recoleta não executados')
 assert(walked,'guia não caminhou com animação')
 assert(fires>=2 and reloads>=1 and damaged,'tiro, recarga ou dano não executados')
 assert(#linblux.items.list()==1 and linblux.items.equipped(),'inventário final inválido')
 linblux.ui.create('panel',{anchor='bottom-left',x=16,y=-16,width=150,height=24,background='#00FFAEFF'})
end)
'''
world['objects'].append(dict(id=5000,parent=5,name='VerificarFluxo.lua',**{'class':'Script'},source=watchdog,enabled=True));world['nextId']=5001
project=root/'RifleDemo.loblox.json';project.write_text(json.dumps(world))
env=dict(os.environ,HOME=str(root),XDG_CONFIG_HOME=str(root/'config'),XDG_STATE_HOME=str(root/'state'),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2')
log=(root/'player.log').open('w');p=subprocess.Popen([str(player),'--project',str(project),'--frames','1100','--check-scripts'],cwd=root,env=env,stdout=log,stderr=log)
def cmd(*args):return subprocess.run(['xdotool',*map(str,args)],check=True,capture_output=True,text=True,timeout=5).stdout
try:
 window='';deadline=time.monotonic()+20
 while not window and time.monotonic()<deadline:
  assert p.poll() is None,'Player exited early'
  f=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(p.pid)],capture_output=True,text=True,timeout=3)
  if f.returncode==0:window=f.stdout.splitlines()[-1]
  time.sleep(.1)
 assert window,'Player window unavailable';cmd('windowfocus',window);cmd('windowmove',window,0,0);time.sleep(2)
 def key(k,duration=.25):cmd('keydown',k);time.sleep(duration);cmd('keyup',k);time.sleep(.25)
 def shoot():cmd('mousemove','--window',window,640,360);cmd('mousedown',1);time.sleep(.2);cmd('mouseup',1);time.sleep(.25)
 def capture(name):
  path=root/(name+'.png');subprocess.run(['scrot','-u','-o',str(path)],check=True,timeout=5);return Image.open(path).convert('RGB')
 def hotbar_red(im):return sum(1 for y in range(690,710) for x in range(550,740) if (lambda c:abs(c[0]-215)<4 and abs(c[1]-38)<4 and abs(c[2]-63)<4)(im.getpixel((x,y))))
 def collect_and_equip():
  # Wait for the actual inventory state; software GL does not guarantee that
  # a fixed wall-clock hold spans the required simulated collection time.
  deadline=time.monotonic()+6
  cmd('keydown','e')
  try:
   while time.monotonic()<deadline:
    key('1',.05)
    image=capture('collecting')
    if hotbar_red(image)>100:return image
   raise AssertionError('collection/equip did not complete within 6 seconds; '+str(root))
  finally:cmd('keyup','e')
 assert hotbar_red(capture('empty'))==0,'hotbar appears before collection'
 equipped=collect_and_equip();assert hotbar_red(equipped)>100,'actual hotbar did not equip'
 left=sum(1 for c in equipped.crop((930,278,980,300)).getdata() if min(c)>120 and max(c)-min(c)<18)
 right=sum(1 for c in equipped.crop((980,278,1025,300)).getdata() if min(c)>120 and max(c)-min(c)<18)
 assert left>right+100,'official logo decal is absent or horizontally mirrored'
 shoot();key('r')
 # Observe the actual reload_end event before dropping (which cancels reload).
 # A fixed real-time sleep is not a simulation-time guarantee under llvmpipe.
 deadline=time.monotonic()+8;reloaded=False
 while not reloaded and time.monotonic()<deadline:
  assert p.poll() is None,(root/'player.log').read_text()[-3000:]
  image=capture('reload-complete')
  reloaded=sum(1 for c in image.crop((0,592,240,720)).getdata() if c[0]>235 and c[1]<25 and c[2]>235)>1000
  if not reloaded:time.sleep(.15)
 assert reloaded,'reload_end not observed before drop; '+str(root)
 key('Delete');assert hotbar_red(capture('dropped'))==0,'hotbar survived Delete'
 collect_and_equip();shoot()
 # Wait for the assertion marker, not a fixed wall-clock delay: simulation
 # time is bounded per frame and need not track llvmpipe rendering time.
 deadline=time.monotonic()+12;cyan=0
 while cyan<=1000 and time.monotonic()<deadline:
  assert p.poll() is None,(root/'player.log').read_text()[-3000:]
  time.sleep(.2);im=capture('verified');cyan=sum(1 for c in im.getdata() if c[0]<25 and c[1]>235 and 145<c[2]<195)
 assert cyan>1000,'Lua assertions did not reach their success marker'
 key('Escape');cmd('mousemove','--window',window,50,685);cmd('mousedown',1);time.sleep(.25);cmd('mouseup',1)
 p.wait(timeout=6);assert p.returncode==0,(root/'player.log').read_text()[-3000:]
 print('RifleDemo real Player: E, 1, shooting, NPC damage, reload, Delete, same-ID recollection, HUD/Lua UI: OK;',root)
finally:
 if p.poll() is None:p.terminate()
 try:p.wait(timeout=5)
 except subprocess.TimeoutExpired:p.kill();p.wait()
 log.close()
