-- Script global. Referências espaciais explícitas; a hierarquia antiga não vira um vínculo físico.
local v = linblux.vec3
local group=linblux.scene.create("group",{name="Demonstração de transformações"})
local frame = linblux.scene.create("part", {name="Referencial",position=v(40,10,0),rotation=v(0,90,0),size=v(1,1,1),collidable=false})
local child = linblux.scene.create("part", {name="Peça programável",parent=group,size=v(1,1,1)})
child:set_transform({position=v(2,5,0),rotation=v(0,0,0)},frame)
assert(math.abs(child.position.x-40)<0.001 and math.abs(child.position.z+2)<0.001)
local relative=child:get_transform(frame)
assert(math.abs(relative.position.x-2)<0.001)
local point=child:point_to_world(v(1,0,0))
assert(math.abs(child:point_to_local(point).x-1)<0.001)
assert(math.abs(child:vector_to_local(child:vector_to_world(v(0,1,0))).y-1)<0.001)
child:set_transform({position=v(3,5,0)},frame)
local before=child.position
frame.position=v(41,10,0)
assert(child.position.x==before.x and child.position.z==before.z)
child:scale_by(v(2,3,4))
assert(child.size.x==2 and child.size.y==3 and child.size.z==4)
local middle=linblux.transform.interpolate(
    {position=child.position,rotation=v(0,170,0),size=child.size},
    {position=child.position,rotation=v(0,-170,0),size=child.size},0.5)
child.transform=middle
assert(child:vector_to_world(v(1,0,0)).x < -0.99)
local hit=linblux.physics.raycast(v(child.position.x,30,child.position.z),v(0,-1,0),20)
assert(hit and hit.object==child and math.abs(hit.point.y-16.5)<0.01)
local component=child:add_script("linblux.self:set_attribute('component_ran',true)","Componente gerado.lua")
assert(#child:components()==1 and component.enabled)
linblux.time.after(0.05,function()
    assert(child:get_attribute("component_ran"))
    child:remove_component(component)
    assert(not component.alive and #child:components()==0)
    linblux.scene.root:set_attribute("transformacoes_concluidas",true)
end)
