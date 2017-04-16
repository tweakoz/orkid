require("std/orklib")
inspect = require("std/inspect")
-------------------------------------------------------------------------------
local Diver = {}
-------------------------------------------------------------------------------
function Diver:OnEntityLink()
    printf( "DIVER::OnEntityLink()" )
    self.timer = 1.0
    local c = self.ent.components
    self.charcon = c["SimpleCharController"]
    printf( "charcon: %s", tostring(self.charcon) );
end
-------------------------------------------------------------------------------
function Diver:OnEntityStart()
    printf( "DIVER::OnEntityStart()" )
    self.timer = 1.0
    self.charcon:sendEvent("state",{
        id = "idle"
    })
    self.statetable = {
        [1] = "idle",
        [2] = "walk",
        [3] = "run",
        [4] = "attack1",
        [5] = "attack2",
        [6] = "attack3",
    }
end
-------------------------------------------------------------------------------
function Diver:OnEntityStop()
    --entity_exec_table[e:name()]=nil
    --printf( "Yo::OnEntityStop(%s)", tostring(e))
end
-------------------------------------------------------------------------------
function Diver:OnEntityUpdate(dt)
    --printf( "DIVER::OnEntityUpdate()" )
    --printf( "entname<%s> dt<%g>",self.ent.name,dt )
    --printf( "ent<%s> pos<%s>",tostring(self.ent),tostring(self.ent.pos) )
    self.timer = self.timer-dt 
    if self.timer<0 then
        self.timer = math.random(1,3)
        local stnum = math.random(1,6)
        local statename = self.statetable[stnum]
        if statename ~= nil then
            self.charcon:sendEvent("state",{
                id = statename
            })
        end

        dir = math.random(-180,180)
        self.charcon:sendEvent("setDir",dir*math.pi/180)

    end

end
-------------------------------------------------------------------------------
return Diver