////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "harness.h"
#include <boost/program_options.hpp>
#include <iostream>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/hud.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorPicking.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScaleBias.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorScreen.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorVr.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/OutputNodeRtGroup.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/ui/layoutgroup.inl>
#include <ork/lev2/gfx/gfxvtxbuf.inl>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_deferred.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_node_forward.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/unlit_node.h>
#include <ork/util/logger.h>
///////////////////////////////////////////////////////////////////////////////
namespace po = boost::program_options;
static logchannel_ptr_t logchan_harness = logger()->createChannel("singul.harness", fvec3(1, 0.6, .8), true);
///////////////////////////////////////////////////////////////////////////////
#if defined(__APPLE__)
namespace ork::lev2 {
extern bool _macosUseHIDPI;
}
#endif
///////////////////////////////////////////////////////////////////////////////
audiodevice_ptr_t gaudiodevice;
///////////////////////////////////////////////////////////////////////////////
SingularityTestApp::SingularityTestApp(appinitdata_ptr_t initdata)
    // TODO - get init data with lev2 enabled...
    : OrkEzApp(initdata) {
  gaudiodevice = AudioDevice::instance();
  // startupAudio();
}
///////////////////////////////////////////////////////////////////////////////
SingularityTestApp::~SingularityTestApp() {
  // tearDownAudio();
}
///////////////////////////////////////////////////////////////////////////////
std::string testpatternname = "";
std::string testprogramname = "";
std::string midiportname    = "";
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
singularitytestapp_ptr_t createEZapp(appinitdata_ptr_t init_data) {

  lev2::initModule(init_data);

  auto desc = init_data->commandLineOptions("Singularity Options");
  desc->add_options()                                               //
      ("help", "produce help message")                              //
      ("test", po::value<std::string>(), "test name (list,vo,nvo)") //
      ("port", po::value<std::string>(), "midiport name (list)")    //
      ("program", po::value<std::string>(), "program name")         //
      ("hidpi", "hidpi mode");

  auto& vars = *init_data->parse();

  if (vars.count("help")) {
    std::cout << desc << "\n";
    exit(0);
  }
  if (vars.count("port")) {
    midiportname = vars["port"].as<std::string>();
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
  auto ezapp  = std::make_shared<SingularityTestApp>(init_data);
  auto ezwin  = ezapp->_mainWindow;
  auto appwin = ezwin->_appwin;
  auto uicontext                  = ezapp->_uicontext;
  appwin->_rootWidget->_uicontext = uicontext.get();
  //////////////////////////////////////////////////////////
  // a wee bit convoluted, TODO: fixme
  auto hudvplayout       = ezapp->_topLayoutGroup->makeChild<HudLayoutGroup>(); //here (name==1)
  ezapp->_hudvp = hudvplayout.typedWidget();
  synth::instance()->_hudvp = hudvplayout.typedWidget();

  ezapp->_topLayoutGroup->_layout->dump();

  ezapp->_topLayoutGroup->dumpTopology();
  ezapp->_eztopwidget->enableUiDraw();

  //OrkAssert(false);

  //////////////////////////////////////////////////////////
  // create references to various items scoped by ezapp
  //////////////////////////////////////////////////////////
  auto renderer = ezapp->_vars->makeSharedForKey<IRenderer>("renderer");
  auto lmd      = ezapp->_vars->makeSharedForKey<LightManagerData>("lmgrdata");
  auto lightmgr = ezapp->_vars->makeSharedForKey<LightManager>("lmgr", lmd);
  auto compdata = ezapp->_vars->makeSharedForKey<CompositingData>("compdata");
  auto material = ezapp->_vars->makeSharedForKey<FreestyleMaterial>("material");
  auto CPD      = ezapp->_vars->makeSharedForKey<CompositingPassData>("CPD");
  auto cameras  = ezapp->_vars->makeSharedForKey<CameraDataLut>("cameras");
  auto camdata  = ezapp->_vars->makeSharedForKey<CameraData>("camdata");
  //////////////////////////////////////////////////////////
  compdata->presetUnlit();
  compdata->mbEnable  = true;
  auto nodetek        = compdata->tryNodeTechnique<NodeCompositingTechnique>("scene1", "item1");
  auto outpnode       = nodetek->tryOutputNodeAs<RtGroupOutputCompositingNode>();
  auto compositorimpl = compdata->createImpl();
  compositorimpl->bindLighting(lightmgr.get());
  CPD->addStandardLayers();
  (*cameras)["spawncam"] = camdata;
  //////////////////////////////////////////////////////////
  ezapp->onGpuInit([=](Context* ctx) {
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
    auto dbufcontext = ezapp->_vars->makeSharedForKey<DrawBufContext>("dbufcontext");
    ezapp->onUpdate([=](ui::updatedata_ptr_t updata) {
    ///////////////////////////////////////
    auto DB = dbufcontext->acquireForWriteLocked();
    if(DB){
      DB->Reset();
      DB->copyCameras(*cameras);
      ezapp->_hudvp->onUpdateThreadTick(updata);
     dbufcontext->releaseFromWriteLocked(DB);
    }
  });
  //////////////////////////////////////////////////////////
  ezapp->onDraw([=](ui::drawevent_constptr_t drwev) {
    ////////////////////////////////////////////////
    auto DB = dbufcontext->acquireForReadLocked();
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
    RCFD.pushCompositor(compositorimpl);
    RCFD.setUserProperty("DB"_crc, lev2::rendervar_t(DB));
    context->pushRenderContextFrameData(&RCFD);
    lev2::UiViewportRenderTarget rt(nullptr);
    auto tgtrect        = context->mainSurfaceRectAtOrigin();
    CPD->_irendertarget = &rt;
    CPD->SetDstRect(tgtrect);
    compositorimpl->pushCPD(*CPD);
    context->beginFrame();
    mtxi->PushUIMatrix();
    ezapp->_uicontext->draw(drwev);
    mtxi->PopUIMatrix();
    context->endFrame();
    ////////////////////////////////////////////////////
    dbufcontext->releaseFromReadLocked(DB);
  });
  //////////////////////////////////////////////////////////
  ezapp->onResize([=](int w, int h) { //
                                      // printf("GOTRESIZE<%d %d>\n", w, h);
                                      // ezapp->_eztopwidget->_topLayoutGroup->SetSize(w, h);
    ezapp->_eztopwidget->SetSize(w, h);
    ezapp->_hudvp->SetSize(w,h);
    ezapp->_eztopwidget->_topLayoutGroup->SetSize(w, h);
  });
  //////////////////////////////////////////////////////////
  const int64_t trackMAX = (4095 << 16);
  ezapp->onUiEvent([=](ui::event_constptr_t ev) -> ui::HandlerResult {
    bool isalt  = ev->mbALT;
    bool isctrl = ev->mbCTRL;
    switch (ev->_eventcode) {
      case ui::EventCode::KEY_DOWN:
      case ui::EventCode::KEY_REPEAT:
        switch (ev->miKeyCode) {
          case 'p':
            synth::instance()->_hudpage = (synth::instance()->_hudpage + 1) % 2;
            break;
          default:
            break;
        }
        break;
      default:
        return ezapp->_hudvp->handleUiEvent(ev);
        break;
    }
    ui::HandlerResult rval;
    return rval;
  });
  return ezapp;
}
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
singularitybenchapp_ptr_t createBenchmarkApp(appinitdata_ptr_t initdata, prgdata_constptr_t program) {
  //////////////////////////////////////////////////////////////////////////////
  // benchmark
  //////////////////////////////////////////////////////////////////////////////
  constexpr size_t histosize = 65536;
  /////////////////////////////////////////
  auto uicontext                  = std::make_shared<ui::Context>();
  auto app                        = std::make_shared<SingularityBenchMarkApp>(initdata);
  auto ezwin                      = app->_mainWindow;
  auto appwin                     = ezwin->_appwin;
  appwin->_rootWidget->_uicontext = uicontext.get();
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
    const auto& obuf = synth::instance()->_obuf;
    /////////////////////////////////////////
    double upd_time = 0.0;
    bool done       = false;
    while (done == false) {

      int numcurvoices = synth::instance()->_numactivevoices.load();
      if (numcurvoices < int(app->_maxvoices)) {
        int irand = rand() & 0xffff;
        if (irand < 32768)
          enqueue_audio_event(program, 0.0f, 2.5, 48);
      }
      synth::instance()->compute(SingularityBenchMarkApp::KNUMFRAMES, app->_inpbuf);
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
      context->GBI()->DrawPrimitiveEML(vw, PrimitiveType::LINES);
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
      context->GBI()->DrawPrimitiveEML(vw2, PrimitiveType::LINES);
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
          str[3] = FormatString("NumActiveVoices<%d>", synth::instance()->_numactivevoices.load());
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

prgdata_constptr_t testpattern(
    syndata_ptr_t syndat, //
    int argc,
    char** argv) {

  auto midictx = midi::InputContext::instance();
  if (midiportname == "list") {
    for (auto portitem : midictx->_portmap) {
      printf("midiport<%d:%s>\n", portitem.second, portitem.first.c_str());
    }
    exit(0);
  }

  auto program = syndat->getProgramByName(testprogramname);

  int count = 0;

  if (testpatternname == "list") {
    for (auto item : syndat->_bankdata->_programs) {
      int id    = item.first;
      auto prog = item.second;
      printf("program<%d:%s>\n", id, prog->_name.c_str());
    }
    return nullptr;
  } else if (testpatternname == "none") {
    synth::instance()->_globalprog = program;
    return program;
  } else if (testpatternname == "midi") {
    int portindex = atoi(midiportname.c_str());
    midictx->startMidiInputByIndex(portindex, &mymidicallback);
    synth::instance()->_globalprog = program;
    return program;
  } else if (testpatternname == "sq1") {
    seq1(120.0f, 0, program);
    seq1(120.0f, 4, program);
    seq1(120.0f, 8, program);
    seq1(120.0f, 12, program);
  } else if (testpatternname == "slow") {
    for (int i = 0; i < 12; i++) {        // note length
      for (int n = 0; n <= 64; n += 12) { // note
        enqueue_audio_event( program, // prog
                             count * 3.0, // time
                             2.0, // duration
                             36 + n, // note
                             128); // vel
        count++;
      }
    }
  } else if (testpatternname == "pscan") {
    auto bankdata = syndat->_bankdata;
    int count = 0;
    for( auto program_item : bankdata->_programs ){
      auto program = program_item.second;
      logchan_harness->log("enqueuing seq1 for program<%s>", program->_name.c_str() );
      seq1(180.0f, count*4, program);
      count++;
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
          enqueue_audio_event(
              program,        // program
              count * 0.15,   // time
              (i + 1) * 0.05, // duration
              36 + n,         // midinote
              velocity);      // velocity
          count++;
        }
      }
    }
  } else if (testpatternname == "vo3") {
    for (int i = 0; i < 12; i++) {          // note length
      for (int n = 0; n <= 64; n += 12) {   // note
        for (int v = 0; v <= 128; v += 8) { // velocity
          // printf("getProgramByName<%s>\n", program->_name.c_str());
          enqueue_audio_event(
              program,             // program
              count * 0.5,        // time
              0.25,                // duration
              48 + n,              // midinote
              v);                  // velocity
          count++;
        }
      }
    }
  } else if (testpatternname == "nvo") {
    for (int i = 0; i < 12; i++) { // 2 32 patch banks
      for (int velocity = 0; velocity <= 128; velocity += 8) {
        for (int n = 0; n <= 64; n += 12) {
          // printf("getProgramByName<%s>\n", program->_name.c_str());
          enqueue_audio_event(program, count * 0.20, (i + 1) * 0.05, 48 + n + i, velocity);
          count++;
        }
      }
    }
  } else {
    for (int rep = 0; rep <= 16; rep++) {
      for (int velocity = 0; velocity <= 128; velocity += 8) {
        enqueue_audio_event(program, count * 0.25, 0.1, 48, velocity);
        count++;
      }
    }
  }
  return program;
}
