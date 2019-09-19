#pragma once

namespace ork::lev2 {

  enum RenderGroupState
  {
    ERGST_NONE = 0,
    ERGST_FIRST,
    ERGST_CONTINUE,
    ERGST_LAST,
  };


    enum EOutputTimeStep {
      EOutputTimeStep_RealTime = 0,
      EOutputTimeStep_15fps,
      EOutputTimeStep_24fps,
      EOutputTimeStep_30fps,
      EOutputTimeStep_48fps,
      EOutputTimeStep_60fps,
      EOutputTimeStep_72fps,
      EOutputTimeStep_96fps,
      EOutputTimeStep_120fps,
      EOutputTimeStep_240fps,
    };

    enum EOutputRes {
      EOutputRes_640x480 = 0,
      EOutputRes_960x640,
      EOutputRes_1024x1024,
      EOutputRes_1280x720,
      EOutputRes_1600x1200,
      EOutputRes_1920x1080,
    };

    enum EOutputResMult {
      EOutputResMult_Quarter = 0,
      EOutputResMult_Half,
      EOutputResMult_Full,
      EOutputResMult_Double,
      EOutputResMult_Quadruple,
    };

    ///////////////////////////////////////////////////////////////////////////////

    enum ECOMPOSITEBlend {
      BoverAplusC = 0,
      AplusBplusC,
      AlerpBwithC,
      Asolo,
      Bsolo,
      Csolo,
    };

    enum EOp2CompositeMode {
      Op2AsumB = 0,
      Op2AmulB,
      Op2AdivB,
      Op2BoverA,
      Op2AoverB,
      Op2Asolo,
      Op2Bsolo,
    };

    enum EFrameEffect
    {
    	EFRAMEFX_NONE = 0,
    	EFRAMEFX_STANDARD,
    	EFRAMEFX_COMIC,
    	EFRAMEFX_GLOW,
    	EFRAMEFX_GHOSTLY,
    	EFRAMEFX_AFTERLIFE,
    };

} // namespace ork::lev2 {
