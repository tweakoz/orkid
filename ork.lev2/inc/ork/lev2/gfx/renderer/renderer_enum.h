#pragma once

namespace ork::lev2 {

  enum RenderGroupState
  {
    ERGST_NONE = 0,
    ERGST_FIRST,
    ERGST_CONTINUE,
    ERGST_LAST,
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
