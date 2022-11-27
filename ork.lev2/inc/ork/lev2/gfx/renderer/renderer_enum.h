#pragma once

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

enum class CompositeBlendMode {
  BoverAplusC = 0,
  AplusBplusC,
  AlerpBwithC,
  Asolo,
  Bsolo,
  Csolo,
};

enum class Op2CompositeMode {
  Op2AsumB = 0,
  Op2AmulB,
  Op2AdivB,
  Op2BoverA,
  Op2AoverB,
  Op2Asolo,
  Op2Bsolo,
};

enum class FrameEffect {
  NONE = 0,
  STANDARD,
  COMIC,
  GLOW,
  GHOSTLY,
  AFTERLIFE,
};

} // namespace ork::lev2
