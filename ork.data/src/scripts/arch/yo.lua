require("std/orklib")
require("std/inspect")

function _onLink(e)
	--scene = ork.getscene()
	printf( "Yo::OnEntityLink() ent<%s>", e )
		--local a = e:archetype()
	--return true
end

function _onStart(e)
	printf( "Yo::OnEntityStart() ent<%s>", e )
	--e.rx = 0
	--e.rz = 0
	--e.t = 0.0
	--e.rt = math.random(100)*0.01
	--e.i = 0
end

function _onStop(e)
	printf( "Yo::OnEntityStop() ent<%s>", e )
	--entity_exec_table[e:name()]=nil
	--printf( "Yo::OnEntityStop(%s)", tostring(e))
end

function _onUpdate(e,dt)
	printf( "Yo::OnEntityUpdate() ent<%s>", e )
	local p = e.pos
	printf("p<%s>",p)
	--p.x = p.x+e.rx
	--p.z = p.z+e.rz
	--e.pos = p
	--[[
	e.t = e.t + dt
	if e.t>e.rt then
		local is_odd = (e.i%2)==1
		if is_odd then
			e.rx = (math.random(100)-50)*0.01
			e.rz = (math.random(100)-50)*0.01
		else
			local mx = 0.9+(math.random(100)*0.001)
			local mz = 0.9+(math.random(100)*0.001)
			e.rx = -p.x*0.01*mx
			e.rz = -p.z*0.01*mz
		end
		e.t = 0.0
		e.i = e.i+1
	end
	]]--
end

return {
	OnEntityLink=_onLink,
	OnEntityStart=_onStart,
	OnEntityStop=_onStop,
	OnEntityUpdate=_onUpdate,
}
