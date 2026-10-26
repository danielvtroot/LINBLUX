"""Exercise the real standalone Player with saved scenes and real Lua scripts."""
import argparse
import copy
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--player", type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    base = json.loads((root / "assets/defaults/factory/world.loblox.json").read_text())
    # Os exemplos animados precisam do rig R6 de fábrica (personagem e clipes).
    character_game = json.loads((root / "assets/defaults/factory/character.loblox.json").read_text())["game"]
    animated_cases = {"NpcPatrulhaAnimada.lua", "NpcMonstroAnimado.lua"}
    workspace = next(o["id"] for o in base["objects"] if o["class"] == "Workspace")
    example = (root / "assets/examples/lua/temporizadores.lua").read_text()
    example += '\nlinblux.events.on("stop", function() assert(linblux.scene.root:get_attribute("timer_concluido")) end)\n'
    cases = [
        ("InteracaoPorta.lua",(root / "assets/examples/lua/interacao_porta.lua").read_text(),0,"erros Lua: 0"),
        ("InteracaoInvalida.lua","local p=linblux.scene.create('part');linblux.interactions.register(p,{hold_seconds=-1})",1,"duração"),
        ("Inventario.lua",(root / "assets/examples/lua/inventario.lua").read_text(),0,"erros Lua: 0"),
        ("InventarioDuplicado.lua","local m=linblux.scene.create('model');linblux.scene.create('part',{parent=m});linblux.items.register('x',m);local a=linblux.items.spawn('x');linblux.items.collect(a);linblux.items.collect(a)",1,"já está no inventário"),
        ("InventarioTipoInvalido.lua","local m=linblux.scene.create('model');linblux.scene.create('part',{parent=m});linblux.items.register('../x',m)",1,"Identificador do tipo"),
        ("Juntas.lua",(root / "assets/examples/lua/juntas.lua").read_text(),0,"erros Lua: 0"),
        ("JuntaInvalida.lua","local p=linblux.scene.create('part');local a=linblux.scene.create('attachment',{parent=p});a:connect_to(a)",1,"duas peças distintas"),
        ("AttachmentPaiInvalido.lua", "linblux.scene.create('attachment')",1,"Não foi possível criar o objeto"),
        ("AttachmentLeitura.lua", "local p=linblux.scene.create('part');local a=linblux.scene.create('attachment',{parent=p});a.world_position=linblux.vec3(0,0,0)",1,"somente leitura"),
        ("Attachments.lua",(root / "assets/examples/lua/attachments.lua").read_text(),0,"erros Lua: 0"),
        ("ModelosLimite.lua","local m=linblux.scene.create('model');for i=1,33 do m.position=linblux.vec3(i,0,0) end",1,"Limite de 32 operações"),
        ("Modelos.lua",(root / "assets/examples/lua/modelos.lua").read_text()+'\nlinblux.events.on("stop",function() assert(linblux.scene.root:get_attribute("modelos_concluido")) end)',0,"erros Lua: 0"),
        ("SurfacePlayer.lua","local p=linblux.scene.find('SurfacePart');local c=p:components();assert(#c==1 and c[1].type=='decal');p:remove_component(c[1]);assert(#p:components()==0)",0,"erros Lua: 0"),
        ("CorposRigidos.lua",(root / "assets/examples/lua/corpos_rigidos.lua").read_text()+'\nlinblux.events.on("stop",function() assert(linblux.scene.root:get_attribute("corpos_rigidos_concluido")) end)',0,"erros Lua: 0"),
        ("CorpoInvalido.lua","linblux.scene.create('part',{body={density=-1}})",1,"Propriedades físicas fora dos limites"),
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
        ("NpcPatrulha.lua", (root / "assets/examples/lua/npc_patrulha.lua").read_text()+'\nlinblux.events.on("stop",function() assert(linblux.scene.root:get_attribute("patrulha_pronta")) end)', 0, "erros Lua: 0"),
        ("NpcPatrulhaAnimada.lua", (root / "assets/examples/lua/npc_patrulha_animado.lua").read_text()+'\nlinblux.events.on("stop",function() assert(linblux.scene.root:get_attribute("patrulha_animada_pronta")); assert(linblux.scene.root:get_attribute("patrulha_run")) end)', 0, "erros Lua: 0"),
        ("NpcMonstroAnimado.lua", (root / "assets/examples/lua/npc_monstro_animado.lua").read_text()+'\nlinblux.events.on("stop",function() assert(linblux.scene.root:get_attribute("monstro_rig") ~= nil) end)', 0, "erros Lua: 0"),
        ("NpcDano.lua", "local npc=linblux.npc.create{name='Alvo',position=linblux.vec3(10,2.6,10),max_health=10}\nlinblux.npc.damage(npc,15)\nassert(linblux.npc.state(npc)=='dead' and linblux.npc.health(npc)==0)", 0, "erros Lua: 0"),
        ("NpcInvalido.lua", "linblux.npc.create{size=linblux.vec3(-1,2,3)}", 1, "Tamanho mínimo do NPC"),
        ("NpcSemComponente.lua", "local p=linblux.scene.create('part',{name='SemNpc',position=linblux.vec3(5,5,5)})\nlinblux.npc.move_to(p, linblux.vec3(0,2,0))", 1, "não possui componente de NPC"),
        ("Audio.lua", "local h = linblux.audio.play('assets/tone.ogg',{volume=0.5,loop=true})\nassert(h and linblux.audio.is_playing(h))\nassert(linblux.audio.duration(h) > 0.1)\nlinblux.audio.pause(h)\nassert(linblux.audio.is_paused(h))\nlinblux.audio.resume(h)\nlinblux.audio.set_volume(h,0.25)\nassert(math.abs(linblux.audio.volume(h)-0.25) < 0.001)\nlinblux.audio.set_loop(h,false)\nassert(not linblux.audio.looping(h))\nlinblux.audio.stop(h)\nassert(not linblux.audio.is_playing(h))\nlinblux.scene.root:set_attribute('audio_ok', true)", 0, "erros Lua: 0"),
        ("AudioAusente.lua", "linblux.audio.play('assets/nao-existe.ogg')", 1, "não encontrado no projeto"),
        ("FalhaDeScript.lua", "error('probe validation failure')", 1, "FalhaDeScript.lua:1"),
        ("FalhaNoStop.lua", "linblux.events.on('stop', function() error('probe stop failure') end)", 1, "probe stop failure"),
        ("ErroEvicto.lua", "error('old error evicted by output')", 1, "erros Lua: 1"),
    ]
    with tempfile.TemporaryDirectory(prefix="linblux-lua-player-") as temporary:
        audio_dir = Path(temporary) / "assets"
        audio_dir.mkdir()
        shutil.copy(root / "tests/fixtures/audio/tone.ogg", audio_dir / "tone.ogg")
        for name, source, expected, diagnostic in cases:
            scene = copy.deepcopy(base)
            if name in animated_cases:
                scene["game"] = copy.deepcopy(character_game)
            identifier = scene["nextId"]
            scene["nextId"] += 1
            scene["objects"].append({"class": "Script", "id": identifier, "name": name,
                                     "parent": workspace, "source": source, "enabled": True})
            if name == "ErroEvicto.lua":
                scene["objects"].append({"class": "Script", "id": scene["nextId"], "name": "MuitasMensagens.lua",
                    "parent": workspace, "source": "for i=1,300 do print('buffer', i) end", "enabled": True})
                scene["nextId"] += 1
            if name == "SurfacePlayer.lua":
                parent=scene["nextId"];scene["nextId"]+=2
                scene["objects"].append({"class":"Part","id":parent,"name":"SurfacePart","parent":workspace,
                    "shape":"Block","position":[40,2,0],"rotation":[0,0,0],"size":[2,2,2],"color":[255,255,255,255],
                    "material":"SmoothPlastic","anchored":True,"canCollide":True})
                scene["objects"].append({"class":"Decal","id":parent+1,"name":"Adesivo","parent":parent,
                    "surface":{"image":"assets/builtin/character-face.png","face":0,"rotation":27,"transparency":0.3}})
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
