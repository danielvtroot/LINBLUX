"""Exercise the real standalone Player with saved scenes and real Lua scripts."""
import argparse
import copy
import json
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--player", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    base = json.loads((root / "assets/defaults/factory/world.loblox.json").read_text())
    workspace = next(o["id"] for o in base["objects"] if o["class"] == "Workspace")
    example = (root / "assets/examples/lua/temporizadores.lua").read_text()
    example += '\nlinblux.events.on("stop", function() assert(linblux.scene.root:get_attribute("timer_concluido")) end)\n'
    cases = [
        ("ConsultasControle.lua",(root / "assets/examples/lua/consultas_controle.lua").read_text()+'\nlinblux.events.on("stop",function() assert(linblux.scene.root:get_attribute("consultas_controle_concluido")) end)',0,"erros Lua: 0"),
        ("ConsultaLimite.lua","for i=1,9 do linblux.physics.overlap_sphere(linblux.vec3(0,10,0),1) end",1,"Limite de consultas espaciais"),
        ("MovimentoInvalido.lua","linblux.player:move(linblux.vec3(0,1,0))",1,"direção horizontal"),
        ("Persistencia.lua", (root / "assets/examples/lua/persistencia.lua").read_text()+'\nassert(not linblux.storage.status().persistent)',0,"erros Lua: 0"),
        ("PersistenciaFuncao.lua","linblux.storage.set('bad',function() end)",1,"somente dados JSON"),
        ("PersistenciaCiclo.lua","local t={};t.loop=t;linblux.storage.set('bad',t)",1,"profundidade"),
        ("PersistenciaMista.lua","linblux.storage.set('bad',{[1]=1,a=2})",1,"Não misture"),
        ("PersistenciaMeta.lua","linblux.storage.set('bad',setmetatable({},{__index={}}))",1,"sem metatabela"),
        ("PersistenciaLimite.lua","for i=1,65 do linblux.storage.set('key',i) end",1,"Limite de alterações"),
        ("PersistenciaLeituras.lua","for i=1,129 do linblux.storage.get('key') end",1,"Limite de leituras"),
        ("Transformacoes.lua", (root / "assets/examples/lua/transformacoes.lua").read_text()+'\nlinblux.events.on("stop",function() assert(linblux.scene.root:get_attribute("transformacoes_concluidas")) end)',0,"erros Lua: 0"),
        ("Camera.lua", (root / "assets/examples/lua/camera.lua").read_text()+'\nlinblux.events.on("stop",function() assert(linblux.scene.root:get_attribute("camera_concluida")) end)',0,"erros Lua: 0"),
        ("CameraInvalida.lua","linblux.camera.set_pose({position=linblux.vec3(1,2,3),target=linblux.vec3(1,2,3)})",1,"Pose de câmera inválida"),
        ("Temporizadores.lua", example, 0, "erros Lua: 0"),
        ("FalhaDeScript.lua", "error('probe validation failure')", 1, "FalhaDeScript.lua:1"),
        ("FalhaNoStop.lua", "linblux.events.on('stop', function() error('probe stop failure') end)", 1, "probe stop failure"),
        ("ErroEvicto.lua", "error('old error evicted by output')", 1, "erros Lua: 1"),
    ]
    with tempfile.TemporaryDirectory(prefix="linblux-lua-player-") as temporary:
        for name, source, expected, diagnostic in cases:
            scene = copy.deepcopy(base)
            identifier = scene["nextId"]
            scene["nextId"] += 1
            scene["objects"].append({"class": "Script", "id": identifier, "name": name,
                                     "parent": workspace, "source": source, "enabled": True})
            if name == "ErroEvicto.lua":
                scene["objects"].append({"class": "Script", "id": scene["nextId"], "name": "MuitasMensagens.lua",
                    "parent": workspace, "source": "for i=1,300 do print('buffer', i) end", "enabled": True})
                scene["nextId"] += 1
            path = Path(temporary) / (name + ".json")
            path.write_text(json.dumps(scene, ensure_ascii=False))
            before = path.read_bytes()
            result = subprocess.run([str(args.player.resolve()), "--validate", str(path)],
                                    capture_output=True, text=True, timeout=8)
            assert result.returncode == expected, (name, result.returncode, result.stdout, result.stderr)
            assert diagnostic in result.stdout + result.stderr, (name, result.stdout, result.stderr)
            assert path.read_bytes() == before, "Player alterou a cena salva"
            print(f"PASSOU: {name}, saída {expected}, diagnóstico e cena preservada")


if __name__ == "__main__":
    main()
