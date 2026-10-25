-- Script global: três pulsos periódicos, sem recursão ou tarefas ilimitadas.
local root = linblux.scene.root
root:set_attribute("timer_total", 0)
local interval
interval = linblux.time.every(0.02, function(incremento)
    local total = root:get_attribute("timer_total") + incremento
    root:set_attribute("timer_total", total)
    if total >= 6 then interval:cancel() end
end, 2)
local cancelado = linblux.time.after(0.01, function()
    error("Esta tarefa cancelada não deve executar")
end)
linblux.time.cancel(cancelado)
linblux.time.after(0.15, function()
    assert(root:get_attribute("timer_total") == 6 and not interval.active)
    root:set_attribute("timer_concluido", true)
end)
