////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//#include <ork/math/raytracer.h>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>
#include <ork/math/frustum.h>
#include <ork/kernel/thread_pool.h>
//#include <ork/util/avi_utils.h>

class QTimer;

template<class Interface>
inline void SafeRelease(
    Interface **ppInterfaceToRelease
    )
{
    if (*ppInterfaceToRelease != NULL)
    {
        (*ppInterfaceToRelease)->Release();

        (*ppInterfaceToRelease) = NULL;
    }
}

///////////////////////////////////////////////////////////////////////////////

class render_graph;
class thread_pool;

///////////////////////////////////////////////////////////////////////////////

class DemoApp
{
public:
    DemoApp(int iw, int ih);
    ~DemoApp();

    // Register the window class and call methods for instantiating drawing resources
    //HRESULT Initialize();

    // Process and dispatch messages
    void Run();

//	void StartMovie();
//	void EndMovie();

private:
    // Initialize device-independent resources.
    //HRESULT CreateDeviceIndependentResources();

    // Initialize device-dependent resources.
    //HRESULT CreateDeviceResources();

    // Release device-dependent resource.
  //  void DiscardDeviceResources();

    // Draw content.
    //HRESULT OnRender();

	void Render1();
	void Render2();

    // Resize the render target.
    //void OnResize(
      //  UINT width,
        //UINT height
        //);

    // The windows procedure.
//    static LRESULT CALLBACK WndProc(
  //      HWND hWnd,
    //    UINT message,
      //  WPARAM wParam,
        //LPARAM lParam
        //);


	private:
//	HWND m_hwnd;
	
	int							miNumAviFrames;
	int							miFrameIndex;
//	ID2D1Factory* m_pDirect2dFactory;
//	ID2D1HwndRenderTarget* m_pRenderTarget;
//	ID2D1SolidColorBrush* mpDefaultBrush;
//	ID2D1SolidColorBrush* m_pLightSlateGrayBrush;
//	ID2D1SolidColorBrush* m_pCornflowerBlueBrush;
//	ID2D1BitmapBrush*	 mpCheckerBrush;
//	ID2D1Bitmap*		mpBackBufferBitmap;
	u32*				mpFrameBuffer;
	render_graph*		mRenderGraph;
	ork::threadpool::thread_pool*		mpThreadPool;
    QTimer*             mpTimer;
	int					miWidth;
	int					miHeight;
};
