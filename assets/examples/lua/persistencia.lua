-- Estado privado por instalação. --validate e cenas não salvas usam memória.
local storage=linblux.storage
local visits=storage.get("visitas",0)+1
storage.set("visitas",visits)
storage.set("configuracao",{dificuldade="normal",volumes={0.2,0.5},spawn=linblux.vec3(1,2,3)})
local config=storage.get("configuracao")
assert(config.dificuldade=="normal" and config.volumes[2]==0.5 and config.spawn.z==3)
storage.set("temporario",true)
assert(storage.remove("temporario") and storage.get("temporario")==nil)
linblux.scene.root:set_attribute("visitas_persistidas",visits)
linblux.events.on("stop",function() storage.set("encerrado",true) end)
