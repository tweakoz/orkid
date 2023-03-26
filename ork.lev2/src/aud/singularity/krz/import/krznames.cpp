////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "krzio.h"

namespace ork::audio::singularity::krzio {

const std::map<int, std::string>& get_controllermap() {
  static std::map<int, std::string> gmap = {
      {0, "MPress"},
      {33, "MPress"},
      {1, "MWheel"},
      {2, "Breath"},
      {4, "Foot"},
      {6, "Data"},
      {7, "Volume"},
      {10, "Pan"},
      {16, "Ctl A"},
      {17, "Ctl B"},
      {18, "Ctl C"},
      {19, "Ctl D"},
      {32, "Chan St"},
      {35, "PWheel"},
      {37, "AbsPwl"},
      {38, "GASR2"},
      {39, "GFUN2"},
      {40, "GLFO2"},
      {41, "GLFO2ph"},
      {42, "GFUN4"},
      {64, "SUST(MIDI64)"},
      {65, "PORT(MIDI65)"},
      {66, "SOST(MIDI66)"},
      {67, "SOFT(MIDI67)"},
      {68, "LEGATO(MIDI68)"},
      {70, "SNDCTRL1(MIDI70)"},
      {98, "KeyNum"},
      {99, "BKeyNum"},
      {100, "AttVel"},
      {101, "InvAttVel"},
      {104, "RelVel"},
      {105, "Bi-AVel"},
      {106, "VTRIG1"},
      {107, "VTRIG2"},
      {108, "RandV1"},
      {109, "RandV2"},
      {110, "ASR1"},
      {111, "ASR2"},
      {112, "FUN1"},
      {113, "FUN2"},
      {114, "LFO1"},
      {115, "LFO1ph"},
      {116, "LFO2"},
      {117, "LFO2ph"},
      {118, "FUN3"},
      {119, "FUN4"},
      {120, "AMPENV"},
      {121, "ENV2"},
      {122, "ENV3"},
      {123, "LOOPST"},
      {124, "SAMPLEPBRATE"},
      {125, "ATKSTATE"},
      {126, "RELSTATE"},
      {127, "ON"},
      {128, "-ON"},
      {129, "GKeyNum"},
      {130, "GAttVel"},
      {131, "GHiKey"},
      {132, "GLoKey"},
  };

  return gmap;
}

const std::map<int, std::string>& get_funopmap() {
  static std::map<int, std::string> gmap = {
      {0, "None"},
      {1, "a+b"},
      {2, "a-b"},
      {3, "(a+b)/2"},
      {4, "a/2+b"},
      {5, "a/4+b/2"},
      {6, "(a+2b)/3"},
      {9, "a*b"},
      {10, "-a*b"},
      {13, "a*10^b"},
      {16, "|a+b|"},
      {17, "|a-b|"},
      {18, "min(a,b)"},
      {19, "max(a,b)"},
      {20, "Quantize B To A"},
      {22, "lowpass(f=a,b)"},
      {23, "hipass(f=a,b)"},
      {25, "b/(1-a)"},
      {27, "a(b-y)"},
      {29, "(a+b)^2"},
      {31, "sin(a+b)"},
      {32, "cos(a+b)"},
      {33, "tri(a+b)"},
      {35, "warp1(a,b)"},
      {36, "warp2(a,b)"},
      {37, "warp3(a,b)"},
      {38, "warp4(a,b)"},
      {39, "warp8(a,b)"},
      {41, "a AND b"},
      {42, "a OR b"},
      {43, "b > a"},
      {47, "ramp(f=a+b)"},
      {48, "ramp(f=a-b)"},
      {49, "ramp(f=(a+b)/2)"},
      {50, "ramp(f=a*b)"},
      {51, "ramp(f=-a*b)"},
      {52, "ramp(f=a*10^b)"},
      {53, "ramp(f=(a+b)/4)"},
      {54, "a(y+b)"},
      {55, "ay+b"},
      {56, "(a+1)y+b"},
      {57, "y+a(y+b)"},
      {58, "a|y|+b"},
      {61, "Sample B on A"},
      {62, "Sample B on ~A"},
      {63, "Track B while A"},
      {64, "diode(a-b)"},
      {65, "diode(a-b+.5)"},
      {66, "diode(a-b-.5)"},
      {67, "diode(a-b+.25)"},
      {68, "diode(a-b-.25)"},
      {69, "Track B while ~A"},
  };

  return gmap;
}

std::string getControlSourceName(int srcid) {
  std::stringstream ss;

  if (srcid >= 133 and srcid <= 141) {
    int x = -99 + (srcid - 133);
    ss << float(x) * 0.01f;
  } else if (srcid >= 142 and srcid <= 181) {
    int x = -90 + (srcid - 142) * 2;
    ss << float(x) * 0.01f;
  } else if (srcid >= 182 and srcid <= 191) {
    int x = -10 + (srcid - 182);
    ss << float(x) * 0.01f;
  } else if (srcid >= 192 and srcid <= 201) {
    int x = 0 + (srcid - 192);
    ss << float(x) * 0.01f;
  } else if (srcid >= 202 and srcid <= 241) {
    int x = 10 + (srcid - 202);
    ss << float(x) * 0.01f;
  } else if (srcid >= 242 and srcid <= 251) {
    int x = 90 + (srcid - 242);
    ss << float(x) * 0.01f;
  } else if (srcid >= 133 and srcid <= 225) {
    ss << "val(" << srcid << ")";
  } else {
    auto& ctrlmap = get_controllermap();

    auto it = ctrlmap.find(srcid);
    if (it != ctrlmap.end()) {
      ss << it->second;
    } else {
      ss << "MIDI(" << srcid << ")";
    }
  }

  return ss.str();
}

std::string getFunOpName(int srcid) {
  std::stringstream ss;

  auto& ctrlmap = get_funopmap();

  auto it = ctrlmap.find(srcid);
  if (it != ctrlmap.end()) {
    ss << it->second;
  } else {
    ss << "FUNOP(" << srcid << ")";
  }

  return ss.str();
}

float getAsrTime(int ival) {
  float rval = 0.0f;

  switch (ival) {
    case 0:
      rval = 0.000;
      break;
    case 1:
      rval = 0.002;
      break;
    case 2:
      rval = 0.005;
      break;
    case 3:
      rval = 0.010;
      break;
    case 250:
      rval = 35.00;
      break;
    case 251:
      rval = 40.00;
      break;
    case 252:
      rval = 45.00;
      break;
    case 253:
      rval = 50.00;
      break;
    case 254:
      rval = 55.00;
      break;
    case 255:
      rval = 60.00;
      break;
    default: {
      if (ival >= 4 and ival <= 102) {
        int x = 2 + (ival - 4) * 2;
        rval  = float(x) * 0.01f;

      } else if (ival >= 103 and ival <= 177) {
        int x = 200 + (ival - 103) * 4;
        rval  = float(x) * 0.01f;
      } else if (ival >= 178 and ival <= 227) {
        int x = 500 + (ival - 178) * 10;
        rval  = float(x) * 0.01f;
      } else if (ival >= 228 and ival <= 249) {
        int x = 1000 + (ival - 228) * 50;
        rval  = float(x) * 0.01f;
      }
      break;
    }
  }

  return rval;
}

float getEnvCtrl(int ival) {
  int nval = makeSigned(ival);
  // printf( "ival<%d> nval<%d>\n", ival, nval );
  ival               = nval + 43;
  const int valtab[] = {
      18,    20,    22,    25,    27,    30,    33,    36,    40,    43,    47,    50,    55,    61,    67,    73,    80,    90,
      100,   110,   120,   130,   140,   150,   160,   180,   200,   220,   250,   270,   300,   330,   360,   400,   430,   470,
      500,   550,   610,   670,   730,   800,   900,   1000,  1100,  1200,  1300,  1400,  1500,  1600,  1800,  2000,  2200,  2500,
      2700,  3000,  3300,  3600,  4000,  4300,  4700,  5000,  5500,  6100,  6700,  7300,  8000,  9000,  10000, 11000, 12000, 13000,
      14000, 15000, 16000, 18000, 20000, 22000, 25000, 27000, 30000, 33000, 36000, 40000, 43000, 47000, 50000,
  };
  static_assert((sizeof(valtab) / sizeof(int)) == 87, "incorrect num of envctrl values");
  assert(ival >= 0);
  if (ival > 87)
    ival = 87;
  return float(valtab[ival]) * 0.001f;
}

float get72Adjust(int index) {
  const int ktabsize = 151;

  const float valtab[ktabsize] = {

      0.010f, 0.010f, 0.011f, 0.013f, 0.014f, 0.015f, 0.015f, 0.016f, 0.018f, 0.019f, 0.020f, 0.021f, 0.024f, 0.025f,
      0.028f, 0.030f, 0.031f, 0.034f, 0.035f, 0.038f, 0.040f, 0.041f, 0.044f, 0.045f, 0.048f, 0.050f, 0.055f, 0.060f,
      0.065f, 0.070f, 0.075f, 0.080f, 0.085f, 0.090f, 0.095f, 0.100f, 0.110f, 0.120f, 0.130f, 0.140f, 0.150f, 0.160f,
      0.170f, 0.180f, 0.190f, 0.200f, 0.220f, 0.240f, 0.260f, 0.280f, 0.300f, 0.320f, 0.340f, 0.360f, 0.380f, 0.400f,
      0.420f, 0.440f, 0.460f, 0.480f, 0.500f, 0.550f, 0.600f, 0.650f, 0.700f, 0.750f, 0.800f, 0.850f, 0.900f, 0.950f,
      1.000f, 1.050f, 1.100f, 1.150f, 1.200f, 1.250f, 1.300f, 1.350f, 1.400f, 1.450f, 1.500f, 1.550f, 1.600f, 1.650f,
      1.700f, 1.750f, 1.800f, 1.850f, 1.900f, 1.950f, 2.000f, 2.050f, 2.100f, 2.150f, 2.200f, 2.250f, 2.300f, 2.350f,
      2.400f, 2.450f, 2.500f, 2.550f, 2.600f, 2.650f, 2.700f, 2.750f, 2.800f, 2.850f, 2.900f, 2.950f, 3.000f, 3.050f,
      3.100f, 3.150f, 3.200f, 3.250f, 3.300f, 3.350f, 3.400f, 3.450f, 3.500f, 3.550f, 3.600f, 3.650f, 3.700f, 3.750f,
      3.800f, 3.850f, 3.900f, 3.950f, 4.000f, 4.050f, 4.100f, 4.150f, 4.200f, 4.250f, 4.300f, 4.350f, 4.400f, 4.450f,
      4.500f, 4.550f, 4.600f, 4.650f, 4.700f, 4.750f, 4.800f, 4.850f, 4.900f, 4.950f, 5.000f,

  };

  static_assert((sizeof(valtab) / sizeof(float)) == ktabsize, "incorrect num of values");

  if (index > ktabsize)
    index = ktabsize;
  // if(ival>87) ival = 87;

  return valtab[index];
}

int getVelTrack96(int ival) {
  int nval = makeSigned(ival);
  // printf( "ival<%d> nval<%d>\n", ival, nval );
  ival               = nval + 127;
  const int ktabsize = 255;

  const int valtab[ktabsize] = {
      -10800, -10400, -10000, -9600, -9500, -9400, -9300, -9200, -9100, -9000, -8900, -8800, -8700, -8600, -8500, -8400, -8300,
      -8200,  -8100,  -8000,  -7900, -7800, -7700, -7600, -7500, -7400, -7300, -7200, -7100, -7000, -6900, -6800, -6700, -6600,
      -6500,  -6400,  -6300,  -6200, -6100, -6000, -5900, -5800, -5700, -5600, -5500, -5400, -5300, -5200, -5100, -5000, -4900,
      -4800,  -4700,  -4600,  -4500, -4400, -4300, -4200, -4100, -4000, -3900, -3800, -3700, -3600, -3500, -3400, -3300, -3200,
      -3100,  -3000,  -2900,  -2800, -2700, -2600, -2500, -2400, -2300, -2200, -2100, -2000, -1900, -1800, -1700, -1600, -1500,
      -1400,  -1300,  -1200,  -1100, -1000, -900,  -800,  -700,  -600,  -500,  -450,  -400,  -350,  -300,  -250,  -200,  -150,
      -120,   -100,   -90,    -80,   -70,   -60,   -55,   -50,   -45,   -40,   -35,   -30,   -27,   -24,   -22,   -20,   -18,
      -16,    -14,    -12,    -10,   -8,    -6,    -4,    -2,    0,     2,     4,     6,     8,     10,    12,    14,    16,
      18,     20,     22,     24,    27,    30,    35,    40,    45,    50,    55,    60,    70,    80,    90,    100,   120,
      150,    200,    250,    300,   350,   400,   450,   500,   600,   700,   800,   900,   1000,  1100,  1200,  1300,  1400,
      1500,   1600,   1700,   1800,  1900,  2000,  2100,  2200,  2300,  2400,  2500,  2600,  2700,  2800,  2900,  3000,  3100,
      3200,   3300,   3400,   3500,  3600,  3700,  3800,  3900,  4000,  4100,  4200,  4300,  4400,  4500,  4600,  4700,  4800,
      4900,   5000,   5100,   5200,  5300,  5400,  5500,  5600,  5700,  5800,  5900,  6000,  6100,  6200,  6300,  6400,  6500,
      6600,   6700,   6800,   6900,  7000,  7100,  7200,  7300,  7400,  7500,  7600,  7700,  7800,  7900,  8000,  8100,  8200,
      8300,   8400,   8500,   8600,  8700,  8800,  8900,  9000,  9100,  9200,  9300,  9400,  9500,  9600,  10000, 10400, 10800,
  };

  static_assert((sizeof(valtab) / sizeof(int)) == ktabsize, "incorrect num of values");

  assert(ival >= 0);
  if (ival > ktabsize)
    ival = ktabsize;
  // if(ival>87) ival = 87;

  return valtab[ival];
}

float getVelTrack97(int ival) {
  int nval = makeSigned(ival);
  // printf( "ival<%d> nval<%d>\n", ival, nval );
  ival               = nval + 105;
  const int ktabsize = 211;

  const float valtab[ktabsize] = {

      -5.00f, -4.90f, -4.80f, -4.70f, -4.60f, -4.50f, -4.40f, -4.30f, -4.20f, -4.10f, -4.00f, -3.90f, -3.80f, -3.70f, -3.60f,
      -3.50f, -3.40f, -3.30f, -3.20f, -3.10f, -3.00f, -2.90f, -2.80f, -2.70f, -2.60f, -2.50f, -2.40f, -2.30f, -2.20f, -2.10f,
      -2.00f, -1.95f, -1.90f, -1.85f, -1.80f, -1.75f, -1.70f, -1.65f, -1.60f, -1.55f, -1.50f, -1.45f, -1.40f, -1.35f, -1.30f,
      -1.25f, -1.20f, -1.15f, -1.10f, -1.05f, -1.00f, -0.98f, -0.96f, -0.94f, -0.92f, -0.90f, -0.88f, -0.86f, -0.84f, -0.82f,
      -0.80f, -0.78f, -0.76f, -0.74f, -0.72f, -0.70f, -0.68f, -0.66f, -0.64f, -0.62f, -0.60f, -0.58f, -0.56f, -0.54f, -0.52f,
      -0.50f, -0.48f, -0.46f, -0.44f, -0.42f, -0.40f, -0.38f, -0.36f, -0.34f, -0.32f, -0.30f, -0.28f, -0.26f, -0.24f, -0.22f,
      -0.20f, -0.18f, -0.16f, -0.14f, -0.12f, -0.10f, -0.09f, -0.08f, -0.07f, -0.06f, -0.05f, -0.04f, -0.03f, -0.02f, -0.01f,
      0.00f,  0.01f,  0.02f,  0.03f,  0.04f,  0.05f,  0.06f,  0.07f,  0.08f,  0.09f,  0.10f,  0.12f,  0.14f,  0.16f,  0.18f,
      0.20f,  0.22f,  0.24f,  0.26f,  0.28f,  0.30f,  0.32f,  0.34f,  0.36f,  0.38f,  0.40f,  0.42f,  0.44f,  0.46f,  0.48f,
      0.50f,  0.52f,  0.54f,  0.56f,  0.58f,  0.60f,  0.62f,  0.64f,  0.66f,  0.68f,  0.70f,  0.72f,  0.74f,  0.76f,  0.78f,
      0.80f,  0.82f,  0.84f,  0.86f,  0.88f,  0.90f,  0.92f,  0.94f,  0.96f,  0.98f,  1.00f,  1.05f,  1.10f,  1.15f,  1.20f,
      1.25f,  1.30f,  1.35f,  1.40f,  1.45f,  1.50f,  1.55f,  1.60f,  1.65f,  1.70f,  1.75f,  1.80f,  1.85f,  1.90f,  1.95f,
      2.00f,  2.10f,  2.20f,  2.30f,  2.40f,  2.50f,  2.60f,  2.70f,  2.80f,  2.90f,  3.00f,  3.10f,  3.20f,  3.30f,  3.40f,
      3.50f,  3.60f,  3.70f,  3.80f,  3.90f,  4.00f,  4.10f,  4.20f,  4.30f,  4.40f,  4.50f,  4.60f,  4.70f,  4.80f,  4.90f,
      5.00f,

  };

  static_assert((sizeof(valtab) / sizeof(float)) == ktabsize, "incorrect num of values");

  assert(ival >= 0);
  if (ival > ktabsize)
    ival = ktabsize;
  // if(ival>87) ival = 87;

  return valtab[ival];
}

int getVelTrack98(int ival) {
  int nval = makeSigned(ival);
  // printf( "ival<%d> nval<%d>\n", ival, nval );
  ival                       = nval + 123;
  const int ktabsize         = 247;
  const int valtab[ktabsize] = {

      -7200, -6700, -6500, -6000, -5500, -5300, -5000, -4900, -4800, -4700, -4600, -4500, -4400, -4300, -4200, -4100, -4000, -3900,
      -3800, -3700, -3600, -3500, -3400, -3300, -3200, -3100, -3000, -2900, -2800, -2700, -2600, -2500, -2400, -2300, -2200, -2100,
      -2000, -1900, -1800, -1700, -1600, -1500, -1400, -1300, -1200, -1100, -1000, -950,  -900,  -850,  -800,  -750,  -700,  -650,
      -600,  -550,  -500,  -450,  -400,  -380,  -360,  -340,  -320,  -300,  -280,  -260,  -240,  -220,  -200,  -190,  -180,  -170,
      -160,  -150,  -140,  -130,  -120,  -110,  -100,  -95,   -90,   -85,   -80,   -75,   -70,   -65,   -60,   -55,   -50,   -48,
      -46,   -44,   -42,   -40,   -38,   -36,   -34,   -32,   -30,   -28,   -26,   -24,   -22,   -20,   -19,   -18,   -17,   -16,
      -15,   -14,   -13,   -12,   -11,   -10,   -9,    -8,    -7,    -6,    -5,    -4,    -3,    -2,    -1,    0,     1,     2,
      3,     4,     5,     6,     7,     8,     9,     10,    11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
      22,    24,    26,    28,    30,    32,    34,    36,    38,    40,    42,    44,    46,    48,    50,    55,    60,    65,
      70,    75,    80,    85,    90,    95,    100,   110,   120,   130,   140,   150,   160,   170,   180,   190,   200,   220,
      240,   260,   280,   300,   320,   340,   360,   380,   400,   450,   500,   550,   600,   650,   700,   750,   800,   850,
      900,   950,   1000,  1100,  1200,  1300,  1400,  1500,  1600,  1700,  1800,  1900,  2000,  2100,  2200,  2300,  2400,  2500,
      2600,  2700,  2800,  2900,  3000,  3100,  3200,  3300,  3400,  3500,  3600,  3700,  3800,  3900,  4000,  4100,  4200,  4300,
      4400,  4500,  4600,  4700,  4800,  4900,  5000,  5300,  5500,  6000,  6500,  6700,  7200,

  };

  static_assert((sizeof(valtab) / sizeof(int)) == ktabsize, "incorrect num of values");

  assert(ival >= 0);
  if (ival > ktabsize)
    ival = ktabsize;
  // if(ival>87) ival = 87;

  return valtab[ival];
}

std::string getMidiNoteName(int uval) {
  int note   = uval % 12;
  int octave = (uval / 12) - 2;

  const int ktabsize     = 12;
  const char* nottab[12] = {"C ", "C# ", "D ", "D# ", "E ", "F ", "F# ", "G ", "G# ", "A ", "A# ", "B "};

  auto rval = ork::FormatString("%s%d", nottab[note], octave);
  return rval;
}

Keystart getKeyStart81(int uval) {
  int nval = makeSigned(uval);
  // printf( "ival<%d> nval<%d> nnval<%d>\n", ival, nval, nval+48 );
  Keystart rval;

  const int ktabsize     = 12;
  const char* nottab[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

  if (nval >= 0) {
    int kstart = nval;
    int note   = kstart % 12;
    int octave = (kstart / 12) - 1;

    rval._note   = nottab[note];
    rval._octave = octave;
    rval._mode   = "Unipolar";
  } else // if( nval<0 )
  {
    int kstart = nval + 120;
    assert(kstart < 120);

    int note     = kstart % 12;
    int octave   = kstart / 12;
    rval._note   = nottab[note];
    rval._octave = octave;
    rval._mode   = "Bipolar";
  }
  return rval;
}

std::string getFreq83(int ival) {
  int nval = makeSigned(ival);
  // printf( "ival<%d> nval<%d> nnval<%d>\n", ival, nval, nval+48 );
  ival = nval + 48;
  ival = ival & 0x7f;

  int note   = ival % 12;
  int octave = ival / 12;

  const int ktabsize     = 12;
  const char* nottab[12] = {"C ", "C# ", "D ", "D# ", "E ", "F ", "F# ", "G ", "G# ", "A ", "A# ", "B "};

  auto rval = ork::FormatString("%s%d", nottab[note], octave);
  return rval;
}
std::string getLfoShape87(int ival) {
  static std::map<int, std::string> shmap = {
      {0, "None"},     {1, "Sine"},      {2, "+Sine"},    {3, "Square"},     {4, "+Square"},  {5, "Triangle"},  {6, "+Triangle"},
      {7, "Rise Saw"}, {8, "+Rise Saw"}, {9, "Fall Saw"}, {10, "+Fall Saw"}, {20, "3 Step"},  {21, "+3 Step"},  {22, "4 Step"},
      {23, "+4 Step"}, {24, "5 Step"},   {25, "+5 Step"}, {26, "6 Step"},    {27, "+6 Step"}, {28, "7 Step"},   {29, "+7 Step"},
      {30, "8 Step"},  {31, "+8 Step"},  {34, "10 Step"}, {35, "+10 Step"},  {38, "12 Step"}, {39, "+12 Step"},
  };
  std::stringstream ss;
  auto it = shmap.find(ival);
  if (it != shmap.end()) {
    ss << it->second;
  } else {
    ss << "LFOSHAPE(" << ival << ")";
  }

  return ss.str();
}
float getLfoRate86(int ival) {
  const int ktabsize           = 185;
  const float valtab[ktabsize] = {
      0.00f, 0.01f, 0.02f, 0.03f, 0.04f, 0.05f, 0.06f, 0.07f, 0.08f, 0.09f, 0.10f, 0.11f, 0.12f, 0.13f, 0.14f, 0.15f, 0.16f,
      0.17f, 0.18f, 0.19f, 0.20f, 0.25f, 0.30f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.75f, 0.80f, 0.85f,
      0.90f, 0.95f, 1.00f, 1.10f, 1.20f, 1.30f, 1.40f, 1.50f, 1.60f, 1.70f, 1.80f, 1.90f, 2.00f, 2.10f, 2.20f, 2.30f, 2.40f,
      2.50f, 2.60f, 2.70f, 2.80f, 2.90f, 3.00f, 3.10f, 3.20f, 3.30f, 3.40f, 3.50f, 3.60f, 3.70f, 3.80f, 3.90f, 4.00f, 4.10f,
      4.20f, 4.30f, 4.40f, 4.50f, 4.60f, 4.70f, 4.80f, 4.90f, 5.00f, 5.10f, 5.20f, 5.30f, 5.40f, 5.50f, 5.60f, 5.70f, 5.80f,
      5.90f, 6.00f, 6.10f, 6.20f, 6.30f, 6.40f, 6.50f, 6.60f, 6.70f, 6.80f, 6.90f, 7.00f, 7.10f, 7.20f, 7.30f, 7.40f, 7.50f,
      7.60f, 7.70f, 7.80f, 7.90f, 8.00f, 8.10f, 8.20f, 8.30f, 8.40f, 8.50f, 8.60f, 8.70f, 8.80f, 8.90f, 9.00f, 9.10f, 9.20f,
      9.30f, 9.40f, 9.50f, 9.60f, 9.70f, 9.80f, 9.90f, 0.00f, 0.20f, 0.40f, 0.60f, 0.80f, 1.00f, 1.20f, 1.40f, 1.60f, 1.80f,
      2.00f, 2.20f, 2.40f, 2.60f, 2.80f, 3.00f, 3.20f, 3.40f, 3.60f, 3.80f, 4.00f, 4.20f, 4.40f, 4.60f, 4.80f, 5.00f, 5.20f,
      5.40f, 5.60f, 5.80f, 6.00f, 6.20f, 6.40f, 6.60f, 6.80f, 7.00f, 7.20f, 7.40f, 7.60f, 7.80f, 8.00f, 8.20f, 8.40f, 8.60f,
      8.80f, 9.00f, 9.20f, 9.40f, 9.60f, 9.80f, 0.00f, 0.50f, 1.00f, 1.50f, 2.00f, 2.50f, 3.00f, 3.50f, 4.00f,
  };
  static_assert((sizeof(valtab) / sizeof(int)) == ktabsize, "incorrect num of values");

  assert(ival >= 0);
  if (ival > ktabsize)
    ival = ktabsize;
  return valtab[ival];
}
int getKeyTrack85(int ival) {
  int nval = makeSigned(ival);
  // printf( "ival<%d> nval<%d>\n", ival, nval );
  ival                       = nval + 120;
  const int ktabsize         = 241;
  const int valtab[ktabsize] = {
      -2400, -2200, -2000, -1800, -1600, -1500, -1400, -1300, -1200, -1150, -1100, -1050, -1000, -950, -900, -850, -800, -750, -700,
      -650,  -600,  -550,  -500,  -450,  -400,  -380,  -360,  -340,  -320,  -300,  -280,  -260,  -240, -220, -200, -195, -190, -185,
      -180,  -175,  -170,  -165,  -160,  -158,  -156,  -154,  -152,  -150,  -148,  -146,  -144,  -142, -140, -138, -136, -134, -132,
      -130,  -128,  -126,  -124,  -122,  -120,  -118,  -116,  -114,  -112,  -110,  -109,  -108,  -107, -106, -105, -104, -103, -102,
      -101,  -100,  -99,   -98,   -97,   -96,   -95,   -94,   -93,   -92,   -91,   -90,   -88,   -86,  -84,  -82,  -80,  -78,  -76,
      -74,   -72,   -70,   -68,   -66,   -64,   -62,   -60,   -58,   -56,   -54,   -52,   -50,   -48,  -46,  -44,  -42,  -40,  -35,
      -30,   -25,   -20,   -15,   -10,   -5,    0,     5,     10,    15,    20,    25,    30,    35,   40,   42,   44,   46,   48,
      50,    52,    54,    56,    58,    60,    62,    64,    66,    68,    70,    72,    74,    76,   78,   80,   82,   84,   86,
      88,    90,    91,    92,    93,    94,    95,    96,    97,    98,    99,    100,   101,   102,  103,  104,  105,  106,  107,
      108,   109,   110,   112,   114,   116,   118,   120,   122,   124,   126,   128,   130,   132,  134,  136,  138,  140,  142,
      144,   146,   148,   150,   152,   154,   156,   158,   160,   165,   170,   175,   180,   185,  190,  195,  200,  220,  240,
      260,   280,   300,   320,   340,   360,   380,   400,   450,   500,   550,   600,   650,   700,  750,  800,  850,  900,  950,
      1000,  1050,  1100,  1150,  1200,  1300,  1400,  1500,  1600,  1800,  2000,  2200,  2400,
  };

  static_assert((sizeof(valtab) / sizeof(int)) == ktabsize, "incorrect num of values");

  assert(ival >= 0);
  if (ival > ktabsize)
    ival = ktabsize;
  return valtab[ival];
}

int makeSigned(int inp) {
  int rval  = inp;
  bool sign = (rval & 0x80);
  if (sign) {
    rval = inp - 256;
  }
  return rval;
}

const std::map<int, std::string>& get_dspblkmap() {
  static std::map<int, std::string> gmap = {
      {1, "AMP"},
      {15, "LOPASS"},
      {16, "HIPASS"},
      {17, "ALPASS"},
      {18, "GAIN"},
      {19, "SHAPER"},
      {20, "DIST"},
      {22, "PWM"},
      {23, "SINE"},
      {24, "LF SIN"},
      {25, "SW+SHP"},
      {26, "SAW+"},
      {27, "SAW"},
      {28, "LF SAW"},
      {29, "SQUARE"},
      {30, "LF SQR"},
      {31, "WRAP"},
      {33, "SYNC M"},
      {34, "SYNC S"},
      {35, "BAND2"},
      {36, "NOTCH2"},
      {37, "LOPAS2"},
      {40, "PANNER"},
      {41, "x GAIN"},
      {42, "+ GAIN"},
      {43, "XFADE"},
      {44, "AMPMOD"},
      {48, "x AMP"},
      {49, "+ AMP"},
      {52, "HIPAS2"},
      {53, "SW+DST"},
      {57, "LPGATE"},
      {60, "NONE"},
      {63, "NONE"},
      {69, "LOPAS2"},
      {70, "LPCLIP"},
      {71, "SINE+"},
      {73, "LP2RES"},
      {74, "SHAPE2"},
      {75, "! AMP"},
      {76, "NOISE+"},
      {77, "MASTER"},
      {78, "SLAVE"},
      {2, "2POLE LOWPASS"},
      {3, "BANDPASS FILT"},
      {4, "NOTCH FILTER"},
      {5, "2POLE ALLPASS"},
      {8, "PARA BASS"},
      {9, "PARA TREBLE"},
      {10, "PARA BASS"},
      {11, "PARA TREBLE"},
      {38, "AMP U   AMP L"},
      {39, "BAL     AMP"},
      {51, "PARA MID"},
      {61, "NONE"},
      {64, "2PARAM SHAPER"},
      {66, "x SHAPEMOD OSC"},
      {67, "+ SHAPEMOD OSC"},
      {68, "SHAPE MOD OSC"},
      {72, "AMP MOD OSC"},
      {12, "HIFREQ STIMULATOR"},
      {13, "PARAMETRIC EQ"},
      {14, "STEEP RESONANT BASS"},
      {50, "4POLE LOPASS W/SEP"},
      {54, "4POLE HIPASS W/SEP"},
      {55, "TWIN PEAKS BANDPASS"},
      {56, "DOUBLE NOTCH W/SEP"},
      {62, "NONE"},
  };

  return gmap;
}
std::string getDspBlockName(int srcid) {
  std::stringstream ss;

  auto& dspmap = get_dspblkmap();

  auto it = dspmap.find(srcid);
  if (it != dspmap.end()) {
    ss << it->second;
  } else {
    ss << "DSPBLK(" << srcid << ")";
  }

  return ss.str();
}
const std::map<int, std::string>& get_dspblksch() {
  static std::map<int, std::string> gmap = {
      {1, "AMP"},          {15, "FRQ"},         {16, "FRQ"},         {17, "FRQ"},         {18, "AMP"},         {19, "AMT"},
      {20, "DRV"},         {22, "WID(PWM)"},    {23, "PCH"},         {24, "PCH(LF)"},     {25, "PCH"},         {26, "PCH"},
      {27, "PCH"},         {28, "PCH(LF)"},     {29, "PCH"},         {30, "PCH(LF)"},     {31, "WRP"},         {33, "PCH"},
      {34, "PCH"},         {35, "FRQ"},         {36, "FRQ"},         {37, "FRQ"},         {40, "POS"},         {41, "AMP"},
      {42, "AMP"},         {43, "XFD"},         {44, "AMP"},         {48, "AMP"},         {49, "AMP"},         {52, "FRQ"},
      {53, "PCH"},         {57, "FRQ"},         {60, "OFF"},         {63, "OFF"},         {69, "FRQ"},         {70, "FRQ"},
      {71, "PCH"},         {73, "FRQ"},         {74, "AMT"},         {75, "AMP"},         {76, "AMP"},         {77, "FRQ"},
      {78, "FRQ"},         {2, "FRQ RES"},      {3, "FRQ WID"},      {4, "FRQ WID"},      {5, "FRQ WID"},      {8, "FRQ AMP"},
      {9, "FRQ AMP"},      {10, "FRQ AMP"},     {11, "FRQ AMP"},     {38, "AMP AMP"},     {39, "POS AMP"},     {51, "FRQ AMP"},
      {61, "OFF OFF"},     {64, "EVN ODD"},     {66, "PCH DEP"},     {67, "PCH DEP"},     {68, "PCH DEP"},     {72, "PCH DEP"},
      {12, "FRQ DRV AMP"}, {13, "FRQ WID AMP"}, {14, "FRQ RES AMP"}, {50, "FRQ RES SEP"}, {54, "FRQ RES SEP"}, {55, "FRQ WID SEP"},
      {56, "FRQ WID SEP"}, {62, "OFF OFF OFF"},
  };

  return gmap;
}
std::string getDspBlockScheme(int fnid) {
  std::stringstream ss;

  auto& dspmap = get_dspblksch();

  auto it = dspmap.find(fnid);
  if (it != dspmap.end()) {
    ss << it->second;
  } else {
    ss << "";
  }

  return ss.str();
}

int getDspBlockWidth(int fnid) {
  auto schm = getDspBlockScheme(fnid);
  int n     = std::count(schm.begin(), schm.end(), ' ');
  return n + 1;
}

algcfg getAlgConfig(int algID) {
  switch (algID) {
    //               F 1 2 3 4
    case 1:
      return {1, 3, 0, 0, 1};
      break;
    case 2:
      return {1, 2, 0, 1, 1};
      break;
    case 3:
      return {1, 2, 0, 2, 0};
      break;
    case 4:
      return {1, 2, 0, 1, 1};
      break;
    case 5:
      return {1, 2, 0, 1, 1};
      break;
    case 6:
      return {1, 2, 0, 1, 1};
      break;
    case 7:
      return {1, 2, 0, 1, 1};
      break;
    case 8:
      return {1, 1, 1, 1, 1};
      break;
    case 9:
      return {1, 1, 1, 1, 1};
      break;
    case 10:
      return {1, 1, 1, 1, 1};
      break;

    //               F 1 2 3 4
    case 11:
      return {1, 1, 1, 1, 1};
      break;
    case 12:
      return {1, 1, 1, 1, 1};
      break;
    case 13:
      return {1, 1, 1, 1, 1};
      break;

    //               F 1 2 3 4
    case 14:
      return {1, 1, 1, 2, 0};
      break;
    case 15:
      return {1, 1, 1, 2, 0};
      break;

    case 16:
      return {1, 1, 2, 0, 1};
      break;
    case 17:
      return {1, 1, 2, 0, 1};
      break;
    case 18:
      return {1, 1, 2, 0, 1};
      break;
    case 19:
      return {1, 1, 2, 0, 1};
      break;

    //               F 1 2 3 4
    case 20:
      return {1, 1, 1, 1, 1};
      break;
    case 21:
      return {1, 1, 1, 1, 1};
      break;
    case 22:
      return {1, 1, 1, 1, 1};
      break;
    case 23:
      return {1, 1, 1, 1, 1};
      break;
    case 24:
      return {1, 1, 1, 1, 1};
      break;

    case 25:
      return {1, 1, 1, 2, 0};
      break;

    //               F 1 2 3 4
    case 26:
      return {0, 1, 1, 1, 1};
      break;
    case 27:
      return {0, 1, 1, 1, 1};
      break;
    case 28:
      return {0, 1, 1, 1, 1};
      break;
    case 29:
      return {0, 1, 1, 1, 1};
      break;
    case 30:
      return {0, 1, 1, 1, 1};
      break;

    case 31:
      return {0, 1, 1, 2, 0};
      break;
  }

  return algcfg();
}
} // namespace ork::audio::singularity::krzio
