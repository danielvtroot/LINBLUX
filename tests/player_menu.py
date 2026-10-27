"""Standalone Player: real Esc/menu clicks, fullscreen and resolution captures."""
import json, os, subprocess, sys, tempfile, time
from pathlib import Path
from PIL import Image
player=Path(sys.argv[1]).resolve();root=Path(tempfile.mkdtemp(prefix='linblux-player-menu-'))
world=json.loads((Path(__file__).resolve().parent.parent/'assets/defaults/factory/world.loblox.json').read_text())
for node in world['objects']:
    if node.get('class')=='Script':node['source']=''
script_id=max(node['id'] for node in world['objects'])+1
watchdog="""
local marker=linblux.ui.create('panel',{anchor='bottom-right',x=-16,y=-16,width=40,height=40,background='#00FF00FF'})
linblux.events.on('update',function()
 assert(not linblux.input.key_down('W'),'menu deixou tecla de gameplay chegar ao Lua')
 local color=math.floor(linblux.time.now()*4)%2==0 and '#00FF00FF' or '#0000FFFF'
 linblux.ui.set(marker,{background=color})
end)
"""
world['objects'].append(dict(id=script_id,parent=1,name='MenuSemPausa.lua',**{'class':'Script'},source=watchdog,enabled=True))
world['nextId']=script_id+1
project=root/'Menu.loblox.json';project.write_text(json.dumps(world))
env=dict(os.environ,HOME=str(root),XDG_CONFIG_HOME=str(root/'config'),XDG_STATE_HOME=str(root/'state'),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2')
log=(root/'player.log').open('w');process=subprocess.Popen([str(player),'--project',str(project),'--frames','2400','--check-scripts'],cwd=root,env=env,stdout=log,stderr=log)
def command(*args):return subprocess.run(['xdotool',*map(str,args)],check=True,capture_output=True,text=True,timeout=8).stdout
try:
    deadline=time.monotonic()+20;window=''
    while not window and time.monotonic()<deadline:
        assert process.poll() is None,'Player exited before window'
        found=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],capture_output=True,text=True,timeout=3)
        if found.returncode==0:window=found.stdout.splitlines()[-1]
        time.sleep(.1)
    assert window,'no Player window';command('windowfocus',window);command('windowmove',window,0,0);time.sleep(2)
    def key(value):
        command('keydown',value);time.sleep(.25);command('keyup',value);time.sleep(.3)
    def click(x,y):command('mousemove','--window',window,int(x),int(y));command('mousedown',1);time.sleep(.25);command('mouseup',1);time.sleep(.25)
    def shot(name):
        path=root/(name+'.png');subprocess.run(['scrot','-o','-u',str(path)],check=True,timeout=5);return Image.open(path).convert('RGB')
    for width,height in [(1280,720),(1920,1080)]:
        command('windowsize',window,width,height);time.sleep(1);key('Escape');image=shot('menu-'+str(width))
        first=image.getpixel((width-30,height-30));changed=False
        for attempt in range(6):
            time.sleep(.15);current=shot('menu-running-'+str(width)).getpixel((width-30,height-30))
            if sum(abs(a-b) for a,b in zip(first,current))>80:changed=True;break
        assert changed,'scripts/tempo congelaram ao abrir o menu'
        key('w') # watchdog Lua exige que a tecla permaneça bloqueada
        # Actual left panel covers the window edge; the world remains visible right.
        r,g,b=image.getpixel((8,180));assert max(r,g,b)<45,(r,g,b)
        scale=height/720;click(int(70*scale),int(270*scale));shot('audio-'+str(width))
        key('Escape');closed=shot('closed-'+str(width));assert max(closed.getpixel((8,180)))>55,'Esc did not remove side panel'
    command('windowsize',window,1280,720);time.sleep(.4);key('Escape');click(50,312);click(360,108);time.sleep(.4)
    assert 'WIDTH=1920' in command('getwindowgeometry','--shell',window),'fullscreen did not expand to monitor'
    shot('video-fullscreen');click(540,162);time.sleep(.4)
    assert 'WIDTH=1280' in command('getwindowgeometry','--shell',window),'fullscreen did not restore window'
    click(50,685);process.wait(timeout=6)
    assert process.returncode==0,'Player exit failed'
    print('Player menu: Esc, mouse, Audio/Video, fullscreen, exit and 720p/1080p captures: OK;',root)
finally:
    if process.poll() is None:process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait()
    log.close()
