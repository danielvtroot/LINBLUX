local v=linblux.vec3
local part=linblux.scene.create("part",{name="Corpo programável",position=v(80,20,0),size=v(2,2,2),anchored=false,
    body={density=2,friction=0.4,restitution=0.6,linear_damping=0,angular_damping=0}})
assert(math.abs(part.mass-16)<0.001)
part:apply_impulse(v(16,0,0))
assert(math.abs(part.velocity.x-1)<0.001)
part.body={density=4} -- preserve the other properties, update the same Bullet body
assert(math.abs(part.mass-32)<0.001 and math.abs(part.body.friction-0.4)<0.001)
assert(math.abs(part.velocity.x-1)<0.001) -- changing density preserves velocity
part:apply_impulse(v(32,0,0))
assert(math.abs(part.velocity.x-2)<0.001)
part.angular_velocity=v(0,0,0)
part:apply_angular_impulse(v(0,32,0))
assert(part.angular_velocity.y>0)
part:apply_torque(v(0,1,0))
part.body={linear_damping=1,angular_damping=1}
linblux.time.after(0.15,function()
    assert(math.abs(part.velocity.x)<0.01 and math.abs(part.angular_velocity.y)<0.01)
    part.anchored=true
    assert(part.mass==0)
    part.anchored=false
    assert(math.abs(part.mass-32)<0.001)
    linblux.scene.root:set_attribute("corpos_rigidos_concluido",true)
end)
