require("std/orklib")
require("std/inspect")

function OnEntityLink(e)
	--scene = ork.getscene()
	--local a = e:archetype()
end

function OnEntityStart(e)
	--printf( "Yo::OnEntityStart()" )
	e.ry = 0
	e.rz = 0
	e.t = 0.0
	e.rt = math.random(100)*0.01
	e.i = 0
end

function OnEntityStop(e)
	--entity_exec_table[e:name()]=nil
	--printf( "Yo::OnEntityStop(%s)", tostring(e))
end

function OnEntityUpdate(e,dt)
	local p = e.pos
	p.y = p.y+e.ry
	p.z = p.z+e.rz
	e.pos = p
	e.t = e.t + dt
	if e.t>e.rt then
		local is_odd = (e.i%2)==1
		if is_odd then
			e.ry = (math.random(100)-50)*0.01
			e.rz = (math.random(100)-50)*0.01
		else
			local my = 0.9+(math.random(100)*0.001)
			local mz = 0.9+(math.random(100)*0.001)
			e.ry = -p.y*0.01*my
			e.rz = -p.z*0.01*mz
		end
		e.t = 0.0
		e.i = e.i+1
	end
end
