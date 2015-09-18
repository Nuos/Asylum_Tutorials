//*************************************************************************************************************
#include <d3d10.h>
#include <d3dx10.h>
#include <iostream>

#include "../common/common.h"

#define MAX_SAMPLES		16384 //4096

// helper macros
#define TITLE				"Shader sample 24: Simple path tracer"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define MYVALID(x)			{ if( FAILED(hr = x) ) { MYERROR(#x); return hr; } }
#define SAFE_RELEASE(x)		{ if( (x) ) { (x)->Release(); (x) = NULL; } }

// external variables
extern ID3D10Device* device;
extern IDXGISwapChain* swapchain;
extern ID3D10RenderTargetView* rendertargetview;
extern ID3D10DepthStencilView* depthstencilview;
extern HWND hwnd;

extern long		screenwidth;
extern long		screenheight;
extern short	mousedx;
extern short	mousedy;
extern short	mousedown;

// external functions
extern HRESULT LoadMeshFromQM(LPCTSTR file, DWORD options, ID3DX10Mesh** mesh);
extern HRESULT RenderText(const std::string& str, ID3D10Texture2D* tex, DWORD width, DWORD height);

// sample variables
ID3D10Effect*				pathtracer		= 0;
ID3D10EffectTechnique*		technique1		= 0;
ID3D10EffectTechnique*		technique2		= 0;

ID3D10RenderTargetView*		rendertargetview1	= 0;
ID3D10RenderTargetView*		rendertargetview2	= 0;
ID3D10Texture2D*			rendertargettex1	= 0;
ID3D10Texture2D*			rendertargettex2	= 0;
ID3D10ShaderResourceView*	rendertargetsrv1	= 0;
ID3D10ShaderResourceView*	rendertargetsrv2	= 0;

state<D3DXVECTOR2>			cameraangle;
int							currtarget = 0;
int							currsample = 0;

static HRESULT DXCreateEffect(const char* file, ID3D10Effect** effect)
{
	ID3D10Blob*	errors		= NULL;
	UINT hlslflags = D3D10_SHADER_ENABLE_STRICTNESS;

#ifdef _DEBUG
	hlslflags |= D3D10_SHADER_DEBUG;
#endif

	HRESULT hr = D3DX10CreateEffectFromFile(file, 0, 0, "fx_4_0", hlslflags, 0, device, 0, 0, effect, &errors, 0);

	if( errors )
	{
		char* str = (char*)errors->GetBufferPointer();
		std::cout << str << "\n";

		errors->Release();
	}

	return hr;
}

HRESULT InitScene()
{
	HRESULT hr;

	SetWindowText(hwnd, TITLE);

	// effects
	std::cout << "Compiling shader...\n";
	MYVALID(DXCreateEffect("../media/shaders10/pathtracer10.fx", &pathtracer));
	
	technique1 = pathtracer->GetTechniqueByName("pathtrace");
	technique2 = pathtracer->GetTechniqueByName("tonemap");

	// render targets
	D3D10_RENDER_TARGET_VIEW_DESC	rtvdesc;
	D3D10_TEXTURE2D_DESC			texdesc;
	D3D10_SHADER_RESOURCE_VIEW_DESC	srvdesc;

	texdesc.Width				= screenwidth;
	texdesc.Height				= screenheight;
	texdesc.MipLevels			= 1;
	texdesc.ArraySize			= 1;
	texdesc.SampleDesc.Count	= 1;
	texdesc.SampleDesc.Quality	= 0;
	texdesc.Format				= DXGI_FORMAT_R32G32B32A32_FLOAT;
	texdesc.Usage				= D3D10_USAGE_DEFAULT;
	texdesc.BindFlags			= D3D10_BIND_RENDER_TARGET|D3D10_BIND_SHADER_RESOURCE;
	texdesc.CPUAccessFlags		= 0;
	texdesc.MiscFlags			= 0;

	MYVALID(device->CreateTexture2D(&texdesc, NULL, &rendertargettex1));
	MYVALID(device->CreateTexture2D(&texdesc, NULL, &rendertargettex2));

	srvdesc.Format						= texdesc.Format;
	srvdesc.ViewDimension				= D3D10_SRV_DIMENSION_TEXTURE2D;
	srvdesc.Texture2D.MipLevels			= 1;
	srvdesc.Texture2D.MostDetailedMip	= 0;

	MYVALID(device->CreateShaderResourceView(rendertargettex1, &srvdesc, &rendertargetsrv1));
	MYVALID(device->CreateShaderResourceView(rendertargettex2, &srvdesc, &rendertargetsrv2));

	rtvdesc.Format				= texdesc.Format;
	rtvdesc.ViewDimension		= D3D10_RTV_DIMENSION_TEXTURE2D;
	rtvdesc.Texture2D.MipSlice	= 0;

	MYVALID(device->CreateRenderTargetView(rendertargettex1, &rtvdesc, &rendertargetview1));
	MYVALID(device->CreateRenderTargetView(rendertargettex2, &rtvdesc, &rendertargetview2));

	// other
	cameraangle = D3DXVECTOR2(0, 0);

	return S_OK;
}
//*************************************************************************************************************
void UninitScene()
{
	SAFE_RELEASE(rendertargetview1);
	SAFE_RELEASE(rendertargetview2);
	SAFE_RELEASE(rendertargetsrv1);
	SAFE_RELEASE(rendertargetsrv2);
	SAFE_RELEASE(rendertargettex1);
	SAFE_RELEASE(rendertargettex2);
	SAFE_RELEASE(pathtracer);
}
//*************************************************************************************************************
void KeyPress(WPARAM wparam)
{
}
//*************************************************************************************************************
void Update(float delta)
{
	D3DXVECTOR2 velocity(mousedx, mousedy);

	cameraangle.prev = cameraangle.curr;

	if( mousedown == 1 )
		cameraangle.curr += velocity * 0.004f;

	// clamp to [-pi, pi]
	if( cameraangle.curr.y >= 1.5f )
		cameraangle.curr.y = 1.5f;

	if( cameraangle.curr.y <= -1.5f )
		cameraangle.curr.y = -1.5f;

	if( memcmp(&cameraangle.prev, &cameraangle.curr, sizeof(D3DXVECTOR2)) != 0 )
		currsample = 0;
}
//*************************************************************************************************************
void Render(float alpha, float elapsedtime)
{
	static float time = 0;

	ID3D10RenderTargetView* rtvs[] = { rendertargetview1, rendertargetview2 };
	ID3D10ShaderResourceView* srvs[] = { rendertargetsrv1, rendertargetsrv2 };

	D3DXMATRIX		view, proj, viewproj, viewprojinv;
	D3DXVECTOR3		eye(0, 1.5f, -5.5f);
	D3DXVECTOR3		look(0, 1.5f, -0.5f);
	D3DXVECTOR3		up(0, 1, 0);
	D3DXVECTOR2		orient = cameraangle.smooth(alpha);
	D3DXVECTOR2		screensize((float)screenwidth, (float)screenheight);
	float			color[4] = { 0, 0, 0, 1 };

	time += elapsedtime;

	// setup things
	float aspect	= (float)screenwidth / (float)screenheight;
	float fovy		= (float)D3DX_PI / 4.0f;
	float tfovx		= aspect * tanf(fovy * 0.5f);

	D3DXMatrixRotationYawPitchRoll(&view, orient.x, orient.y, 0);
	D3DXVec3TransformCoord(&eye, &eye, &view);

	D3DXMatrixLookAtLH(&view, &eye, &look, &up);
	D3DXMatrixPerspectiveFovLH(&proj, fovy, aspect, 0.1f, 50);

	proj._33 = 1;
	proj._43 = -0.1f;

	D3DXMatrixMultiply(&viewproj, &view, &proj);
	D3DXMatrixInverse(&viewprojinv, 0, &viewproj);

	if( currsample == 0 )
	{
		device->ClearRenderTargetView(rendertargetview1, color);
		device->ClearRenderTargetView(rendertargetview2, color);
	}

	if( currsample < MAX_SAMPLES )
	{
		currtarget = 1 - currtarget;
		++currsample;

		// path trace
		device->OMSetRenderTargets(1, &rtvs[currtarget], depthstencilview);
		device->ClearRenderTargetView(rtvs[currtarget], color);
		device->ClearDepthStencilView(depthstencilview, D3D10_CLEAR_DEPTH|D3D10_CLEAR_STENCIL, 1.0f, 0);
		{
			pathtracer->GetVariableByName("matViewProjInv")->AsMatrix()->SetMatrix(&viewprojinv._11);
			pathtracer->GetVariableByName("eyePos")->AsVector()->SetFloatVector(&eye.x);
			pathtracer->GetVariableByName("screenSize")->AsVector()->SetFloatVector(&screensize.x);
			pathtracer->GetVariableByName("prevIteration")->AsShaderResource()->SetResource(srvs[1 - currtarget]);
			pathtracer->GetVariableByName("time")->AsScalar()->SetFloat(time);
			pathtracer->GetVariableByName("currSample")->AsScalar()->SetFloat((float)currsample);

			device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			device->IASetInputLayout(0);

			technique1->GetPassByIndex(0)->Apply(0);
			device->Draw(4, 0);
		}
	}

	// present
	device->OMSetRenderTargets(1, &rendertargetview, depthstencilview);
	device->ClearRenderTargetView(rendertargetview, color);
	device->ClearDepthStencilView(depthstencilview, D3D10_CLEAR_DEPTH|D3D10_CLEAR_STENCIL, 1.0f, 0);
	{
		pathtracer->GetVariableByName("prevIteration")->AsShaderResource()->SetResource(0);
		pathtracer->GetVariableByName("sceneTexture")->AsShaderResource()->SetResource(srvs[currtarget]);

		device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		device->IASetInputLayout(NULL);

		technique2->GetPassByIndex(0)->Apply(0);
		device->Draw(4, 0);
	}
	swapchain->Present(0, 0);
}
//*************************************************************************************************************
