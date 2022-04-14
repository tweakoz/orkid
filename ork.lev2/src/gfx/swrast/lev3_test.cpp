////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/glfw/ctx_glfw.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/math/cvector4.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/gfx/texman.h>

#include "lev3_test.h"
#include "render_graph.h"
#include <math.h>

#if defined(ORK_OSX)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

extern GLuint gLastBoundNonZeroTex;

using namespace ork;
using namespace ork::lev2;

///////////////////////////////////////////////////////////////////////////////

class MyViewport : public ui::Viewport {

public:
  MyViewport(const std::string& name)
      : ui::Viewport(name, 1, 1, 1, 1, ork::fcolor3(0.0f, 0.0f, 0.0f), 1.0f)
      , gltex(0) {
  }

  ui::HandlerResult DoOnUiEvent(ui::event_constptr_t pEV) final // virtual
  {
    return ui::HandlerResult(this);
  }

  void Init(Context* pTARG) // virtual
  {
    mtl.gpuInit(pTARG);
    tex          = Texture::createBlank(512, 512, EBufferFormat::RGBA8);
    auto pu32    = (uint32_t*)tex->_data;
    uint32_t idx = 0;
    for (int iw = 0; iw < 512; iw++)
      for (int ih = 0; ih < 512; ih++) {
        pu32[idx++] = idx;
      }
    tex->_dirty = true;
  }
  void DoRePaintSurface(ui::drawevent_constptr_t ev) override {
    _target->FBI()->SetAutoClear(true);
    auto tgtrect = _target->mainSurfaceRectAtOrigin();

    _target->FBI()->setViewport(tgtrect);
    _target->FBI()->setScissor(tgtrect);
    _target->beginFrame();
    {
      // _target->TXI()->VRamUpload(tex);
      ork::fvec4 clr1(1.0f, 1.0f, 1.0f, 1.0f);
      mtl.SetTexture(tex.get());
      // mtl.SetColorMode( GfxMaterial3DSolid::EMODE_MOD_COLOR );
      mtl.SetColorMode(GfxMaterial3DSolid::EMODE_TEX_COLOR);
      mtl._rasterstate.SetBlending(Blending::OFF);
      _target->FBI()->GetThisBuffer()->RenderMatOrthoQuad(
          tgtrect.asSRect(), tgtrect.asSRect(), &mtl, 0.0f, 0.0f, 1.0f, 1.0f, 0, clr1);

      GLint curtex = 0;
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &curtex);
      printf("tex<%d>\n", int(gLastBoundNonZeroTex));

      glBindTexture(GL_TEXTURE_2D, gLastBoundNonZeroTex);
      static uint32_t* gpu32 = new uint32_t[512 * 512];
      int ix                 = rand() % 512;
      int iy                 = rand() % 512;
      int io                 = (iy * 512) + ix;
      int icr                = rand() % 255;
      int icg                = rand() % 255;
      int icb                = rand() % 255;
      gpu32[io]              = icr | (icg << 8) | (icb << 16) | (0xff << 24);

      glTexImage2D(GL_TEXTURE_2D, 0, 4, 512, 512, 0, GL_RGBA, GL_UNSIGNED_BYTE, (void*)gpu32);
      // glTexSubImage2D(	GL_TEXTURE_2D,
      //                    0,
      //                  0, 0,
      //                512, 512,
      //              GL_RGBA,
      //            GL_UNSIGNED_BYTE,
      //          tex->GetTexData() );
    }
    /////////////////////////////////////////////////////////////////////
    // HUD
    /////////////////////////////////////////////////////////////////////
    // DrawHUD(the_renderer.framedata());
    /////////////////////////////////////////////////////////////////////
    _target->endFrame(); // the_renderer );*/
  }

  GfxMaterial3DSolid mtl;
  texture_ptr_t tex;
  GLuint gltex;
};

DemoApp::DemoApp(int iw, int ih)
    : miWidth(iw)
    , miHeight(ih){
  mRenderGraph = new render_graph;
  mpThreadPool = new ork::threadpool::thread_pool;
  // mpThreadPool->init(16);
  mpThreadPool->init(4);

  //mpTimer = new QTimer;

  ////////////////////

  MyViewport* gpvp   = new MyViewport("yo");
  AppWindow* pgfxwin = new AppWindow(gpvp);

  //CtxGLFW* pctqt = new CtxGLFW(pgfxwin, nullptr);

  // gfxdock->setWidget( pctqt->window() );
  // gfxdock->setMinimumSize( 100, 100 );
  // addDockWidget(Qt::NoDockWidgetArea, gfxdock);

  //pctqt->Show();

  //pctqt->window()->Enable();

  // viewnum++;


  GfxEnv::GetRef().RegisterWinContext(pgfxwin);

  gpvp->Init(pgfxwin->context());

  //mpTimer->connect(mpTimer, SIGNAL(timeout()), pctqt->window(), SLOT(repaint()));
  //mpTimer->setSingleShot(false);
  //mpTimer->setInterval(16);
  //mpTimer->start(16);
}

///////////////////////////////////////////////////////////////////////////////

DemoApp::~DemoApp() {
  // SafeRelease(&m_pDirect2dFactory);
  // SafeRelease(&m_pRenderTarget);
  // SafeRelease(&m_pLightSlateGrayBrush);
  // SafeRelease(&m_pCornflowerBlueBrush);

  delete mpThreadPool;
}

void DemoApp::Run() {
}

///////////////////////////////////////////////////////////////////////////////

// void DemoApp::RunMessageLoop()
//{

/*	MSG msg;

    while(1)
    {
        if( PeekMessage( &msg, NULL, 0, 0, PM_NOREMOVE ) )
        {
            if (GetMessage(&msg, NULL, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else
        {
            Sleep(10);
        }
        InvalidateRect ( m_hwnd, NULL, TRUE );
    }*/
//}

///////////////////////////////////////////////////////////////////////////////

/*HRESULT DemoApp::CreateDeviceIndependentResources()
{
    HRESULT hr = S_OK;

    // Create a Direct2D factory.
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pDirect2dFactory);

    return hr;
}
*/
///////////////////////////////////////////////////////////////////////////////
/*
HRESULT DemoApp::Initialize()
{
    HRESULT hr;

    // Initialize device-indpendent resources, such
    // as the Direct2D factory.
    hr = CreateDeviceIndependentResources();

    if (SUCCEEDED(hr))
    {
        // Register the window class.
        WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
        wcex.style         = 0; //CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc   = DemoApp::WndProc;
        wcex.cbClsExtra    = 0;
        wcex.cbWndExtra    = sizeof(LONG_PTR);
        wcex.hInstance     = HINST_THISCOMPONENT;
        wcex.hbrBackground = NULL;
        wcex.lpszMenuName  = NULL;
        wcex.hCursor       = LoadCursor(NULL, IDI_APPLICATION);
        wcex.lpszClassName = "Lev3Test";

        RegisterClassEx(&wcex);


        // Because the CreateWindow function takes its size in pixels,
        // obtain the system DPI and use it to scale the window size.
        FLOAT dpiX, dpiY;

        // The factory returns the current system DPI. This is also the value it will use
        // to create its own windows.
        m_pDirect2dFactory->GetDesktopDpi(&dpiX, &dpiY);


        // Create the window.
        m_hwnd = CreateWindow(
            "Lev3Test",
            "Lev3Test App",
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            miWidth, //static_cast<UINT>(ceil(float(miWidth) * dpiX / 96.f)),
            miHeight, //static_cast<UINT>(ceil(float(miHeight) * dpiY / 96.f)),
            NULL,
            NULL,
            HINST_THISCOMPONENT,
            this
            );
        hr = m_hwnd ? S_OK : E_FAIL;
        if (SUCCEEDED(hr))
        {
            ShowWindow(m_hwnd, SW_SHOWNORMAL);
            UpdateWindow(m_hwnd);
        }
    }

    return hr;
}

///////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DemoApp::WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;

    if (message == WM_CREATE)
    {
        LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
        DemoApp *pDemoApp = (DemoApp *)pcs->lpCreateParams;

        ::SetWindowLongPtrW(
            hwnd,
            GWLP_USERDATA,
            PtrToUlong(pDemoApp)
            );

        result = 1;
    }
    else
    {
        DemoApp *pDemoApp = reinterpret_cast<DemoApp *>(static_cast<LONG_PTR>(
            ::GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA
                )));

        bool wasHandled = false;

        if (pDemoApp)
        {
            switch (message)
            {
            case WM_SIZE:
                {
                    UINT width = LOWORD(lParam);
                    UINT height = HIWORD(lParam);
                    pDemoApp->OnResize(width, height);
                }
                result = 0;
                wasHandled = true;
                break;

            case WM_DISPLAYCHANGE:
                {
                    InvalidateRect(hwnd, NULL, FALSE);
                }
                result = 0;
                wasHandled = true;
                break;

            case WM_PAINT:
                {
                    pDemoApp->OnRender();
                    ValidateRect(hwnd, NULL);
                }
                result = 0;
                wasHandled = true;
                break;

            case WM_DESTROY:
                {
                    PostQuitMessage(0);
                }
                result = 1;
                wasHandled = true;
                break;
            case WM_KEYDOWN:
            {
                int key = wParam;
                switch( key )
                {
                    case 'M':
                        if( pDemoApp->miNumAviFrames )
                            pDemoApp->EndMovie();
                        else
                            pDemoApp->StartMovie();
                        break;
                }
                break;
            }
            }
        }

        if (!wasHandled)
        {
            result = DefWindowProc(hwnd, message, wParam, lParam);
        }
    }

    return result;
}

void DemoApp::EndMovie()
{
    miNumAviFrames = 0;
    printf( "Movie Ended\n" );
}

void DemoApp::StartMovie()
{
    if( 0 == miNumAviFrames )
    {
        printf( "starting movie\n" );
        miNumAviFrames = 30*60; //60; //60;
        miFrameIndex = 0;
    }
}
///////////////////////////////////////////////////////////////////////////////

HRESULT DemoApp::CreateDeviceResources()
{
    HRESULT hr = S_OK;

    if (!m_pRenderTarget)
    {
        RECT rc;
        GetClientRect(m_hwnd, &rc);

        D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top
            );

        // Create a Direct2D render target.
        hr = m_pDirect2dFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &m_pRenderTarget
            );


        if (SUCCEEDED(hr))
        {
            // Create a gray brush.
            hr = m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::LightSlateGray),
                &m_pLightSlateGrayBrush
                );
        }
        if (SUCCEEDED(hr))
        {
            // Create a blue brush.
            hr = m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::CornflowerBlue),
                &m_pCornflowerBlueBrush
                );
        }
        if( SUCCEEDED(hr) )
        {
            // Create a blue brush.
            hr = m_pRenderTarget->CreateSolidColorBrush(
                D2D1::ColorF(D2D1::ColorF::Black),
                &mpDefaultBrush
                );

        }
        if( SUCCEEDED(hr) )
        {
            D2D1_BITMAP_PROPERTIES bmaprops;
            D2D1_SIZE_U bmapsize;

            bmapsize.width = 2;
            bmapsize.height = 2;
            bmaprops.dpiX = 1;
            bmaprops.dpiY = 1;
            bmaprops.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
            bmaprops.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
            ID2D1Bitmap* bitmap = 0;

            u32* psrcdata = new u32[4];

            psrcdata[0] = 0xffffffff;
            psrcdata[1] = 0x0;
            psrcdata[2] = 0x0;
            psrcdata[3] = 0xffffffff;

            UINT32 psrcpitch = 8;

            hr = m_pRenderTarget->CreateBitmap(
                bmapsize,
                psrcdata,
                psrcpitch,
                & bmaprops,
                &bitmap
                );

            D2D1_MATRIX_3X2_F mtx;
            mtx._11 = 1.0f;
            mtx._12 = 0.0f;
            mtx._21 = 0.0f;
            mtx._22 = 1.0f;
            mtx._31 = 0.0f;
            mtx._32 = 0.0f;

            D2D1_BRUSH_PROPERTIES brushprops;
            brushprops.opacity = 1.0f;
            brushprops.transform = mtx;

            D2D1_BITMAP_BRUSH_PROPERTIES brushbmapprops;
            brushbmapprops.extendModeX = D2D1_EXTEND_MODE_WRAP;
            brushbmapprops.extendModeY = D2D1_EXTEND_MODE_WRAP;
            brushbmapprops.interpolationMode = D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR;
//D2D1_BITMAP_INTERPOLATION_MODE_LINEAR;

            hr = m_pRenderTarget->CreateBitmapBrush( bitmap, & brushbmapprops, & brushprops, & mpCheckerBrush );


        }

        if( SUCCEEDED(hr) )
        {
            mpFrameBuffer = new u32[ size.width*size.height ];

            D2D1_BITMAP_PROPERTIES bmaprops;
            D2D1_SIZE_U bmapsize;

            bmapsize.width = size.width;
            bmapsize.height = size.height;
            bmaprops.dpiX = 1;
            bmaprops.dpiY = 1;
            bmaprops.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
            bmaprops.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;

            UINT32 psrcpitch = size.width*4;

            hr = m_pRenderTarget->CreateBitmap(
                bmapsize,
                0,
                psrcpitch,
                & bmaprops,
                &mpBackBufferBitmap
                );

        }

    }

    return hr;
}

///////////////////////////////////////////////////////////////////////////////

void DemoApp::DiscardDeviceResources()
{
    SafeRelease(&m_pRenderTarget);
    SafeRelease(&m_pLightSlateGrayBrush);
    SafeRelease(&m_pCornflowerBlueBrush);
}
*/
///////////////////////////////////////////////////////////////////////////////
/*
void DemoApp::OnResize(UINT width, UINT height)
{
    mRenderGraph->Resize(width,height);
    if (m_pRenderTarget)
    {
        // Note: This method can fail, but it's okay to ignore the
        // error here, because the error will be returned again
        // the next time EndDraw is called.
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));

    }

    if( mpFrameBuffer )
    {
        delete[] mpFrameBuffer;
    }
    mpFrameBuffer = new u32[ width*height ];

    if( mpBackBufferBitmap )
    {
        mpBackBufferBitmap->Release();

        D2D1_BITMAP_PROPERTIES bmaprops;
        D2D1_SIZE_U bmapsize;

        bmapsize.width = width;
        bmapsize.height = height;
        bmaprops.dpiX = 1;
        bmaprops.dpiY = 1;
        bmaprops.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
        bmaprops.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;

        UINT32 psrcpitch = width*4;

        HRESULT hr = m_pRenderTarget->CreateBitmap(
            bmapsize,
            0,
            psrcpitch,
            & bmaprops,
            &mpBackBufferBitmap
            );

    }
}
*/
