-- Protótipo oculto, instância real e identidade preservada na sessão.
local v = linblux.vec3
local modelo = linblux.scene.create("model", {name="Protótipo de chave"})
linblux.scene.create("part", {name="Chave",parent=modelo,position=v(4,3,0),size=v(1,1,2),color=v(220,40,60)})
modelo:add_script("linblux.self:set_attribute('comportamento_ativo',true)", "Item.lua")
linblux.items.register("chave", modelo, {name="Chave vermelha"})
local chave = linblux.items.spawn("chave", {position=v(4,3,0)})
chave:set_attribute("carga",37)
local id = chave.id
local mudancas = 0
linblux.events.on("inventory_changed",function(evento)
    assert(evento.id and evento.action)
    mudancas = mudancas + 1
end)
linblux.items.collect(chave)
assert(#linblux.items.list()==1)
linblux.items.equip(chave)
assert(linblux.items.equipped().id==id)
local tempo, terminou = 0, false
linblux.events.on("update",function(dt)
    tempo = tempo + dt
    if tempo > 0.08 and not terminou then
        assert(chave:get_attribute("comportamento_ativo"))
        linblux.items.drop(chave)
        assert(#linblux.items.list()==0 and linblux.items.state(chave)=="world")
        assert(chave.id==id and chave:get_attribute("carga")==37)
        linblux.items.collect(chave)
        linblux.items.equip(chave)
        linblux.items.unequip()
        assert(linblux.items.info(chave).name=="Chave vermelha")
        terminou = true
    end
end)
linblux.events.on("stop",function()
    assert(terminou and mudancas>0 and #linblux.items.list()==1)
end)
