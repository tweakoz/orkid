require("std/orklib")
inspect = require("std/inspect")
-------------------------------------------------------------------------------
local RandPosXYZ = {}
local scene = ork.scene()
-------------------------------------------------------------------------------
function RandPosXYZ:OnEntityLink()
    printf( "RANDPOSZYX::OnEntityLink()::begin" )
    self.timer = 1.0
    local c = self.ent.components
    self.angle = 0.0
    self.pos = ork.vec3(0,0,0)
    self.targetpos = ork.vec3(0,0,0)
    printf( "RANDPOSZYX::OnEntityLink()::end" )
end
-------------------------------------------------------------------------------
function RandPosXYZ:OnEntityStart()
  printf( "RANDPOSZYX::OnEntityStart()" )
end
-------------------------------------------------------------------------------
function RandPosXYZ:OnEntityStop()
  printf( "RANDPOSZYX::OnEntityStop()" )
end
-------------------------------------------------------------------------------
function RandPosXYZ:OnEntityActivate()
    printf( "RANDPOSZYX::OnEntityActivate()" )
    self.timer = 1.0
end
-------------------------------------------------------------------------------
function RandPosXYZ:OnEntityDeactivate()
end
-------------------------------------------------------------------------------
function RandPosXYZ:OnEntityUpdate(dt)
  printf( "RANDPOSZYX::OnEntityUpdate()::begin" )
    self.timer = self.timer-dt
    if self.timer<0 then
        self.timer = math.random(1,3)
			  x = (math.random(1000)-500)*0.01
			  y = (math.random(1000)-500)*0.01
			  z = (math.random(1000)-500)*0.01
        self.targetpos = ork.vec3(x,y,z)
    end
    --local pos = self.ent:pos()
    local delta = (self.targetpos-self.pos)
    printf( "delta<%s>\n", tostring(delta) )
    self.pos = self.pos+delta*(0.5*dt);
    self.ent:setPos(self.pos)
  printf( "RANDPOSZYX::OnEntityUpdate()::end" )
end
-------------------------------------------------------------------------------
return RandPosXYZ
