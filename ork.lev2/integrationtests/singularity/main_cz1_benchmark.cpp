#include <map>
#include "harness.h"
#include <ork/kernel/timer.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>

using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto the_synth = synth::instance();
  double SR      = getSampleRate();
  the_synth->setSampleRate(SR);
  the_synth->_masterGain = 0.5f;
  //////////////////////////////////////////////////////////////////////////////
  // allocate program/layer data
  //////////////////////////////////////////////////////////////////////////////
  auto program   = std::make_shared<ProgramData>();
  auto layerdata = program->newLayer();
  auto czoscdata = std::make_shared<CzOscData>();
  program->_role = "czx";
  program->_name = "test";
  //////////////////////////////////////
  // setup dsp graph
  //////////////////////////////////////
  layerdata->_algdata = configureCz1Algorithm(layerdata, 1);
  auto dcostage       = layerdata->stageByName("DCO");
  auto ampstage       = layerdata->stageByName("AMP");
  auto osc            = dcostage->appendTypedBlock<CZX>(czoscdata, 0);
  auto amp            = ampstage->appendTypedBlock<AMP_MONOIO>();
  //////////////////////////////////////
  // setup modulators
  //////////////////////////////////////
  auto DCAENV = layerdata->appendController<RateLevelEnvData>("DCAENV");
  auto DCWENV = layerdata->appendController<RateLevelEnvData>("DCWENV");
  auto LFO2   = layerdata->appendController<LfoData>("MYLFO2");
  auto LFO1   = layerdata->appendController<LfoData>("MYLFO1");
  //////////////////////////////////////
  // setup envelope
  //////////////////////////////////////
  DCAENV->_ampenv = true;
  DCAENV->addSegment("seg0", .2, .7);
  DCAENV->addSegment("seg1", .2, .7);
  DCAENV->addSegment("seg2", 1, 1);
  DCAENV->addSegment("seg3", 1, .3);
  DCAENV->addSegment("seg4", 1, 0);
  //
  DCWENV->_ampenv = false;
  DCWENV->addSegment("seg0", 0.1, .7);
  DCWENV->addSegment("seg1", 1, 1);
  DCWENV->addSegment("seg2", 2, .5);
  DCWENV->addSegment("seg3", 2, 1);
  DCWENV->addSegment("seg4", 2, 1);
  DCWENV->addSegment("seg5", 1, 1);
  DCWENV->addSegment("seg6", 1, 0);
  //////////////////////////////////////
  // setup LFO
  //////////////////////////////////////
  LFO1->_minRate = 0.25;
  LFO1->_maxRate = 0.25;
  LFO1->_shape   = "Sine";
  LFO2->_minRate = 3.3;
  LFO2->_maxRate = 3.3;
  LFO2->_shape   = "Sine";
  //////////////////////////////////////
  // setup modulation routing
  //////////////////////////////////////
  auto& modulation_index_param      = osc->_paramd[0]._mods;
  modulation_index_param._src1      = DCWENV;
  modulation_index_param._src1Depth = 1.0;
  // modulation_index_param._src2      = LFO1;
  // modulation_index_param._src2DepthCtrl = LFO2;
  modulation_index_param._src2MinDepth = 0.5;
  modulation_index_param._src2MaxDepth = 0.1;
  //////////////////////////////////////
  czoscdata->_dcoBaseWaveA = 6;
  czoscdata->_dcoBaseWaveB = 7;
  czoscdata->_dcoWindow    = 2;
  //////////////////////////////////////
  auto& amp_param   = amp->_paramd[0];
  amp_param._coarse = 0.0f;
  amp_param.useDefaultEvaluator();
  amp_param._mods._src1      = DCAENV;
  amp_param._mods._src1Depth = 1.0;
  //////////////////////////////////////////////////////////////////////////////
  // benchmark
  //////////////////////////////////////////////////////////////////////////////
  constexpr size_t histosize = 65536;
  /////////////////////////////////////////
  static constexpr size_t KNUMFRAMES = 512;
  struct SingularityBenchMarkApp final : public OrkEzQtApp {
    SingularityBenchMarkApp(int& argc, char** argv)
        : OrkEzQtApp(argc, argv) {
    }
    ~SingularityBenchMarkApp() override {
    }
    std::vector<int> _time_histogram;
    ork::lev2::freestyle_mtl_ptr_t _material;
    const FxShaderTechnique* _fxtechniqueMODC = nullptr;
    const FxShaderTechnique* _fxtechniqueVTXC = nullptr;
    const FxShaderParam* _fxparameterMVP      = nullptr;
    const FxShaderParam* _fxparameterMODC     = nullptr;
    ork::Timer _timer;
    float _inpbuf[KNUMFRAMES * 2];
    int _numiters     = 0;
    double _cur_time  = 0.0;
    double _prev_time = 0.0;
    lev2::Font* _font;
    int _charw             = 0;
    int _charh             = 0;
    double _underrunrate   = 0;
    int _numunderruns      = 0;
    double _maxvoices      = 4.0;
    double _accumnumvoices = 0.0;
  };
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
    memset(app->_inpbuf, 0, KNUMFRAMES * 2 * sizeof(float));
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
          enqueue_audio_event(program, 0.0f, 2.0, 48);
      }
      the_synth->compute(KNUMFRAMES, app->_inpbuf);
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
      double desired_blockperiod = 1000.0 / (48000.0 / double(KNUMFRAMES));
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
        app->_maxvoices += 0.25;
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
          str[1] = FormatString("NumIters<%d>", app->_numiters);
          str[2] = FormatString("NumActiveVoices<%d>", the_synth->_numactivevoices.load());
          str[3] = FormatString("AvgActiveVoices<%d>", int(avgnumvoices));
          lev2::FontMan::DrawText(context, 32, y += 32, str[0].c_str());
          lev2::FontMan::DrawText(context, 32, y += 32, str[1].c_str());
          lev2::FontMan::DrawText(context, 32, y += 16, str[2].c_str());
          lev2::FontMan::DrawText(context, 32, y += 16, str[3].c_str());
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
  return app->exec();
}
