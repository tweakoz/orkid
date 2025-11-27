from orkengine.core import vec3 
from orkengine import lev2 

def createGridData(extent=10.0,majordim=1,minordim=0.1):
  grid_data = lev2.GridDrawableData()
  grid_data.shader_suffix = "_V4"
  grid_data.modcolor = vec3(.7)
  grid_data.intensityA = 1.0*0.5
  grid_data.intensityB = 0.97*0.5
  grid_data.intensityC = 0
  grid_data.intensityD = 0
  grid_data.lineWidth = 0.025
  grid_data.extent =  extent
  grid_data.majorTileDim = majordim
  grid_data.minorTileDim = minordim
  return grid_data