require("string")
require("std/ansicolors")

print(ansicolors.reset)

-----------------------------------------------------------

function printf(...)
	print(ansicolors.white.."LUA: "..string.format(...)..ansicolors.reset)
end

-----------------------------------------------------------

function keys(tab)
	local keyset={}
	local n=0

	for k,v in pairs(tab) do
	  n=n+1
	  keyset[n]=k
	end
	return keyset
end

-----------------------------------------------------------

function UpdateSceneEntities(dt)

	--printf( "///////////////////////////")
	--printf( "OnSceneUpdate")
	--printf( "///////////////////////////")

	for n,exec_item in pairs(entity_exec_table) do
		--printf( "osu ent<%s>", exec_item.ent:name())
		--printf( "osu fn<%s>", tostring(exec_item.fn))		
		exec_item.fn(exec_item.ent,dt)
	end

 end
