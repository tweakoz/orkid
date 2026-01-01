################################################################################
# CharacterComponent - skinned character model component with IK support
################################################################################

from orkengine.core import vec3, vec4, vec2, quat, mtx4
from orkengine import lev2
from ork.app.application import ApplicationComponent

################################################################################
# IK Chain definitions for mixamo rigs
################################################################################

ARM_IK_CHAINS = {
  "Left": {
    "arm": "mixamorig.LeftArm",
    "forearm": "mixamorig.LeftForeArm",
    "hand": "mixamorig.LeftHand",
  },
  "Right": {
    "arm": "mixamorig.RightArm",
    "forearm": "mixamorig.RightForeArm",
    "hand": "mixamorig.RightHand",
  }
}

LEG_IK_CHAINS = {
  "Left": {
    "upleg": "mixamorig.LeftUpLeg",
    "leg": "mixamorig.LeftLeg",
    "foot": "mixamorig.LeftFoot",
  },
  "Right": {
    "upleg": "mixamorig.RightUpLeg",
    "leg": "mixamorig.RightLeg",
    "foot": "mixamorig.RightFoot",
  }
}

################################################################################

class CharacterComponent(ApplicationComponent):

  def __init__(self, modelpath, bonescale=4.0, enable_ik=True):
    super().__init__()
    self.modelpath = modelpath
    self.bonescale = bonescale
    self.enable_ik = enable_ik

    # IK state variables
    self.ik_chain = None
    self.ik_target = None
    self.ik_upper_joint = -1   # arm or upleg
    self.ik_lower_joint = -1   # forearm or leg
    self.ik_end_joint = -1     # hand or foot
    self.ik_extend_length = 0.0
    self.ik_fixup_joints = []
    self.ik_initial_end_pos = None
    self.ik_initial_locals = None
    self.ik_initial_concats = None
    self.ik_mode = None  # "xz" or "xy"
    self.ik_plane_fixed = 0.0  # Y for xz plane, Z for xy plane
    self.ik_index_joints = []
    self.ik_limb_type = None  # "arm" or "leg"
    self.ik_limb_side = None  # "Left" or "Right"

    # FK state variables
    self.sel_joint = -1
    self.activate_rot = False
    self.descendants = []
    self.push_screen_pos = None
    self.pivot_point = None
    self.concats_at_push = None
    self.locals_at_push = None
    self.relmats = None

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

    # IK target visualization ball
    if self.enable_ik:
      self.ball_model = lev2.XgmModel("data://tests/pbr_calib")
      self.ball_drawable = self.ball_model.createDrawable()
      self.ball_node = SG.createDrawableNodeOnLayers(SGC.fwd_layers, "ik-ball-node", self.ball_drawable)
      self.ball_node.worldTransform.scale = 0.01
      self.ball_node.pickable = False

  ##############################################################################
  # Pose Control
  ##############################################################################

  def resetPose(self):
    """Reset to bind pose."""
    self.localpose.bindPose()
    self.localpose.blendPoses()
    self.localpose.concatenate()

  ##############################################################################
  # Bone Selection
  ##############################################################################

  def deselectBone(self):
    """Clear bone selection and IK state."""
    self.skeleton.selectBone(-1)
    self.sel_joint = -1
    self.ik_chain = None
    self.ik_mode = None

  def selectBoneForFK(self, bone_index):
    """
    Select a bone and setup FK rotation state.
    Returns (parent_index, child_index) or (None, None) if invalid.
    """
    if bone_index is None:
      return (None, None)

    self.skeleton.selectBone(bone_index)
    bone = self.skeleton.bone(bone_index)
    parent_index = bone.parentIndex
    child_index = bone.childIndex

    self.sel_joint = parent_index
    self.pivot_point = self.localpose.concatMatrices[parent_index].translation

    print(f"bone:{bone_index} parent:{parent_index} child:{child_index} pivot:{self.pivot_point}")

    # Setup FK rotation state
    self.descendants = self.skeleton.descendantJointsOf(parent_index)
    pmat = self.localpose.concatMatrices[parent_index]
    chcmats = [self.localpose.concatMatrices[i] for i in self.descendants]
    self.concats_at_push = self.localpose.concatMatrices[0:]
    self.locals_at_push = self.localpose.localMatrices[0:]
    self.relmats = [pmat.inverse * ch for ch in chcmats]
    self.activate_rot = False

    return (parent_index, child_index)

  ##############################################################################
  # IK Methods
  ##############################################################################

  def detectLimbFromJoint(self, joint_index):
    """
    Detect if a joint belongs to left or right arm or leg.
    Returns ("arm", "Left"), ("arm", "Right"), ("leg", "Left"), ("leg", "Right"), or (None, None).
    """
    jname = self.skeleton.jointName(joint_index)

    # Check for arm
    if "Left" in jname and ("Hand" in jname or "Arm" in jname):
      return ("arm", "Left")
    elif "Right" in jname and ("Hand" in jname or "Arm" in jname):
      return ("arm", "Right")

    # Check for leg
    if "Left" in jname and ("Foot" in jname or "Leg" in jname or "Toe" in jname):
      return ("leg", "Left")
    elif "Right" in jname and ("Foot" in jname or "Leg" in jname or "Toe" in jname):
      return ("leg", "Right")

    return (None, None)

  def setupIkChain(self, clicked_joint_index):
    """
    Setup IK chain for the arm or leg that was clicked.
    Uses hardcoded chains for mixamo rigs.
    """
    if not self.enable_ik:
      return

    # Detect which limb was clicked
    limb_type, limb_side = self.detectLimbFromJoint(clicked_joint_index)
    if limb_type is None:
      print(f"Joint {clicked_joint_index} is not part of an arm or leg, IK disabled")
      self.ik_chain = None
      return

    self.ik_limb_type = limb_type
    self.ik_limb_side = limb_side

    if limb_type == "arm":
      print(f"Setting up {limb_side} arm IK")
      chain_info = ARM_IK_CHAINS[limb_side]

      # Get joint indices
      self.ik_upper_joint = self.skeleton.jointIndex(chain_info["arm"])
      self.ik_lower_joint = self.skeleton.jointIndex(chain_info["forearm"])
      self.ik_end_joint = self.skeleton.jointIndex(chain_info["hand"])

      print(f"  Arm: {chain_info['arm']} ({self.ik_upper_joint})")
      print(f"  ForeArm: {chain_info['forearm']} ({self.ik_lower_joint})")
      print(f"  Hand: {chain_info['hand']} ({self.ik_end_joint})")

      # Create IK chain
      self.ik_chain = lev2.IkChain(self.skeleton)
      self.ik_chain.bindToJointNamed(chain_info["arm"])
      self.ik_chain.bindToJointNamed(chain_info["forearm"])
      self.ik_chain.prepare()
      self.ik_chain.compute(self.localpose, vec3(0,0,0))
      # Use hardcoded values for arm
      self.ik_chain.C1 = 0.079
      self.ik_chain.C2 = 0.029

      # Get index finger joints for hand rotation correction
      self.ik_index_joints = [self.skeleton.jointIndex(f"mixamorig.{limb_side}HandIndex{i+1}") for i in range(4)]
      print(f"  Index joints: {self.ik_index_joints}")

    else:  # leg
      print(f"Setting up {limb_side} leg IK")
      chain_info = LEG_IK_CHAINS[limb_side]

      # Get joint indices
      self.ik_upper_joint = self.skeleton.jointIndex(chain_info["upleg"])
      self.ik_lower_joint = self.skeleton.jointIndex(chain_info["leg"])
      self.ik_end_joint = self.skeleton.jointIndex(chain_info["foot"])

      print(f"  UpLeg: {chain_info['upleg']} ({self.ik_upper_joint})")
      print(f"  Leg: {chain_info['leg']} ({self.ik_lower_joint})")
      print(f"  Foot: {chain_info['foot']} ({self.ik_end_joint})")

      # Create IK chain
      self.ik_chain = lev2.IkChain(self.skeleton)
      self.ik_chain.bindToJointNamed(chain_info["upleg"])
      self.ik_chain.bindToJointNamed(chain_info["leg"])
      self.ik_chain.prepare()
      self.ik_chain.compute(self.localpose, vec3(0,0,0))

      # Get toe joints for foot rotation correction
      self.ik_index_joints = [self.skeleton.jointIndex(f"mixamorig.{limb_side}ToeBase")]
      print(f"  Toe joints: {self.ik_index_joints}")

    # Fixup joints: end effector + all descendants
    self.ik_fixup_joints = [self.ik_end_joint] + self.skeleton.descendantJointsOf(self.ik_end_joint)
    print(f"  Fixup joints: {len(self.ik_fixup_joints)}")

  def constrainLimb(self, concats):
    """
    Constrain limb rotation to anatomically plausible limits.
    Arms: prevent crossing through torso
    Legs: prevent crossing through other leg
    """
    curr_upper_mtx = concats[self.ik_upper_joint]
    curr_lower_mtx = concats[self.ik_lower_joint]

    # Compute current limb direction (upper to lower)
    curr_limb_dir = (curr_lower_mtx.translation - curr_upper_mtx.translation).normalized

    # Get pivot position
    pivot_pos = curr_upper_mtx.translation

    needs_correction = False
    clamped_dir = None

    if self.ik_limb_type == "arm":
      # Arms: prevent crossing through torso (X constraint)
      if self.ik_limb_side == "Left":
        # Left arm naturally points +X, stop it from going to -X
        if curr_limb_dir.x < 0:
          needs_correction = True
          clamped_dir = vec3(0, curr_limb_dir.y, curr_limb_dir.z).normalized
      else:  # Right
        # Right arm naturally points -X, stop it from going to +X
        if curr_limb_dir.x > 0:
          needs_correction = True
          clamped_dir = vec3(0, curr_limb_dir.y, curr_limb_dir.z).normalized

    else:  # leg
      # Legs: prevent crossing through other leg (X constraint, opposite of arms)
      if self.ik_limb_side == "Left":
        # Left leg, stop it from going too far to +X (crossing right)
        if curr_limb_dir.x > 0.3:
          needs_correction = True
          clamped_dir = vec3(0.3, curr_limb_dir.y, curr_limb_dir.z).normalized
      else:  # Right
        # Right leg, stop it from going too far to -X (crossing left)
        if curr_limb_dir.x < -0.3:
          needs_correction = True
          clamped_dir = vec3(-0.3, curr_limb_dir.y, curr_limb_dir.z).normalized

    if needs_correction and clamped_dir is not None and clamped_dir.length > 0.001:
      # Compute rotation to go from current to clamped direction
      correction_axis = curr_limb_dir.cross(clamped_dir)
      if correction_axis.length > 0.001:
        correction_axis = correction_axis.normalized
        correction_angle = curr_limb_dir.angle(clamped_dir)

        # Build correction matrix around pivot
        Qc = quat()
        Qc.fromAxisAngle(vec4(correction_axis, correction_angle))
        Mc = Qc.toMatrix()

        # Apply correction around pivot
        IP = mtx4.transMatrix(pivot_pos * -1.0)
        P = mtx4.transMatrix(pivot_pos)
        correction = P * Mc * IP

        # Apply to upper joint and all descendants
        limb_descendants = [self.ik_upper_joint] + self.skeleton.descendantJointsOf(self.ik_upper_joint)
        for ji in limb_descendants:
          concats[ji] = correction * concats[ji]

  def updateIk(self, cur_screen_pos, camdat):
    """
    Update IK - target follows ray intersection with fixed plane.
    """
    if self.ik_chain is None:
      return

    # Reset pose to initial state
    for i in range(self.skeleton.numJoints):
      self.localpose.localMatrices[i] = self.ik_initial_locals[i]
      self.localpose.concatMatrices[i] = self.ik_initial_concats[i]

    concats = self.localpose.concatMatrices

    # Get lower and end joint matrices from reset pose
    mtx_lower = concats[self.ik_lower_joint]
    mtx_end = concats[self.ik_end_joint]

    # Compute extend length (distance from lower joint to end effector)
    extend_length = (mtx_lower.translation - mtx_end.translation).length

    # Project ray through mouse position
    SGC = self.app.SGC
    sgvpw = SGC.SGVPW
    vp_width = sgvpw.width
    vp_height = sgvpw.height

    norm_x = cur_screen_pos.x / vp_width
    norm_y = 1.0 - (cur_screen_pos.y / vp_height)

    aspect = vp_width / vp_height
    cam_matrices = camdat.computeMatrices(aspect)
    ray = cam_matrices.projectDepthRay(vec2(norm_x, norm_y))

    # Intersect ray with plane based on mode
    eye_pos = camdat.eye
    target = self.ik_initial_end_pos  # fallback

    if self.ik_mode == "xz":
      # XZ plane at fixed Y
      if abs(ray.direction.y) > 0.001:
        t = (self.ik_plane_fixed - eye_pos.y) / ray.direction.y
        if t > 0:
          target = eye_pos + ray.direction * t
    elif self.ik_mode == "xy":
      # XY plane at fixed Z
      if abs(ray.direction.z) > 0.001:
        t = (self.ik_plane_fixed - eye_pos.z) / ray.direction.z
        if t > 0:
          target = eye_pos + ray.direction * t

    # Update ball visualization
    if self.enable_ik and hasattr(self, 'ball_node'):
      self.ball_node.worldTransform.translation = target

    # Solve IK
    self.ik_chain.compute(self.localpose, target)

    # Constrain limb rotation
    self.constrainLimb(concats)

    # Fixup: reconnect end effector to lower joint
    fixup_base = concats[self.ik_lower_joint]
    fixup_old = concats[self.ik_end_joint].translation
    fixup_new = vec3(0, extend_length, 0).transform(fixup_base)
    fixup_delta = fixup_new - fixup_old

    xf_delta = mtx4()
    xf_delta.setColumn(3, vec4(fixup_delta, 1))

    for ji in self.ik_fixup_joints:
      O = concats[ji]
      concats[ji] = xf_delta * O

    # Correct rotation of end effector to point along limb direction
    if len(self.ik_index_joints) > 0:
      dir_lower_to_end = (concats[self.ik_end_joint].translation
                          - concats[self.ik_lower_joint].translation).normalized
      dir_end_to_index = (concats[self.ik_index_joints[0]].translation
                          - concats[self.ik_end_joint].translation).normalized
      dir_cross = dir_lower_to_end.cross(dir_end_to_index)
      if dir_cross.length > 0.001:
        dir_cross = dir_cross.normalized
        angle = dir_lower_to_end.angle(dir_end_to_index)

        Q = quat()
        Q.fromAxisAngle(vec4(dir_cross, -angle))
        MQ = Q.toMatrix()

        # Apply rotation around end effector position
        h = concats[self.ik_end_joint]
        a = mtx4()
        a.setColumn(3, h.getColumn(3))
        ai = a.inverse
        MQ = a * MQ * ai

        for ji in self.ik_fixup_joints:
          O = concats[ji]
          concats[ji] = MQ * O

  ##############################################################################
  # FK Rotation Methods
  ##############################################################################

  def rotateOnScreenZ(self, cur_screen_pos, camdat):
    mag = (cur_screen_pos - self.push_screen_pos).length
    if self.activate_rot == False:
      if mag > 32:
        self.activate_rot = True
        self.activated_pos = cur_screen_pos
    if self.activate_rot:
      deltaA = (self.activated_pos - self.push_screen_pos).normalized
      deltaB = (cur_screen_pos - self.push_screen_pos).normalized
      angle = deltaB.orientedAngle(deltaA)
      self.localpose.concatenate()
      X = self.concats_at_push[self.sel_joint]
      ZN = camdat.znormal
      IP = mtx4.transMatrix(self.pivot_point * -1.0)
      P = mtx4.transMatrix(self.pivot_point)
      Q = quat.createFromAxisAngle(ZN, angle)
      R = Q.toMatrix()
      M = P * R * IP
      self.propogateFromJoint(X, M)

  def rotateOnLocalX(self, cur_screen_pos):
    delta = (cur_screen_pos.x - self.push_screen_pos.x)
    angle = delta * 0.01
    self.localpose.concatenate()
    C = self.concats_at_push[self.sel_joint]
    R = quat.createFromAxisAngle(vec3(1, 0, 0), angle).toMatrix()
    IP = mtx4.transMatrix(self.pivot_point * -1.0)
    P = mtx4.transMatrix(self.pivot_point)
    M = P * R * IP
    self.propogateFromJoint(C, M)

  def rotateOnLocalY(self, cur_screen_pos):
    delta = (cur_screen_pos.x - self.push_screen_pos.x)
    angle = delta * 0.01
    self.localpose.concatenate()
    C = self.concats_at_push[self.sel_joint]
    R = quat.createFromAxisAngle(vec3(0, 1, 0), angle).toMatrix()
    IP = mtx4.transMatrix(self.pivot_point * -1.0)
    P = mtx4.transMatrix(self.pivot_point)
    M = P * R * IP
    self.propogateFromJoint(C, M)

  def rotateOnLocalZ(self, cur_screen_pos):
    delta = (cur_screen_pos.x - self.push_screen_pos.x)
    angle = delta * 0.01
    self.localpose.concatenate()
    C = self.concats_at_push[self.sel_joint]
    R = quat.createFromAxisAngle(vec3(0, 0, 1), angle).toMatrix()
    IP = mtx4.transMatrix(self.pivot_point * -1.0)
    P = mtx4.transMatrix(self.pivot_point)
    M = P * R * IP
    self.propogateFromJoint(C, M)

  def propogateFromJoint(self, C, M):
    self.localpose.concatMatrices[self.sel_joint] = M * C
    for i in range(len(self.descendants)):
      ich = self.descendants[i]
      MCH = self.relmats[i]
      self.localpose.concatMatrices[ich] = M * C * MCH
    self.localpose.deconcatenate()
    self.localpose.concatenate()
