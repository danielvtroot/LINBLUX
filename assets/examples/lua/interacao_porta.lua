-- Interação universal: o mesmo sistema usado pela coleta, sem lógica de porta no C++.
local v = linblux.vec3
local porta = linblux.scene.create("part", {name="Porta interativa",position=v(8,3,-5),size=v(3,6,0.5)})
linblux.interactions.register(porta, {label="Abrir porta",distance=6,hold_seconds=0.6,key="e"})
local aberta = false
linblux.events.on("interaction_completed",function(evento)
    if evento.object and evento.object.id==porta.id then
        aberta = not aberta
        porta.rotation = v(0,aberta and 90 or 0,0)
    end
end)
