
#pragma comment(lib, "OpenGL32.lib")
#pragma comment(lib, "GLU32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "GdiPlus.lib")

#include <iostream>
#include <cmath>
#include <Windows.h>
#include <GL/gl.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#define TITLE				"Asylum's shader sample"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define V_RETURN(r, e, x)	{ if( !(x) ) { MYERROR(e); return r; }}

HWND		hwnd			= NULL;
HDC			hdc				= NULL;
HGLRC		hrc				= NULL;

RECT		workarea;
long		screenwidth		= 1024;
long		screenheight	= 576;
bool		uninited		= false;

struct InputState
{
	unsigned char Button;
	short X, Y;
	int dX, dY, dZ;
} inputstate;

// must be implemented by sample
extern bool InitScene();

extern void UninitScene();
extern void Update(float delta);
extern void Render(float alpha, float elapsedtime);

extern void Event_KeyDown(unsigned char keycode);
extern void Event_KeyUp(unsigned char keycode);
extern void Event_MouseMove(int x, int y, short dx, short dy);
extern void Event_MouseScroll(int x, int y, short dz);
extern void Event_MouseDown(int x, int y, unsigned char button);
extern void Event_MouseUp(int x, int y, unsigned char button);

namespace Quadron
{
	extern bool wIsSupported(const char* name, HDC hdc);
}

bool InitGL(HWND hwnd)
{
	HWND dummy = CreateWindow("TestClass", "Dummy", WS_CLIPCHILDREN|WS_CLIPSIBLINGS,
		0, 0, 100, 100, 0, 0, GetModuleHandle(0), 0);

	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		32, // color
		0, 0, 0, 0, 0, 0,
		0, // alpha
		0, 0, 0, 0, 0, 0,
		24, 8, 0, // depth, stencil, aux
		PFD_MAIN_PLANE,
		0, 0, 0, 0
	};

	hdc = GetDC(dummy);
	V_RETURN(false, "InitGL(): Could not get device context", hdc);
	
	int pixelformat = ChoosePixelFormat(hdc, &pfd);
	V_RETURN(false, "InitGL(): Could not find suitable pixel format", pixelformat);

	BOOL success = SetPixelFormat(hdc, pixelformat, &pfd);
	V_RETURN(false, "InitGL(): Could not setup pixel format", success);

	hrc = wglCreateContext(hdc);
	V_RETURN(false, "InitGL(): Context creation failed", hrc);

	success = wglMakeCurrent(hdc, hrc);
	V_RETURN(false, "InitGL(): Could not acquire context", success);

	const char* str = (const char*)glGetString(GL_VENDOR);
	std::cout << "Vendor: " << str << "\n";

	str = (const char*)glGetString(GL_RENDERER);
	std::cout << "Renderer: " << str << "\n";

	str = (const char*)glGetString(GL_VERSION);
	std::cout << "OpenGL version: " << str << "\n\n";

	int major, minor;

	sscanf_s(str, "%1d.%2d %*s", &major, &minor);

	if( major < 3 || (major == 3 && minor < 2) )
	{
		std::cout << "Device does not support OpenGL 3.2\n";
		return false;
	}

	if( Quadron::wIsSupported("WGL_ARB_pixel_format", hdc) )
	{
		std::cout << "WGL_ARB_pixel_format present, querying pixel formats...\n";

		// NOTE: could use qGLExtensions::QueryFeatures(), but it would be a waste; only core profile contexts report the new extensions
		typedef HGLRC (APIENTRY *WGLCREATECONTEXTATTRIBSARBPROC)(HDC hDC, HGLRC hShareContext, const int *attribList);
		typedef BOOL (APIENTRY *WGLGETPIXELFORMATATTRIBIVARBPROC)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, int *piValues);
		typedef BOOL (APIENTRY *WGLGETPIXELFORMATATTRIBFVARBPROC)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int *piAttributes, FLOAT *pfValues);
		typedef BOOL (APIENTRY *WGLCHOOSEPIXELFORMATARBPROC)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);

		WGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB = (WGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProcAddress("wglGetPixelFormatAttribivARB");
		WGLGETPIXELFORMATATTRIBFVARBPROC wglGetPixelFormatAttribfvARB = (WGLGETPIXELFORMATATTRIBFVARBPROC)wglGetProcAddress("wglGetPixelFormatAttribfvARB");
		WGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = (WGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
		WGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;

		int attrib[32];
		int i = 0;
		UINT numformats;

		memset(attrib, 0, sizeof(attrib));

		attrib[i++] = 0x2001;		// WGL_DRAW_TO_WINDOW_ARB
		attrib[i++] = TRUE;
		attrib[i++] = 0x2003;		// WGL_ACCELERATION_ARB
		attrib[i++] = 0x2027;		// WGL_FULL_ACCELERATION_ARB
		attrib[i++] = 0x2010;		// WGL_SUPPORT_OPENGL_ARB
		attrib[i++] = TRUE;
		attrib[i++] = 0x2011;		// WGL_DOUBLE_BUFFER_ARB
		attrib[i++] = TRUE;
		attrib[i++] = 0x2013;		// WGL_PIXEL_TYPE_ARB
		attrib[i++] = 0x202B;		// WGL_TYPE_RGBA_ARB
		attrib[i++] = 0x2014;		// WGL_COLOR_BITS_ARB
		attrib[i++] = pfd.cColorBits = 32;
		attrib[i++] = 0x201B;		// WGL_ALPHA_BITS_ARB
		attrib[i++] = pfd.cAlphaBits = 0;
		attrib[i++] = 0x2022;		// WGL_DEPTH_BITS_ARB
		attrib[i++] = pfd.cDepthBits = 24;
		attrib[i++] = 0x2023;		// WGL_STENCIL_BITS_ARB
		attrib[i++] = pfd.cStencilBits = 8;
		attrib[i++] = 0;

		if( attrib[1] )
			pfd.dwFlags |= PFD_DRAW_TO_WINDOW;

		if( attrib[5] )
			pfd.dwFlags |= PFD_SUPPORT_OPENGL;

		if( attrib[7] )
			pfd.dwFlags |= PFD_DOUBLEBUFFER;

		if( attrib[9] == 0x2029 )
			pfd.dwFlags |= PFD_SWAP_COPY;

		wglChoosePixelFormatARB(hdc, attrib, 0, 1, &pixelformat, &numformats);

		if( numformats != 0 )
		{
			std::cout << "Selected pixel format: " << pixelformat <<"\n";

			if( Quadron::wIsSupported("WGL_ARB_create_context", hdc) &&
				Quadron::wIsSupported("WGL_ARB_create_context_profile", hdc) )
			{
				wglCreateContextAttribsARB = (WGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
			}

			success = wglMakeCurrent(hdc, NULL);
			V_RETURN(false, "InitGL(): Could not release dummy context", success);

			success = wglDeleteContext(hrc);
			V_RETURN(false, "InitGL(): Could not delete dummy context", success);

			DestroyWindow(dummy);
			dummy = 0;
			hdc = GetDC(hwnd);
			hrc = 0;

			int success = SetPixelFormat(hdc, pixelformat, &pfd);
			V_RETURN(false, "InitGL(): Could not setup pixel format", success);

			if( wglCreateContextAttribsARB )
			{
				int contextattribs[] =
				{
					0x2091,			// WGL_CONTEXT_MAJOR_VERSION_ARB
					major,
					0x2092,			// WGL_CONTEXT_MINOR_VERSION_ARB
					minor,
					0x2094,			// WGL_CONTEXT_FLAGS_ARB
#ifdef _DEBUG
					// NOTE: ARB_debug_output only works on Windows @ nVidia/Intel
					0x0001,			// WGL_CONTEXT_DEBUG_BIT
#else
					0x0000,
#endif
					0x9126,			// WGL_CONTEXT_PROFILE_MASK_ARB
					0x00000001,		// WGL_CONTEXT_CORE_PROFILE_BIT_ARB
					0
				};

				hrc = wglCreateContextAttribsARB(hdc, NULL, contextattribs);
				V_RETURN(false, "InitGL(): Context creation failed", hrc);

				std::cout << "Created core profile context...\n";
			}

			if( !hrc )
			{
				hrc = wglCreateContext(hdc);
				V_RETURN(false, "InitGL(): Context creation failed", hrc);

				std::cout << "Created compatibility profile context...\n";
			}

			success = wglMakeCurrent(hdc, hrc);
			V_RETURN(false, "InitGL(): Could not acquire context", success);

			std::cout << std::endl;
		}
		else
			std::cout << "wglChoosePixelFormat failed, using classic context...\n";
	}
	else
		std::cout << "WGL_ARB_pixel_format not supported\n";

	if( dummy )
	{
		std::cout << "Selected pixel format: " << pixelformat <<"\n";

		DestroyWindow(dummy);
		hdc = GetDC(hwnd);

		int success = SetPixelFormat(hdc, pixelformat, &pfd);
		V_RETURN(false, "InitGL(): Could not setup pixel format", success);

		hrc = wglCreateContext(hdc);
		V_RETURN(false, "InitGL(): Context creation failed", hrc);

		success = wglMakeCurrent(hdc, hrc);
		V_RETURN(false, "InitGL(): Could not acquire context", success);

		std::cout << std::endl;
	}

	return true;
}

void UninitGL()
{
	if( !uninited )
	{
		uninited = true;

		// release context
		if( hrc )
		{
			UninitScene();

			if( !wglMakeCurrent(hdc, NULL) )
				MYERROR("Could not release context");

			if( !wglDeleteContext(hrc) )
				MYERROR("Could not delete context");

			hrc = NULL;
		}

		if( hdc && !ReleaseDC(hwnd, hdc) )
			MYERROR("Could not release device context");

		hdc = NULL;
		DestroyWindow(hwnd);
	}
}

void Adjust(tagRECT& out, long& width, long& height, DWORD style, DWORD exstyle, bool menu = false)
{
	long w = workarea.right - workarea.left;
	long h = workarea.bottom - workarea.top;

	out.left = (w - width) / 2;
	out.top = (h - height) / 2;
	out.right = (w + width) / 2;
	out.bottom = (h + height) / 2;

	AdjustWindowRectEx(&out, style, menu, 0);

	long windowwidth = out.right - out.left;
	long windowheight = out.bottom - out.top;

	long dw = windowwidth - width;
	long dh = windowheight - height;

	if( windowheight > h )
	{
		float ratio = (float)width / (float)height;
		float realw = (float)(h - dh) * ratio + 0.5f;

		windowheight = h;
		windowwidth = (long)floor(realw) + dw;
	}

	if( windowwidth > w )
	{
		float ratio = (float)height / (float)width;
		float realh = (float)(w - dw) * ratio + 0.5f;

		windowwidth = w;
		windowheight = (long)floor(realh) + dh;
	}

	out.left = workarea.left + (w - windowwidth) / 2;
	out.top = workarea.top + (h - windowheight) / 2;
	out.right = workarea.left + (w + windowwidth) / 2;
	out.bottom = workarea.top + (h + windowheight) / 2;

	width = windowwidth - dw;
	height = windowheight - dh;
}

LRESULT WINAPI WndProc(HWND hWnd, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	if( hWnd != hwnd )
		return DefWindowProc(hWnd, msg, wParam, lParam);

	switch( msg )
	{
	case WM_CLOSE:
		ShowWindow(hWnd, SW_HIDE);
		UninitGL();

		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	
	case WM_KEYDOWN:
		Event_KeyDown((unsigned char)wParam);
		break;

	case WM_KEYUP:
		switch(wParam)
		{
		case VK_ESCAPE:
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		default:
			Event_KeyUp((unsigned char)wParam);
			break;
		}
		break;

	case WM_MOUSEMOVE: {
		short x = (short)(lParam & 0xffff);
		short y = (short)((lParam >> 16) & 0xffff);

		inputstate.dX = x - inputstate.X;
		inputstate.dY = y - inputstate.Y;

		inputstate.X = x;
		inputstate.Y = y;

		Event_MouseMove(inputstate.X, inputstate.Y, inputstate.dX, inputstate.dY);
		} break;

	case WM_MOUSEWHEEL: {
		short x = (short)(lParam & 0xffff);
		short y = (short)((lParam >> 16) & 0xffff);
		short delta = (short)((wParam >> 16) & 0xffff);

		inputstate.dZ = delta / WHEEL_DELTA;

		Event_MouseScroll(x, x, inputstate.dZ);
		} break;

	case WM_LBUTTONDOWN:
		inputstate.Button |= 1;
		Event_MouseDown(inputstate.X, inputstate.Y, 1);
		break;

	case WM_RBUTTONDOWN:
		inputstate.Button |= 2;
		Event_MouseDown(inputstate.X, inputstate.Y, 2);
		break;

	case WM_MBUTTONDOWN:
		inputstate.Button |= 4;
		Event_MouseDown(inputstate.X, inputstate.Y, 4);
		break;

	case WM_LBUTTONUP:
		Event_MouseUp(inputstate.X, inputstate.Y, inputstate.Button & 1);
		inputstate.Button &= (~1);
		break;

	case WM_RBUTTONUP:
		Event_MouseUp(inputstate.X, inputstate.Y, inputstate.Button & 2);
		inputstate.Button &= (~2);
		break;

	case WM_MBUTTONUP:
		Event_MouseUp(inputstate.X, inputstate.Y, inputstate.Button & 4);
		inputstate.Button &= (~4);
		break;

	default:
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void ReadResolutionFile()
{
	FILE* fp = 0;
	
	fopen_s(&fp, "res.conf", "rb");

	if( fp )
	{
		fscanf_s(fp, "%ld %ld\n", &screenwidth, &screenheight);
		fclose(fp);

		if( screenwidth < 640 )
			screenwidth = 640;

		if( screenheight < 480 )
			screenheight = 480;
	}
	else
	{
		fopen_s(&fp, "res.conf", "wb");

		if( fp )
		{
			fprintf(fp, "%ld %ld\n", screenwidth, screenheight);
			fclose(fp);
		}
	}
}

std::string GetResource(const std::string& file)
{
	return file;
}

int main(int argc, char* argv[])
{
	ReadResolutionFile();

	LARGE_INTEGER qwTicksPerSec = { 0, 0 };
	LARGE_INTEGER qwTime;
	LONGLONG tickspersec;
	double last, current;
	double delta, accum = 0;

	WNDCLASSEX wc =
	{
		sizeof(WNDCLASSEX),
		CS_OWNDC,
		(WNDPROC)WndProc,
		0L,
		0L,
		GetModuleHandle(NULL),
		NULL,
		LoadCursor(0, IDC_ARROW),
		NULL, NULL, "TestClass", NULL
	};

	RegisterClassEx(&wc);
	SystemParametersInfo(SPI_GETWORKAREA, 0, &workarea, 0);
	
	RECT rect = { 0, 0, screenwidth, screenheight };
	DWORD style = WS_CLIPCHILDREN|WS_CLIPSIBLINGS;

	style |= WS_SYSMENU|WS_BORDER|WS_CAPTION|WS_MINIMIZEBOX;
	Adjust(rect, screenwidth, screenheight, style, true);
	
	hwnd = CreateWindowA("TestClass", TITLE, style,
		rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
		NULL, NULL, wc.hInstance, NULL);
	
	if( !hwnd )
	{
		MYERROR("Could not create window");
		goto _end;
	}

	if( !InitGL(hwnd) )
	{
		MYERROR("Failed to initialize OpenGL");
		goto _end;
	}
	
	if( !InitScene() )
	{
		MYERROR("Failed to initialize scene");
		goto _end;
	}
	
	ShowWindow(hwnd, SW_SHOWDEFAULT);
	UpdateWindow(hwnd);

	MSG msg;
	ZeroMemory(&msg, sizeof(msg));

	QueryPerformanceFrequency(&qwTicksPerSec);
	tickspersec = qwTicksPerSec.QuadPart;

	QueryPerformanceCounter(&qwTime);
	last = (qwTime.QuadPart % tickspersec) / (double)tickspersec;

	inputstate.Button = 0;
	inputstate.X = inputstate.Y = 0;
	inputstate.dX = inputstate.dY = 0;

	while( msg.message != WM_QUIT )
	{
		QueryPerformanceCounter(&qwTime);
		current = (qwTime.QuadPart % tickspersec) / (double)tickspersec;

		if (current < last)
			delta = ((1.0 + current) - last);
		else
			delta = (current - last);

		last = current;
		accum += delta;

		while( accum > 0.1f )
		{
			accum -= 0.1f;

			while( PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE) )
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);

				if( msg.message == WM_QUIT )
					break;
			}

			if( msg.message == WM_QUIT )
				break;

			if( !uninited )
				Update(0.1f);
		}

		if( !uninited )
			Render((float)accum / 0.1f, (float)delta);
	}

_end:
	UninitGL();
	std::cout << "Exiting...\n";

	UnregisterClass("TestClass", wc.hInstance);
	_CrtDumpMemoryLeaks();

#ifdef _DEBUG
	system("pause");
#endif

	return 0;
}
