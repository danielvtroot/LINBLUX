-- Junta física genérica. Sem lógica de arma fixa na engine.
local v = linblux.vec3
local m = linblux.scene.create("model", {name="Junta demonstrativa"})
local base = linblux.scene.create("part", {name="Suporte",parent=m,position=v(0,10,0),anchored=true})
local movel = linblux.scene.create("part", {name="Braço móvel",parent=m,position=v(4,10,0),size=v(2,1,1),anchored=false,collidable=false})
local a = linblux.scene.create("attachment", {name="Origem",parent=base})
local b = linblux.scene.create("attachment", {name="Encaixe",parent=movel})
a:connect_to(b) -- ajusta só a âncora de destino, preservando as Parts
assert(a.joint.id == b.id)
local tempo = 0
linblux.events.on("update", function(dt)
    tempo = tempo + dt
    a.rotation = v(0, math.sin(tempo)*30, 0)
end)
linblux.events.on("stop", function()
    assert(movel.position.y > 9) -- sustentado pelo vínculo, contra a gravidade
    assert(a.joint.id == b.id)
    local copia = m:clone()
    assert(copia.id ~= m.id)
    b:disconnect() -- funciona também pela âncora de destino
    assert(a.joint == nil)
end)
linblux.scene.root:set_attribute("juntas_prontas",true)
