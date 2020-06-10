#include "harness.h"
#include <boost/program_options.hpp>
#include <iostream>
#include <ork/lev2/aud/singularity/hud.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorDeferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorForward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
///////////////////////////////////////////////////////////////////////////////

namespace po = boost::program_options;

#if defined(__APPLE__)
namespace ork::lev2 {
extern bool _macosUseHIDPI;
}
#endif

static auto the_synth = synth::instance();

SingularityTestApp::SingularityTestApp(int& argc, char** argv)
    : OrkEzQtApp(argc, argv) {
  _hudvp = the_synth->_hudvp;
  startupAudio();
}
SingularityTestApp::~SingularityTestApp() {
  tearDownAudio();
}
std::string testpatternname = "";
std::string testprogramname = "";

singularitytestapp_ptr_t createEZapp(int& argc, char** argv) {

  po::options_description desc("Allowed options");
  desc.add_options()                                                //
      ("help", "produce help message")                              //
      ("test", po::value<std::string>(), "test name (list,vo,nvo)") //
      ("program", po::value<std::string>(), "program name")         //
      ("hidpi", "hidpi mode");

  po::variables_map vars;
  po::store(po::parse_command_line(argc, argv, desc), vars);
  po::notify(vars);

  if (vars.count("help")) {
    std::cout << desc << "\n";
    exit(0);
  }
  if (vars.count("test")) {
    testpatternname = vars["test"].as<std::string>();
  }
  if (vars.count("program")) {
    testprogramname = vars["program"].as<std::string>();
  }
  if (vars.count("hidpi")) {
#if defined(__APPLE__)
    ork::lev2::_macosUseHIDPI = true;
#endif
  }

  //////////////////////////////////////////////////////////////////////////////
  // boot up debug HUD
  //////////////////////////////////////////////////////////////////////////////
  static auto& qti = qtinit(argc, argv);
  QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
  auto qtapp  = std::make_shared<SingularityTestApp>(qti._argc, qti._argvp);
  auto qtwin  = qtapp->_mainWindow;
  auto gfxwin = qtwin->_gfxwin;
  //////////////////////////////////////////////////////////
  // create references to various items scoped by qtapp
  //////////////////////////////////////////////////////////
  auto renderer = qtapp->_vars.makeSharedForKey<DefaultRenderer>("renderer");
  auto lmd      = qtapp->_vars.makeSharedForKey<LightManagerData>("lmgrdata");
  auto lightmgr = qtapp->_vars.makeSharedForKey<LightManager>("lmgr", *lmd);
  auto compdata = qtapp->_vars.makeSharedForKey<CompositingData>("compdata");
  auto material = qtapp->_vars.makeSharedForKey<FreestyleMaterial>("material");
  auto CPD      = qtapp->_vars.makeSharedForKey<CompositingPassData>("CPD");
  auto cameras  = qtapp->_vars.makeSharedForKey<CameraDataLut>("cameras");
  auto camdata  = qtapp->_vars.makeSharedForKey<CameraData>("camdata");
  //////////////////////////////////////////////////////////
  compdata->presetForward();
  compdata->mbEnable  = true;
  auto nodetek        = compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1"_pool, "item1"_pool);
  auto outpnode       = nodetek->tryOutputNodeAs<RtGroupOutputCompositingNode>();
  auto compositorimpl = compdata->createImpl();
  compositorimpl->bindLighting(lightmgr.get());
  CPD->addStandardLayers();
  cameras->AddSorted("spawncam", camdata.get());
  //////////////////////////////////////////////////////////
  qtapp->onGpuInit([=](Context* ctx) {
    renderer->setContext(ctx);
    const FxShaderTechnique* fxtechnique = nullptr;
    const FxShaderParam* fxparameterMVP  = nullptr;
    const FxShaderParam* fxparameterMODC = nullptr;
    material->gpuInit(ctx, "orkshader://solid");
    fxtechnique     = material->technique("mmodcolor");
    fxparameterMVP  = material->param("MatMVP");
    fxparameterMODC = material->param("modcolor");
    deco::printf(fvec3::White(), "gpuINIT - context<%p>\n", ctx, fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxtechnique<%p>\n", fxtechnique);
    deco::printf(fvec3::Yellow(), "  fxparameterMVP<%p>\n", fxparameterMVP);
    deco::printf(fvec3::Yellow(), "  fxparameterMODC<%p>\n", fxparameterMODC);
  });
  //////////////////////////////////////////////////////////
  qtapp->onUpdate([=](ui::updatedata_ptr_t updata) {
    ///////////////////////////////////////
    auto DB = DrawableBuffer::acquireForWrite(0);
    DB->Reset();
    DB->copyCameras(*cameras);
    qtapp->_hudvp->onUpdateThreadTick(updata);
    DrawableBuffer::releaseFromWrite(DB);
  });
  //////////////////////////////////////////////////////////
  qtapp->onDraw([=](ui::drawevent_constptr_t drwev) {
    ////////////////////////////////////////////////
    auto DB = DrawableBuffer::acquireForRead(7);
    if (nullptr == DB)
      return;
    ////////////////////////////////////////////////
    auto context = drwev->GetTarget();
    auto fbi     = context->FBI();  // FrameBufferInterface
    auto fxi     = context->FXI();  // FX Interface
    auto mtxi    = context->MTXI(); // FX Interface
    fbi->SetClearColor(fvec4(0.0, 0.0, 0.1, 1));
    ////////////////////////////////////////////////////
    // draw the synth HUD
    ////////////////////////////////////////////////////
    RenderContextFrameData RCFD(context); // renderer per/frame data
    RCFD._cimpl = compositorimpl;
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    context->pushRenderContextFrameData(&RCFD);
    lev2::UiViewportRenderTarget rt(nullptr);
    auto tgtrect        = context->mainSurfaceRectAtOrigin();
    CPD->_irendertarget = &rt;
    CPD->SetDstRect(tgtrect);
    compositorimpl->pushCPD(*CPD);
    context->beginFrame();
    mtxi->PushUIMatrix();
    qtapp->_hudvp->Draw(drwev);
    mtxi->PopUIMatrix();
    context->endFrame();
    ////////////////////////////////////////////////////
    DrawableBuffer::releaseFromRead(DB);
  });
  //////////////////////////////////////////////////////////
  qtapp->onResize([=](int w, int h) { //
    // printf("GOTRESIZE<%d %d>\n", w, h);
    qtapp->_hudvp->SetSize(w, h);
  });
  //////////////////////////////////////////////////////////
  const int64_t trackMAX = (4095 << 16);
  qtapp->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult {
    bool isalt  = ev->mbALT;
    bool isctrl = ev->mbCTRL;
    switch (ev->miEventCode) {
      case ui::UIEV_KEY:
      case ui::UIEV_KEY_REPEAT:
        switch (ev->miKeyCode) {
          case 'p':
            the_synth->_hudpage = (the_synth->_hudpage + 1) % 2;
            break;
          case '-': {
            int64_t amt         = isalt ? 100 : (isctrl ? 1 : 10);
            the_synth->_oswidth = std::clamp(the_synth->_oswidth - amt, int64_t(0), int64_t(4095));
            break;
          }
          case '=': {
            int64_t amt         = isalt ? 100 : (isctrl ? 1 : 10);
            the_synth->_oswidth = std::clamp(the_synth->_oswidth + amt, int64_t(0), int64_t(4095));
            break;
          }
          case '[': {
            float amt             = isalt ? 0.1 : (isctrl ? 0.001 : 0.01);
            the_synth->_ostriglev = std::clamp(the_synth->_ostriglev - amt, -1.0f, 1.0f);
            break;
          }
          case ']': {
            float amt             = isalt ? 0.1 : (isctrl ? 0.001 : 0.01);
            the_synth->_ostriglev = std::clamp(the_synth->_ostriglev + amt, -1.0f, 1.0f);
            break;
          }
          case '\\': {
            the_synth->_ostrigdir = !the_synth->_ostrigdir;
            break;
          }
          case '\'': {
            the_synth->_osgainmode++;
            break;
          }
          default:
            break;
        }
        break;
      default:
        return qtapp->_hudvp->HandleUiEvent(ev);
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  return qtapp;
}
//////////////////////////////////////////////////////////////////////////////
singularitybenchapp_ptr_t createBenchmarkApp(int& argc, char** argv, prgdata_constptr_t program) {
  //////////////////////////////////////////////////////////////////////////////
  // benchmark
  //////////////////////////////////////////////////////////////////////////////
  constexpr size_t histosize = 65536;
  /////////////////////////////////////////
  auto app = std::make_shared<SingularityBenchMarkApp>(argc, argv);
  //////////////////////////////////////////////////////////////////////////////
  app->onGpuInit([=](Context* ctx) { //
    app->_material = std::make_shared<ork::lev2::FreestyleMaterial>();
    app->_material->gpuInit(ctx, "orkshader://ui2");
    app->_fxtechniqueMODC = app->_material->technique("ui_modcolor");
    app->_fxtechniqueVTXC = app->_material->technique("ui_vtxcolor");
    app->_fxparameterMVP  = app->_material->param("mvp");
    app->_fxparameterMODC = app->_material->param("modcolor");
    app->_timer.Start();
    memset(app->_inpbuf, 0, SingularityBenchMarkApp::KNUMFRAMES * 2 * sizeof(float));
    app->_cur_time  = app->_timer.SecsSinceStart();
    app->_prev_time = app->_cur_time;
    app->_time_histogram.resize(histosize);
    app->_font  = lev2::FontMan::GetFont("i14");
    app->_charw = app->_font->GetFontDesc().miAdvanceWidth;
    app->_charh = app->_font->GetFontDesc().miAdvanceHeight;
  });
  //////////////////////////////////////////////////////////////////////////////
  app->onUpdate([=](ui::updatedata_ptr_t updata) {
    const auto& obuf = the_synth->_obuf;
    /////////////////////////////////////////
    double upd_time = 0.0;
    bool done       = false;
    while (done == false) {

      int numcurvoices = the_synth->_numactivevoices.load();
      if (numcurvoices < int(app->_maxvoices)) {
        int irand = rand() & 0xffff;
        if (irand < 32768)
          enqueue_audio_event(program, 0.0f, 2.5, 48);
      }
      the_synth->compute(SingularityBenchMarkApp::KNUMFRAMES, app->_inpbuf);
      app->_cur_time   = app->_timer.SecsSinceStart();
      double iter_time = app->_cur_time - app->_prev_time;
      upd_time += iter_time;
      ///////////////////////////////////////////
      // histogram for looking for timing spikes
      ///////////////////////////////////////////
      size_t index = size_t(double(histosize) * iter_time / 0.02);
      index        = std::clamp(index, size_t(0), histosize - 1);
      app->_time_histogram[index]++;
      ///////////////////////////////////////////
      app->_prev_time = app->_cur_time;
      app->_numiters++;

      app->_accumnumvoices += app->_maxvoices;

      done = upd_time > 1.0 / 60.0;
    }
  });
  //////////////////////////////////////////////////////////////////////////////
  app->onDraw([=](ui::drawevent_constptr_t drwev) { //
    ////////////////////////////////////////////////
    auto context = drwev->GetTarget();
    auto fbi     = context->FBI();  // FrameBufferInterface
    auto fxi     = context->FXI();  // FX Interface
    auto mtxi    = context->MTXI(); // FX Interface
    fbi->SetClearColor(fvec4(0.1, 0.1, 0.1, 1));
    ////////////////////////////////////////////////////
    // draw the synth HUD
    ////////////////////////////////////////////////////
    RenderContextFrameData RCFD(context); // renderer per/frame data
    context->beginFrame();
    auto tgtrect = context->mainSurfaceRectAtOrigin();
    auto uimtx   = mtxi->uiMatrix(tgtrect._w, tgtrect._h);
    app->_material->begin(app->_fxtechniqueMODC, RCFD);
    app->_material->bindParamMatrix(app->_fxparameterMVP, uimtx);
    {
      //////////////////////////////////////////////
      // draw background
      //////////////////////////////////////////////
      auto& primi = lev2::GfxPrimitives::GetRef();
      app->_material->bindParamVec4(app->_fxparameterMODC, fvec4(0, 0, 0, 1));
      primi.RenderEMLQuadAtZV16T16C16(
          context,
          8,              // x0
          tgtrect._w - 8, // x1
          8,              // y0
          tgtrect._h - 8, // y1
          0.0f,           // z
          0.0f,           // u0
          1.0f,           // u1
          0.0f,           // v0
          1.0f            // v1
      );
    }
    app->_material->end(RCFD);
    //////////////////////////////////////////////
    app->_material->begin(app->_fxtechniqueVTXC, RCFD);
    {
      app->_material->bindParamMatrix(app->_fxparameterMVP, uimtx);
      //////////////////////////////////////////////
      double desired_blockperiod = 1000.0 / (48000.0 / double(SingularityBenchMarkApp::KNUMFRAMES));
      //////////////////////////////////////////////
      // compute histogram vertical extents
      //////////////////////////////////////////////
      int maxval       = 0;
      int minbin       = 0;
      int maxbin       = 0;
      double avgbin    = 0.0;
      double avgdiv    = 0.0;
      int numunderruns = 0;
      for (size_t i = 0; i < histosize; i++) {
        int item = app->_time_histogram[i];
        if (item > maxval) {
          maxval = item;
        }
        if (minbin == 0 and item != 0) {
          minbin = i;
        }
        if (item != 0) {
          maxbin = i;
        }
        avgbin += double(i) * double(item);
        avgdiv += double(item);
        double bin_time = 1000.0 * 0.02 * double(i) / double(histosize);
        if (bin_time > desired_blockperiod) {
          numunderruns += item;
        }
      }
      avgbin /= avgdiv;

      if (numunderruns <= app->_numunderruns) {
        app->_maxvoices += 0.1;
      } else {
        app->_maxvoices -= 2.5;
      }
      app->_numunderruns = numunderruns;

      //////////////////////////////////////////////
      // draw histogram
      //////////////////////////////////////////////
      app->_material->bindParamVec4(app->_fxparameterMODC, fvec4(0, 1, 0, 1));
      size_t numlines                      = histosize;
      lev2::DynamicVertexBuffer<vtx_t>& VB = lev2::GfxEnv::GetSharedDynamicV16T16C16();
      lev2::VtxWriter<vtx_t> vw;
      vw.Lock(context, &VB, numlines * 2);
      for (size_t index_r = 1; index_r < histosize; index_r++) {
        size_t index_l = index_r - 1;
        int item_l     = app->_time_histogram[index_l];
        int item_r     = app->_time_histogram[index_r];

        double time_l = 20.0 * double(index_l) / double(histosize);

        double xl = 8.0 + double(tgtrect._w - 16.0) * double(index_l) / double(histosize);
        double xr = 8.0 + double(tgtrect._w - 16.0) * double(index_r) / double(histosize);
        double yl = 8.0 + double(tgtrect._h - 16.0) * double(item_l) / double(maxval);
        double yr = 8.0 + double(tgtrect._h - 16.0) * double(item_r) / double(maxval);

        fvec4 color = time_l < desired_blockperiod //
                          ? fvec4(0, 1, 0, 1)
                          : fvec4(1, 0, 0, 1);

        vw.AddVertex(vtx_t(fvec4(xl, tgtrect._h - yl, 0.0), fvec4(), color));
        vw.AddVertex(vtx_t(fvec4(xr, tgtrect._h - yr, 0.0), fvec4(), color));
      }
      vw.UnLock(context);
      context->GBI()->DrawPrimitiveEML(vw, EPrimitiveType::LINES);
      //////////////////////////////////////////////
      // bin_time = 20 * i / histosize
      // bin_time/i = 20/histosize
      // i/bin_time = histosize/20
      // i=histosize/(20*bin_time)
      double desi = desired_blockperiod / 20.0;
      double desx = 8.0 + double(tgtrect._w - 16.0) * desi;
      lev2::VtxWriter<vtx_t> vw2;
      vw2.Lock(context, &VB, 2);
      vw2.AddVertex(vtx_t(fvec4(desx, 0, 0.0), fvec4(), fvec4(1, 1, 0, 0)));
      vw2.AddVertex(vtx_t(fvec4(desx, tgtrect._h, 0.0), fvec4(), fvec4(1, 1, 0, 0)));
      vw2.UnLock(context);
      context->GBI()->DrawPrimitiveEML(vw2, EPrimitiveType::LINES);
      //////////////////////////////////////////////
      // draw text
      //////////////////////////////////////////////
      {
        context->MTXI()->PushUIMatrix(tgtrect._w, tgtrect._h);
        lev2::FontMan::PushFont(app->_font);
        { //
          int y = 0;

          std::string str[8];

          double avgnumvoices = app->_accumnumvoices / double(app->_numiters);

          context->PushModColor(fcolor4::White());
          lev2::FontMan::beginTextBlock(context);
          str[0] = "     Synth Compute Timing Histogram";
          str[1] = FormatString("Program<%s>", program->_name.c_str());
          str[2] = FormatString("NumIters<%d>", app->_numiters);
          str[3] = FormatString("NumActiveVoices<%d>", the_synth->_numactivevoices.load());
          str[4] = FormatString("AvgActiveVoices<%d>", int(avgnumvoices));
          lev2::FontMan::DrawText(context, 32, y += 32, str[0].c_str());
          lev2::FontMan::DrawText(context, 32, y += 32, str[1].c_str());
          lev2::FontMan::DrawText(context, 32, y += 16, str[2].c_str());
          lev2::FontMan::DrawText(context, 32, y += 16, str[3].c_str());
          lev2::FontMan::DrawText(context, 32, y += 16, str[4].c_str());
          lev2::FontMan::endTextBlock(context);
          context->PopModColor();

          double minbin_time = 0.02 * double(minbin) / double(histosize);
          double maxbin_time = 0.02 * double(maxbin) / double(histosize);
          double avgbin_time = 0.02 * avgbin / double(histosize);
          str[0]             = FormatString("Min IterTime <%g msec>", minbin_time * 1000.0);
          str[1]             = FormatString("Max IterTime <%g msec>", maxbin_time * 1000.0);
          str[2]             = FormatString("Avg IterTime <%g msec>", avgbin_time * 1000.0);

          context->PushModColor(fcolor4::Green());
          lev2::FontMan::beginTextBlock(context);
          lev2::FontMan::DrawText(context, 32, y += 16, str[0].c_str());
          lev2::FontMan::DrawText(context, 32, y += 16, str[1].c_str());
          lev2::FontMan::DrawText(context, 32, y += 16, str[2].c_str());
          lev2::FontMan::endTextBlock(context);
          context->PopModColor();

          context->PushModColor(fcolor4::Yellow());
          lev2::FontMan::beginTextBlock(context);
          str[0] = FormatString("Desired Blockperiod @ 48KhZ <%g msec>", desired_blockperiod);
          lev2::FontMan::DrawText(context, 32, y += 16, str[0].c_str());
          lev2::FontMan::endTextBlock(context);
          context->PopModColor();

          context->PushModColor(fcolor4::Red());
          lev2::FontMan::beginTextBlock(context);
          str[0] = FormatString("Number of Underruns <%d>", numunderruns);
          lev2::FontMan::DrawText(context, 32, y += 16, str[0].c_str());
          lev2::FontMan::endTextBlock(context);
          context->PopModColor();
        }
        lev2::FontMan::PopFont();
        context->MTXI()->PopUIMatrix();
      }
    }
    app->_material->end(RCFD);
    context->endFrame();
  });
  /////////////////////////////////////////
  app->setRefreshPolicy({EREFRESH_FIXEDFPS, 60});
  return app;
}

Sequence::Sequence(float tempo)
    : _tempo(tempo) {
}
float Sequence::mbs2time(int meas, int sixteenth, int clocks) const {
  float timepermeasure = 60.0 * 4.0 / _tempo;
  float out_time       = float(meas) * timepermeasure;
  out_time += float(sixteenth) * timepermeasure / 16.0f;
  out_time += float(clocks) * timepermeasure / 256.0f;
  return out_time;
}
void Sequence::addNote(
    int meas, //
    int sixteenth,
    int clocks,
    int note,
    int vel,
    int dur) {
  Event out;
  out._time = mbs2time(meas, sixteenth, clocks);
  out._note = note;
  out._vel  = vel;
  out._dur  = mbs2time(0, 0, dur);
  _events.push_back(out);
}
void Sequence::enqueue(prgdata_constptr_t program) {
  for (auto e : _events) {
    enqueue_audio_event(program, e._time, e._dur, e._note, e._vel);
  }
}
void seq1(float tempo, int basebar, prgdata_constptr_t program) {
  Sequence sq(tempo);
  for (int baro = 0; baro < 4; baro += 2) {
    int dur = 8 + (baro >> 1) * 8;
    int bar = basebar + baro;
    sq.addNote(bar + 0, 0, 0, 48, 72, dur);
    sq.addNote(bar + 0, 4, 0, 48, 72, dur);
    sq.addNote(bar + 0, 8, 0, 48, 72, dur);
    sq.addNote(bar + 0, 12, 0, 48, 96, dur * 2);
    sq.addNote(bar + 0, 12, 2, 48 + 7, 112, dur * 32);
    sq.addNote(bar + 1, 0, 0, 48, 32, dur);
    sq.addNote(bar + 1, 2, 0, 48, 76, dur);
    sq.addNote(bar + 1, 4, 0, 48, 80, dur);
    sq.addNote(bar + 1, 6, 1, 48, 48, dur);
    sq.addNote(bar + 1, 8, 0, 48, 92, dur);
    sq.addNote(bar + 1, 10, 0, 48, 100, dur);
    sq.addNote(bar + 1, 12, 0, 48, 110, dur);
    sq.addNote(bar + 1, 12, 1, 48 + 8, 127, dur * 3);
    sq.addNote(bar + 1, 14, 3, 48 + 12, 127, dur * 2);
    sq.addNote(bar + 1, 14, -1, 48, 127);
    sq.addNote(bar + 1, 16, -2, 48 + 15, 127);
  }
  sq.enqueue(program);
}

prgdata_constptr_t testpattern(syndata_ptr_t syndat, int argc, char** argv) {

  auto program = syndat->getProgramByName(testprogramname);

  int count = 0;

  if (testpatternname == "list") {
    for (auto item : syndat->_bankdata->_programs) {
      int id    = item.first;
      auto prog = item.second;
      printf("program<%d:%s>\n", id, prog->_name.c_str());
    }
    return nullptr;
  } else if (testpatternname == "sq1") {
    seq1(120.0f, 0, program);
    seq1(120.0f, 4, program);
    seq1(120.0f, 8, program);
    seq1(120.0f, 12, program);
  } else if (testpatternname == "slow") {
    for (int i = 0; i < 12; i++) {        // note length
      for (int n = 0; n <= 64; n += 12) { // note
        enqueue_audio_event(program, count * 3.0, 2.0, 36 + n, 128);
        count++;
      }
    }
  } else if (testpatternname == "vo") {
    for (int i = 0; i < 12; i++) {                             // note length
      for (int velocity = 0; velocity <= 128; velocity += 8) { // velocity
        for (int n = 0; n <= 64; n += 12) {                    // note
          // printf("getProgramByName<%s>\n", program->_name.c_str());
          enqueue_audio_event(program, count * 0.15, (i + 1) * 0.05, 36 + n, velocity);
          count++;
        }
      }
    }
  } else if (testpatternname == "vo2") {
    for (int i = 0; i < 12; i++) {                               // note length
      for (int n = 0; n <= 64; n += 12) {                        // note
        for (int velocity = 0; velocity <= 128; velocity += 8) { // velocity
          // printf("getProgramByName<%s>\n", program->_name.c_str());
          enqueue_audio_event(program, count * 0.15, (i + 1) * 0.05, 36 + n, velocity);
          count++;
        }
      }
    }
  } else if (testpatternname == "vo3") {
    for (int i = 0; i < 12; i++) {                               // note length
      for (int n = 0; n <= 64; n += 12) {                        // note
        for (int velocity = 0; velocity <= 128; velocity += 8) { // velocity
          // printf("getProgramByName<%s>\n", program->_name.c_str());
          enqueue_audio_event(program, count * 0.15, (velocity / 128.0f) * 0.33, 36 + n, velocity);
          count++;
        }
      }
    }
  } else if (testpatternname == "nvo") {
    for (int i = 0; i < 12; i++) { // 2 32 patch banks
      for (int velocity = 0; velocity <= 128; velocity += 8) {
        for (int n = 0; n <= 64; n += 12) {
          // printf("getProgramByName<%s>\n", program->_name.c_str());
          enqueue_audio_event(program, count * 0.20, (i + 1) * 0.05, 36 + n + i, velocity);
          count++;
        }
      }
    }
  } else {
    for (int velocity = 0; velocity <= 128; velocity += 8) {
      enqueue_audio_event(program, count * 0.25, 0.1, 48, velocity);
      count++;
    }
  }
  return program;
}
