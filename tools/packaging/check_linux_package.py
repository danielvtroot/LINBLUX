#!/usr/bin/env python3
"""Supervised smoke check of our own candidate outside the source checkout."""
import hashlib,json,os,subprocess,sys,tarfile,tempfile,time
from pathlib import Path
artifact=Path(sys.argv[1]).resolve();root=Path(tempfile.mkdtemp(prefix='linblux-clean-install-'))
with tarfile.open(artifact) as archive:archive.extractall(root,filter='data')
prefix=root/'linblux-alpha-0.1.0';manifest=json.loads((prefix/'manifest.json').read_text())
for name,digest in manifest['sha256'].items():
    path=prefix/name;assert path.resolve().is_relative_to(prefix.resolve()),'unsafe manifest path'
    assert hashlib.sha256(path.read_bytes()).hexdigest()==digest,'hash mismatch: '+name
assert not list(prefix.rglob('*.mp4')),'video with unverified rights included'
run=root/'run';run.mkdir();environment=dict(os.environ,HOME=str(root/'home'),XDG_DATA_HOME=str(root/'data'),XDG_CONFIG_HOME=str(root/'config'),XDG_STATE_HOME=str(root/'state'),LIBGL_ALWAYS_SOFTWARE='1',LP_NUM_THREADS='2')
for directory in ['home','data','config','state']:(root/directory).mkdir()
player=prefix/'bin/linblux_player';studio=prefix/'bin/linblux';demo=prefix/'share/linblux/examples/rifledemo/world.loblox.json'
for arguments in [['--validate',str(demo)],['--project',str(demo),'--frames','8','--check-scripts']]:
    result=subprocess.run([str(player),*arguments],cwd=run,env=environment,capture_output=True,text=True,timeout=25)
    assert result.returncode==0,result.stdout+result.stderr
log=(root/'studio.log').open('w');process=subprocess.Popen([str(studio)],cwd=run,env=environment,stdout=log,stderr=log)
try:
    window='';deadline=time.monotonic()+20
    while not window and time.monotonic()<deadline:
        assert process.poll() is None,'Studio exited early; '+(root/'studio.log').read_text()[-2000:]
        result=subprocess.run(['xdotool','search','--onlyvisible','--pid',str(process.pid)],capture_output=True,text=True,timeout=3)
        if result.returncode==0:window=result.stdout.splitlines()[-1]
        time.sleep(.2)
    assert window,'Studio window unavailable';time.sleep(2)
    subprocess.run(['xdotool','windowfocus',window],check=True,timeout=5)
    subprocess.run(['scrot','-u','-o',str(root/'studio-home.png')],check=True,timeout=5)
    assert process.poll() is None,'Studio failed after first render'
finally:
    if process.poll() is None:process.terminate()
    try:process.wait(timeout=5)
    except subprocess.TimeoutExpired:process.kill();process.wait()
    log.close()
print(json.dumps({'status':'PASSED','files_verified':len(manifest['sha256']),'outside_checkout':str(root),'player':'validated and rendered','studio':'opened and captured; terminated by supervisor','os_installation':'not tested','release':'still blocked by release gates'},ensure_ascii=False))
