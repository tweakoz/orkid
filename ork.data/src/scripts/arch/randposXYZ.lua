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
    self.pos = self.ent:pos()
    self.targetpos = ork.vec3(0,0,0)
    self.targetaxis = ork.vec3(0,0,0)
    self.angle = 0.0
    self.axis = ork.vec3(0,1,0)
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
  --printf( "RANDPOSZYX::OnEntityUpdate()::begin" )
    self.timer = self.timer-dt
    if self.timer<0 then
        self.timer = math.random(1,10)
			  x = (math.random(1000))*0.01
			  y = (math.random(1000))*0.01
			  z = (math.random(1000))*0.01
        self.targetpos = ork.vec3(x,y,z)
        x = (math.random(1000)-500)*0.01
			  y = (math.random(1000)-500)*0.01
			  z = (math.random(1000)-500)*0.01
        self.targetaxis = ork.vec3(x,y,z):normal()
        --self.angle = 0.0
    end
    local delta = (self.targetpos-self.pos):normal()
    self.pos = self.pos+delta*(0.5*dt);
    self.ent:setPos(self.pos)
    self.angle = self.angle + 0.5*dt
    delta = (self.targetaxis-self.axis):normal()
    self.axis = (self.axis+delta*(0.5*dt)):normal()
    self.ent:setRotAxisAngle(self.axis,self.angle)
  --printf( "RANDPOSZYX::OnEntityUpdate()::end" )
end
-------------------------------------------------------------------------------
return RandPosXYZ
