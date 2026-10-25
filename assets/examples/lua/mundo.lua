-- Script global: crie no sistema de scripts do mundo, não no importador de cenas .lua.
local grupo = linblux.scene.create("group", {name="Objetos da experiência"})
local sinal = linblux.scene.create("part", {
    name="Sinal", parent=grupo, position=linblux.vec3(18, 5, 0),
    size=linblux.vec3(3, 3, 1), color=linblux.vec3(220, 40, 60), material="emissive"
})
linblux.events.on("start", function()
    print("Experiência inicializada", sinal.id)
end)
linblux.time.after(0.1, function()
    linblux.events.emit("porta.alterar", true)
end)
linblux.events.on("stop", function()
    print("Experiência encerrada")
end)
