require("std/orklib")
inspect = require("std/inspect")
-------------------------------------------------------------------------------
local Spinny = {}
local scene = ork.scene()
-------------------------------------------------------------------------------
function Spinny:OnEntityLink()
    printf( "SPINNY::OnEntityLink()::begin" )
    self.timer = 1.0
    local c = self.ent.components
    self.angle = 0.0
    self.axis = ork.vec3(0,1,0)
    printf( "SPINNY::OnEntityLink()::end" )
end
-------------------------------------------------------------------------------
function Spinny:OnEntityStart()
  printf( "SPINNY::OnEntityStart()" )
end
-------------------------------------------------------------------------------
function Spinny:OnEntityStop()
  printf( "SPINNY::OnEntityStop()" )
end
-------------------------------------------------------------------------------
function Spinny:OnEntityActivate()
    printf( "SPINNY::OnEntityActivate()" )
    self.timer = 1.0
end
-------------------------------------------------------------------------------
function Spinny:OnEntityDeactivate()
end
-------------------------------------------------------------------------------
function Spinny:OnEntityUpdate(dt)
  --printf( "SPINNY::OnEntityUpdate()::begin" )
    self.timer = self.timer-dt
    if self.timer<0 then
        self.timer = math.random(1,3)
    end

    self.angle = self.angle + 0.1*dt
    self.ent:setRotAxisAngle(self.axis,self.angle)
end
-------------------------------------------------------------------------------
return Spinny
