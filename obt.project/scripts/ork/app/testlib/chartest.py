################################################################################
# CharacterComponent - skinned character model component
################################################################################

from orkengine import lev2
from ork.app.application import ApplicationComponent

################################################################################

class CharacterComponent(ApplicationComponent):

  def __init__(self, modelpath, bonescale=4.0):
    super().__init__()
    self.modelpath = modelpath
    self.bonescale = bonescale

  def _onGpuLink(self, ctx):
    SGC = self.app.SGC
    SG = SGC.scenegraph

    # Load model
    self.model = lev2.XgmModel(self.modelpath)
    self.skeleton = self.model.skeleton

    # Create drawable & scene node
    self.drawable_model = self.model.createDrawable()
    self.modelinst = self.drawable_model.modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.sgnode = SG.createDrawableNodeOnLayers(SGC.fwd_layers, "modelnode", self.drawable_model)

    # Pose setup
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose
    self.skeleton.visualBoneScale = self.bonescale

    # Print joint info
    infcounts = self.skeleton.jointVertexInfluenceCounts
    for i in range(len(infcounts)):
      infcount = infcounts[i]
      if infcount > 0:
        jname = self.skeleton.jointName(i)
        par = self.skeleton.jointParent(i)
        pname = self.skeleton.jointName(par)
        print("joint<%d:%s> par<%d:%s> infcount<%d>" % (i, jname, par, pname, infcount))

    # Initialize pose
    self.localpose.bindPose()
    self.localpose.blendPoses()
    self.localpose.concatenate()
