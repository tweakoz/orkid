require("string")
require("ansicolors")

print(ansicolors.reset)

enttab = {}

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

