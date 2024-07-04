

print( "imported system_update.py" )

print("x %g"%math.sin(0))
print("ECS<%s>"%dir(ECS))
assert(False)

def onSystemInit():
  print("onSystemInit")
def onSystemLink():
  print("onSystemLink")
def onSystemActivate():
  print("onSystemActivate")
def onSystemStage():
  print("onSystemStage")
  
def onSystemUpdate():
  print("onSystemUpdate")
