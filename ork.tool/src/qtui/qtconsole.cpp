////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/python/context.h>
#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
extern FILE* g_orig_stdout;
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <cassert>
#include <ork/kernel/Array.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtconsole.h>
///////////////////////////////////////////////////////////////////////////////
#include <QtWidgets/QScrollBar>
#include <QtCore/qtextstream.h>
#include <QtCore/qsocketnotifier.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/qtui/qtui.hpp>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/opq.h>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>
#include <cctype>
///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {
static int fd_tty_out_slave = -1;
static int fd_tty_err_slave = -1;
static int fd_tty_inp_slave = -1;
///////////////////////////////////////////////////////////////////////////////
static void getPythonOutput();
///////////////////////////////////////////////////////////////////////////////
vp_cons::vp_cons(const std::string& name)
    : ui::Viewport(name, 0, 0, 0, 0, fcolor3::Red(), 0.0f)
    , mCTQT(nullptr)
    , mPhase0(0)
    , mPhase1(0) {
}
static vp_cons* gPCON = nullptr;
///////////////////////////////////////////////////////////////////////////////
void vp_cons::Register() {
  gPCON = this;

  opq::mainSerialQueue()->enqueue([&]() {
    /*
    const char* inpname = python::slave_inp_name;
    const char* outname = python::slave_out_name;
    const char* errname = python::slave_err_name;

    fd_tty_inp_slave = open(inpname, O_WRONLY | O_NONBLOCK);
    fd_tty_out_slave = open(outname, O_RDONLY | O_NONBLOCK);
    fd_tty_err_slave = open(errname, O_RDONLY | O_NONBLOCK);

    assert(fd_tty_inp_slave >= 0);
    assert(fd_tty_out_slave >= 0);
    assert(fd_tty_err_slave >= 0);

    usleep(1 << 18);

    getPythonOutput();*/
  });
}
///////////////////////////////////////////////////////////////////////////////
ConsoleLine* AllocLine(vp_cons::LinePool& pool, std::list<ConsoleLine*>& list) {
  auto& used = pool.used();
  if (used.size() >= vp_cons::kmaxlines) {
    auto oldine = *used.begin();
    assert(oldine);
    pool.deallocate(oldine);
    auto it = list.begin();
    list.erase(it);
  }
  ConsoleLine* cline = pool.allocate();

  list.push_back(cline);

  return cline;
}

///////////////////////////////////////////////////////////////////////////////
void vp_cons::AppendOutput(const std::string& data) {
  std::vector<std::string> strsa;
  boost::split(strsa, data, boost::is_any_of("\n"));

  lev2::Context* pTARG = mCTQT->GetTarget();
  int IW               = pTARG->mainSurfaceWidth();
  int wrap             = IW / 10;

  std::vector<std::string> strsb;
  for (auto& line : strsa) {
    while (line.length() >= wrap) {
      auto subs = line.substr(0, wrap);
      // printf("line<%s>\n", line.c_str());
      // printf("subs<%s>\n", subs.c_str());
      strsb.push_back(subs);
      line = line.substr(wrap, line.length());
    }
    if (line.length())
      strsb.push_back(line);
  }

  /////////////////////////////////

  int inumstrs = strsb.size();

  for (int i = 0; i < inumstrs; i++) {
    std::string line           = strsb[i];
    std::string::size_type pos = line.find("\n");
    if (std::string::npos == pos) {
      line += ("\n");
    }
    auto cline = AllocLine(mLinePool, mLineList);

    cline->Set(line.c_str());
  }
}
///////////////////////////////////////////////////////////////////////////////
static void getPythonOutput() {
  /////////////////////

  auto readFromFile = [](int fileh) {
    char buf[256];
    int nread = read(fileh, buf, 254);
    switch (nread) {
      case -1: {
        if (errno == EAGAIN) {
          break;
        }
        perror("read");
        break;
      }
      default: {
        buf[nread] = 0;
        std::string outstr(&buf[0]);
        if (gPCON)
          gPCON->AppendOutput(outstr);
        break;
      }
    }
  };

  readFromFile(fd_tty_out_slave);
  readFromFile(fd_tty_err_slave);

  /////////////////////

  opq::mainSerialQueue()->enqueue([&]() { getPythonOutput(); });
}
///////////////////////////////////////////////////////////////////////////////
void ork::tool::vp_cons::BindCTQT(ork::lev2::CTQT* pctqt) {
  mCTQT = pctqt;
  mBaseMaterial.gpuInit(mCTQT->GetTarget());
  Register();

  lev2::RefreshPolicyItem item;
  item._policy = lev2::EREFRESH_FIXEDFPS;
  item._fps    = 8;
  mCTQT->pushRefreshPolicy(item);
}
///////////////////////////////////////////////////////////////////////////////
ui::HandlerResult vp_cons::DoOnUiEvent(const ui::Event& EV) {
  bool bisshift = EV.mbSHIFT;

  const auto& filtev = EV.mFilteredEvent;

  auto advancehist = [this]() {
    auto it      = mHistList.begin();
    size_t count = mHistIndex % mHistList.size();
    for (size_t i = 0; i < count; i++)
      it++;
    auto newl  = *it;
    mInputLine = newl->mBuffer;
  };

  switch (EV.miEventCode) {
    case ui::UIEV_KEY: {
      int mikeyc = filtev.miKeyCode;
      // printf( "%c\n", (char) mikeyc );
      switch (mikeyc) {
        case 13: // enter
        {
          auto cline = AllocLine(mLinePool, mLineList);
          cline->Set(mInputLine.c_str());

          auto hline = AllocLine(mHistPool, mHistList);
          hline->Set(mInputLine.c_str());

          mHistIndex = 0;

          mInputLine += "\n";

          write(fd_tty_inp_slave, mInputLine.c_str(), mInputLine.length());
          mInputLine = "";
          break;
        }
        case 16777219: {
          int len    = mInputLine.size();
          mInputLine = mInputLine.substr(0, len - 1);
          break;
        }
        case 16777235: // cursor up
        {
          mHistIndex++;
          advancehist();
          break;
        }
        case 16777237: // cursor dn
        {
          mHistIndex--;
          advancehist();
          break;
        }

        case 16777248: // shift (NOP)
        case 16777250: // ctrl

          break;
        default: {
          if (mikeyc < 255) {
            char buf[2] = {char(mikeyc), char(0)};

            mInputLine += buf;
            // printf( "mInputLine<%s>\n", mInputLine.c_str() );
          }
        } break;
      }
    }
  }
  return ui::HandlerResult(this);
}
///////////////////////////////////////////////////////////////////////////////
void vp_cons::DoDraw(ui::DrawEvent& drwev) {
  typedef lev2::SVtxV12C4T16 basevtx_t;

  if ((nullptr == mCTQT) || (nullptr == mCTQT->GetTarget()))
    return;

#if defined(__APPLE__)
    // if( 0 == g_orig_stdout ) return;
#endif

  lev2::Context* pTARG = mCTQT->GetTarget();

  int IW = pTARG->mainSurfaceWidth();
  int IH = pTARG->mainSurfaceHeight();

  pTARG->FBI()->SetAutoClear(true);
  pTARG->FBI()->SetClearColor(fcolor4(1.0f, 0.0f, 0.1f, 0.0f));
  BeginFrame(pTARG);
  auto VPRect = pTARG->mainSurfaceRectAtOrigin();
  pTARG->FBI()->pushViewport(VPRect);
  pTARG->MTXI()->PushUIMatrix();
  {
    /////////////////////////
    // GRADIENT BG
    /////////////////////////
    ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>& vbuf = lev2::GfxEnv::GetSharedDynamicVB();

    float faspect = pTARG->mainSurfaceAspectRatio();
    mBaseMaterial.SetColorMode(lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR);

    auto drawquad = [pTARG, &vbuf, this](u32 ucolor1, u32 ucolor2, float x0, float y0, float x1, float y1) {
      fvec2 uv0(0.0f, 0.0f);
      fvec2 uv1(1.0f, 0.0f);
      fvec2 uv2(1.0f, 1.0f);
      fvec2 uv3(0.0f, 1.0f);

      auto v0 = lev2::SVtxV12C4T16(fvec3(x0, y0, 0.0f), uv0, ucolor1);
      auto v1 = lev2::SVtxV12C4T16(fvec3(x1, y0, 0.0f), uv1, ucolor1);
      auto v2 = lev2::SVtxV12C4T16(fvec3(x1, y1, 0.0f), uv2, ucolor2);
      auto v3 = lev2::SVtxV12C4T16(fvec3(x0, y1, 0.0f), uv3, ucolor2);

      lev2::VtxWriter<lev2::SVtxV12C4T16> vw;
      vw.Lock(pTARG, &vbuf, 6);
      {
        vw.AddVertex(v0);
        vw.AddVertex(v1);
        vw.AddVertex(v2);

        vw.AddVertex(v0);
        vw.AddVertex(v2);
        vw.AddVertex(v3);
      }
      vw.UnLock(pTARG);

      pTARG->GBI()->DrawPrimitive(&mBaseMaterial, vw, ork::lev2::EPrimitiveType::TRIANGLES, 6);
    };

    drawquad(0xff200020, 0xff400030, 0, 0, IW, IH);

    /////////////////////////
    // TEXT
    /////////////////////////

    // const int inumlines = mLines.size();
    int inumlines_max_visible = 24; // IH/16;
    /////////////////////////
    int inumchars = 0;
    /////////////////////////
    std::vector<std::string> display_lines;
    display_lines.push_back(mInputLine);
    int inplinelen = mInputLine.size();
    // display_lines.reserve(inumlines);
    int ilctr              = 0;
    const auto& used_lines = mLinePool.used();
    int numlines           = used_lines.size();

    for (auto it = mLineList.rbegin(); it != mLineList.rend(); it++) {
      auto l            = (*it);
      const char* textl = l->mBuffer;
      auto linelen      = strlen(textl);
      if (linelen) // ine.length() )
      {
        bool bskipline = (linelen == 1) && textl[0] == '\n';
        if (false == bskipline && (ilctr < inumlines_max_visible)) {
          inumchars += linelen + 1;
          // fprintf( g_orig_stdout, "iline<%d> <%s>\n", iline, line.c_str() );
          display_lines.push_back(textl);
          ilctr++;
        }
      }
    }
    /////////////////////////
    int inumactuallines = display_lines.size();
    /////////////////////////
    float fx = 24;
    float fy = float(IH - 16); //-float(inumlines_viz)*16.0f;
    /////////////////////////
    // draw cursor
    /////////////////////////

    float sin0 = sinf(mPhase0);
    float cos0 = cosf(mPhase0);
    float sin1 = sinf(mPhase1);
    float cos1 = cosf(mPhase1);
    float sin2 = sinf(mPhase2);

    auto c0 = fvec4(0.5 + sin0 * 0.5, 0.2, 0.5 + cos0 * 0.5);
    auto c1 = fvec4(0.5 + sin1 * 0.5, 0.2, 0.5 + cos1 * 0.5);
    auto c2 = fvec4(0, 0.5 + sin2 * 0.5, 0);

    float cursorx = fx + (inplinelen * 9.0);
    drawquad(
        c2.GetABGRU32(),
        c2.GetABGRU32(), // abgr
        cursorx + 3,
        fy + 1,
        cursorx + 11,
        fy + 13);
    drawquad(
        c0.GetABGRU32(),
        c1.GetABGRU32(), // abgr
        cursorx + 4,
        fy + 2,
        cursorx + 10,
        fy + 12);
    /////////////////////////
    static int ibase = 0;
    pTARG->PushModColor(ork::fcolor4::Green());
    ork::lev2::FontMan::PushFont("i16");
    ork::lev2::FontMan::beginTextBlock(pTARG, inumchars + inumactuallines);
    {
      for (int ili = 0; ili < inumactuallines; ili++) {
        const std::string& line = display_lines[ili];
        ork::lev2::FontMan::DrawText(pTARG, fx, fy, line.c_str());
        fy -= 14.0f;
      }
    }
    ork::lev2::FontMan::endTextBlock(pTARG);
    pTARG->PopModColor();
    ibase++;
    /////////////////////////
  }
  pTARG->MTXI()->PopUIMatrix();
  pTARG->FBI()->popViewport();
  EndFrame(pTARG);

  mPhase0 += 0.41f;
  mPhase1 += 0.63f;
  mPhase2 += PI2 / 4.0;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::tool
