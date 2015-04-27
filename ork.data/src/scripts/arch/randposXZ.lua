require("std/orklib")
require("std/inspect")

function OnEntityLink(e)
	--printf("OEL")
	--ent = e
	--scene = ork.getscene()
	local a = e:archetype()
	--printf( "Yo::OnEntityLink() arch<%s>", a:name() )
end

function OnEntityStart(e)
	--printf( "Yo::OnEntityStart()" )
	--printf( "    ent(%s)", tostring(e))
end

function OnEntityStop(e)
	--entity_exec_table[e:name()]=nil
	--printf( "Yo::OnEntityStop(%s)", tostring(e))
end

function OnEntityUpdate(e)
	--printf( "	yo::OnScriptUpdate()" )
	--printf( "	e:name(%s)", e:name() )

	local a = e:archetype()
	local p = e.pos
	--printf( "	yo::OnScriptUpdate(%s:%s)", e:name(), tostring(a) )
	--printf( "	pos: %s", tostring(p) )
	--printf( "	xz: %s", p.xz )

	p.x = (math.random(100)-50)*0.1
	p.z = (math.random(100)-50)*0.1

	e.pos = p
end

--printf("inspection of Yo::arch<%s>",inspect(arch))