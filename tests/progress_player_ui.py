"""Real graphical Player restarts with isolated private state, under Xvfb/TURBO."""
import argparse
import json
import os
from pathlib import Path
import stat
import subprocess
import tempfile


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--player",type=Path,required=True)
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    scene=json.loads((root/"assets/defaults/factory/world.loblox.json").read_text())
    workspace=next(o["id"] for o in scene["objects"] if o["class"]=="Workspace")
    identifier=scene["nextId"];scene["nextId"]+=1
    source=(root/"assets/examples/lua/persistencia.lua").read_text()
    scene["objects"].append({"class":"Script","id":identifier,"name":"Persistencia.lua","parent":workspace,"source":source,"enabled":True})
    with tempfile.TemporaryDirectory(prefix="linblux-progress-player-") as temporary:
        base=Path(temporary);path=base/"world.json";path.write_text(json.dumps(scene,ensure_ascii=False))
        before=path.read_bytes()
        env={**os.environ,"XDG_STATE_HOME":str(base/"state"),"XDG_CONFIG_HOME":str(base/"config"),"XDG_DATA_HOME":str(base/"data")}
        for visits in (1,2):
            result=subprocess.run([str(args.player.resolve()),"--project",str(path),"--frames","3","--check-scripts"],env=env,capture_output=True,text=True,timeout=15)
            assert result.returncode==0,(result.stdout[-2000:],result.stderr[-2000:])
            files=list((base/"state").rglob("progress.json"));assert len(files)==1,files
            state=json.loads(files[0].read_text())
            assert state["values"]["visitas"]==visits and state["values"]["encerrado"] is True,state
            assert stat.S_IMODE(files[0].stat().st_mode)==0o600
            assert path.read_bytes()==before,"Player alterou a cena autoral"
        saved=files[0].read_bytes()
        result=subprocess.run([str(args.player.resolve()),"--validate",str(path)],env=env,capture_output=True,text=True,timeout=8)
        assert result.returncode==0,(result.stdout,result.stderr)
        assert files[0].read_bytes()==saved,"--validate alterou progresso privado"
        print("PASSOU: dois Players gráficos, progresso 1→2, Stop, permissão 0600, autoria e validação sem gravação")


if __name__=="__main__":main()
