//*************************************************************************************************************
#include <d3d10.h>
#include <d3dx10.h>
#include <iostream>

#include "../common/common.h"

//#define GENERATE_BRDF
//#define GENERATE_DIFF_IRRAD
//#define GENERATE_SPEC_IRRAD

#define FILENAME			"uffizi" //"local1"
#define CUBEMAP_DIFF_SIZE	128
#define CUBEMAP_SPEC_SIZE	512
#define CUBEMAP_SPEC_MIPS	8

#define NUM_SPHERES			11
#define SPHERE_SPACING		0.2f

// helper macros
#define TITLE				"Shader sample 24: Prefilter environment map"
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
ID3D10Effect*				skyeffect		= 0;
ID3D10Effect*				metal			= 0;
ID3DX10Mesh*				object			= 0;
ID3DX10Mesh*				sky				= 0;
ID3D10InputLayout*			vertexlayout1	= 0;
ID3D10InputLayout*			vertexlayout2	= 0;
ID3D10ShaderResourceView*	skytexture		= 0;
ID3D10EffectTechnique*		technique1		= 0;
ID3D10EffectTechnique*		technique2		= 0;
ID3D10ShaderResourceView*	integrateddiff	= 0;
ID3D10ShaderResourceView*	integratedspec	= 0;
ID3D10ShaderResourceView*	integratedbrdf	= 0;

state<D3DXVECTOR2>			cameraangle;

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

	// textures
	MYVALID(D3DX10CreateShaderResourceViewFromFile(device, "../media/textures/" FILENAME ".dds", 0, 0, &skytexture, 0));

	// effects
	MYVALID(DXCreateEffect("../media/shaders10/sky10.fx", &skyeffect));
	MYVALID(DXCreateEffect("../media/shaders10/metal10.fx", &metal));

	technique1 = skyeffect->GetTechniqueByName("sky");
	technique2 = metal->GetTechniqueByName("metal");

	D3D10_PASS_DESC					passdesc;
	const D3D10_INPUT_ELEMENT_DESC*	decl = 0;
	UINT							declsize = 0;

	// meshes
	MYVALID(LoadMeshFromQM("../media/meshes10/sky.qm", 0, &sky));
	MYVALID(LoadMeshFromQM("../media/meshes10/sphere.qm", 0, &object));

	sky->GetVertexDescription(&decl, &declsize);
	technique1->GetPassByIndex(0)->GetDesc(&passdesc);

	MYVALID(device->CreateInputLayout(decl, declsize, passdesc.pIAInputSignature, passdesc.IAInputSignatureSize, &vertexlayout1));

	object->GetVertexDescription(&decl, &declsize);
	technique2->GetPassByIndex(0)->GetDesc(&passdesc);

	MYVALID(device->CreateInputLayout(decl, declsize, passdesc.pIAInputSignature, passdesc.IAInputSignatureSize, &vertexlayout2));

	// other
	cameraangle = D3DXVECTOR2(0, 0);

#if defined(GENERATE_BRDF) || defined(GENERATE_SPEC_IRRAD) || defined(GENERATE_DIFF_IRRAD)
	D3D10_DEPTH_STENCIL_VIEW_DESC	dsvdesc;
	D3D10_RENDER_TARGET_VIEW_DESC	rtvdesc;
	D3D10_TEXTURE2D_DESC			texdesc;
	D3D10_VIEWPORT					viewport;
	float							color[] = { 0, 0, 0, 1 };

	viewport.MinDepth	= 0.0f;
	viewport.MaxDepth	= 1.0f;
	viewport.TopLeftX	= 0;
	viewport.TopLeftY	= 0;
#endif

#if defined(GENERATE_DIFF_IRRAD) || defined(GENERATE_SPEC_IRRAD)
	ID3D10Effect*			prefilter		= 0;
	ID3D10EffectTechnique*	prefiltertech	= 0;

	D3DXMATRIX				world(20, 0, 0, 0, 0, 20, 0, 0, 0, 0, 20, 0, 0, 0, 0, 1);
	D3DXMATRIX				proj;
	D3DXMATRIX				views[6];
	D3DXVECTOR3				look, up, eye(0, 0, 0);

	// view matrices
	up = D3DXVECTOR3(0, 1, 0);
	look = D3DXVECTOR3(1, 0, 0);
	D3DXMatrixLookAtLH(&views[0], &eye, &look, &up);

	look = D3DXVECTOR3(-1, 0, 0);
	D3DXMatrixLookAtLH(&views[1], &eye, &look, &up);

	look = D3DXVECTOR3(0, 0, 1);
	D3DXMatrixLookAtLH(&views[4], &eye, &look, &up);

	look = D3DXVECTOR3(0, 0, -1);
	D3DXMatrixLookAtLH(&views[5], &eye, &look, &up);

	up = D3DXVECTOR3(0, 0, -1);
	look = D3DXVECTOR3(0, 1, 0);
	D3DXMatrixLookAtLH(&views[2], &eye, &look, &up);

	up = D3DXVECTOR3(0, 0, 1);
	look = D3DXVECTOR3(0, -1, 0);
	D3DXMatrixLookAtLH(&views[3], &eye, &look, &up);

	// projection matrix
	D3DXMatrixPerspectiveFovLH(&proj, (float)D3DX_PI * 0.5f, 1.0f, 0.1f, 30.0f);

	// effect
	std::cout << "Compiling 'prefilterenvmap10.fx' ...\n";
	MYVALID(DXCreateEffect("../media/shaders10/prefilterenvmap10.fx", &prefilter));
#endif

#ifdef GENERATE_DIFF_IRRAD
	ID3D10RenderTargetView*	cubertv			= 0;
	ID3D10DepthStencilView*	cubedsv			= 0;
	ID3D10Texture2D*		diffcubemap		= 0;
	ID3D10Texture2D*		diffcubemapds	= 0;

	prefiltertech = prefilter->GetTechniqueByName("prefilter_diff");

	texdesc.Width				= CUBEMAP_DIFF_SIZE;
	texdesc.Height				= CUBEMAP_DIFF_SIZE;
	texdesc.MipLevels			= 1;
	texdesc.ArraySize			= 6;
	texdesc.SampleDesc.Count	= 1;
	texdesc.SampleDesc.Quality	= 0;
	texdesc.Format				= DXGI_FORMAT_D32_FLOAT;
	texdesc.Usage				= D3D10_USAGE_DEFAULT;
	texdesc.BindFlags			= D3D10_BIND_DEPTH_STENCIL;
	texdesc.CPUAccessFlags		= 0;
	texdesc.MiscFlags			= D3D10_RESOURCE_MISC_TEXTURECUBE;

	MYVALID(device->CreateTexture2D(&texdesc, NULL, &diffcubemapds));

	texdesc.Format				= DXGI_FORMAT_R16G16B16A16_FLOAT;
	texdesc.BindFlags			= D3D10_BIND_RENDER_TARGET|D3D10_BIND_SHADER_RESOURCE;
	
	MYVALID(device->CreateTexture2D(&texdesc, NULL, &diffcubemap));

	dsvdesc.Format							= DXGI_FORMAT_D32_FLOAT;
	dsvdesc.ViewDimension					= D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
	dsvdesc.Texture2DArray.FirstArraySlice	= 0;
	dsvdesc.Texture2DArray.ArraySize		= 6;
	dsvdesc.Texture2DArray.MipSlice			= 0;

	rtvdesc.Format							= texdesc.Format;
	rtvdesc.ViewDimension					= D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvdesc.Texture2DArray.FirstArraySlice	= 0;
	rtvdesc.Texture2DArray.ArraySize		= 6;
	rtvdesc.Texture2DArray.MipSlice			= 0;

	MYVALID(device->CreateDepthStencilView(diffcubemapds, &dsvdesc, &cubedsv));
	MYVALID(device->CreateRenderTargetView(diffcubemap, &rtvdesc, &cubertv));

	// render
	viewport.Width = viewport.Height = CUBEMAP_DIFF_SIZE;

	device->OMSetRenderTargets(1, &cubertv, cubedsv);
	device->RSSetViewports(1, &viewport);
	device->ClearRenderTargetView(cubertv, color);
	device->ClearDepthStencilView(cubedsv, D3D10_CLEAR_DEPTH|D3D10_CLEAR_STENCIL, 1.0f, 0);
	{
		device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		device->IASetInputLayout(vertexlayout1);

		prefilter->GetVariableByName("envmap")->AsShaderResource()->SetResource(skytexture);
		prefilter->GetVariableByName("matCubeViews")->AsMatrix()->SetMatrixArray((float*)&views[0], 0, 6);
		prefilter->GetVariableByName("matProj")->AsMatrix()->SetMatrix((float*)&proj);
		prefilter->GetVariableByName("matWorld")->AsMatrix()->SetMatrix((float*)&world);
		prefilter->GetVariableByName("eyePos")->AsVector()->SetFloatVector((float*)&eye);

		prefiltertech->GetPassByIndex(0)->Apply(0);
		sky->DrawSubset(0);
	}

	D3DX10SaveTextureToFile(diffcubemap, D3DX10_IFF_DDS, "../media/textures/" FILENAME "_diff_irrad.dds");

	SAFE_RELEASE(cubertv);
	SAFE_RELEASE(cubedsv);
	SAFE_RELEASE(diffcubemap);
	SAFE_RELEASE(diffcubemapds);
#endif

#ifdef GENERATE_SPEC_IRRAD
	ID3D10RenderTargetView*	cubertvs[CUBEMAP_SPEC_MIPS] = { 0 };
	ID3D10DepthStencilView*	cubedsvs[CUBEMAP_SPEC_MIPS] = { 0 };

	ID3D10Texture2D* cubemap = 0;
	ID3D10Texture2D* cubemapds = 0;
	
	prefiltertech = prefilter->GetTechniqueByName("prefilter_spec");

	texdesc.Width				= CUBEMAP_SPEC_SIZE;
	texdesc.Height				= CUBEMAP_SPEC_SIZE;
	texdesc.MipLevels			= 0;
	texdesc.ArraySize			= 6;
	texdesc.SampleDesc.Count	= 1;
	texdesc.SampleDesc.Quality	= 0;
	texdesc.Format				= DXGI_FORMAT_D32_FLOAT;
	texdesc.Usage				= D3D10_USAGE_DEFAULT;
	texdesc.BindFlags			= D3D10_BIND_DEPTH_STENCIL;
	texdesc.CPUAccessFlags		= 0;
	texdesc.MiscFlags			= D3D10_RESOURCE_MISC_TEXTURECUBE;

	MYVALID(device->CreateTexture2D(&texdesc, NULL, &cubemapds));

	texdesc.Format				= DXGI_FORMAT_R16G16B16A16_FLOAT;
	texdesc.BindFlags			= D3D10_BIND_RENDER_TARGET|D3D10_BIND_SHADER_RESOURCE;
	texdesc.MiscFlags			= D3D10_RESOURCE_MISC_GENERATE_MIPS|D3D10_RESOURCE_MISC_TEXTURECUBE;

	MYVALID(device->CreateTexture2D(&texdesc, NULL, &cubemap));

	dsvdesc.Format							= DXGI_FORMAT_D32_FLOAT;
	dsvdesc.ViewDimension					= D3D10_DSV_DIMENSION_TEXTURE2DARRAY;
	dsvdesc.Texture2DArray.FirstArraySlice	= 0;
	dsvdesc.Texture2DArray.ArraySize		= 6;

	rtvdesc.Format							= texdesc.Format;
	rtvdesc.ViewDimension					= D3D10_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvdesc.Texture2DArray.FirstArraySlice	= 0;
	rtvdesc.Texture2DArray.ArraySize		= 6;

	for( UINT i = 0; i < CUBEMAP_SPEC_MIPS; ++i )
	{
		rtvdesc.Texture2DArray.MipSlice = i;
		dsvdesc.Texture2DArray.MipSlice = i;

		MYVALID(device->CreateDepthStencilView(cubemapds, &dsvdesc, &cubedsvs[i]));
		MYVALID(device->CreateRenderTargetView(cubemap, &rtvdesc, &cubertvs[i]));
	}

	// render
	for( UINT i = 0; i < CUBEMAP_SPEC_MIPS; ++i )
	{
		viewport.Width = viewport.Height =
			max(1, CUBEMAP_SPEC_SIZE / (1 << i));

		float roughness = (i + 0.5f) / CUBEMAP_SPEC_MIPS;

		device->OMSetRenderTargets(1, &cubertvs[i], cubedsvs[i]);
		device->RSSetViewports(1, &viewport);
		device->ClearRenderTargetView(cubertvs[i], color);
		device->ClearDepthStencilView(cubedsvs[i], D3D10_CLEAR_DEPTH|D3D10_CLEAR_STENCIL, 1.0f, 0);
		{
			device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			device->IASetInputLayout(vertexlayout1);

			prefilter->GetVariableByName("envmap")->AsShaderResource()->SetResource(skytexture);
			prefilter->GetVariableByName("matCubeViews")->AsMatrix()->SetMatrixArray((float*)&views[0], 0, 6);
			prefilter->GetVariableByName("matProj")->AsMatrix()->SetMatrix((float*)&proj);
			prefilter->GetVariableByName("matWorld")->AsMatrix()->SetMatrix((float*)&world);
			prefilter->GetVariableByName("eyePos")->AsVector()->SetFloatVector((float*)&eye);
			prefilter->GetVariableByName("roughness")->AsScalar()->SetFloat(roughness);

			prefiltertech->GetPassByIndex(0)->Apply(0);
			sky->DrawSubset(0);
		}
	}

	D3DX10SaveTextureToFile(cubemap, D3DX10_IFF_DDS, "../media/textures/" FILENAME "_spec_irrad.dds");

	for( UINT i = 0; i < CUBEMAP_SPEC_MIPS; ++i )
	{
		SAFE_RELEASE(cubertvs[i]);
		SAFE_RELEASE(cubedsvs[i]);
	}

	SAFE_RELEASE(cubemap);
	SAFE_RELEASE(cubemapds);
#endif

#if defined(GENERATE_DIFF_IRRAD) || defined(GENERATE_SPEC_IRRAD)
	SAFE_RELEASE(prefilter);
#endif

#ifdef GENERATE_BRDF
	ID3D10RenderTargetView*	rtv					= 0;
	ID3D10DepthStencilView*	dsv					= 0;
	ID3D10Effect*			integratebrdf		= 0;
	ID3D10Texture2D*		brdftex				= 0;
	ID3D10Texture2D*		brdfds				= 0;
	ID3D10EffectTechnique*	integratebrdftech	= 0;

	std::cout << "Compiling 'integratebrdf10.fx' ...\n";

	MYVALID(DXCreateEffect("../media/shaders10/integratebrdf10.fx", &integratebrdf));
	integratebrdftech = integratebrdf->GetTechniqueByName("integratebrdf");

	texdesc.Width				= 256;
	texdesc.Height				= 256;
	texdesc.MipLevels			= 1;
	texdesc.ArraySize			= 1;
	texdesc.SampleDesc.Count	= 1;
	texdesc.SampleDesc.Quality	= 0;
	texdesc.Format				= DXGI_FORMAT_D32_FLOAT;
	texdesc.Usage				= D3D10_USAGE_DEFAULT;
	texdesc.BindFlags			= D3D10_BIND_DEPTH_STENCIL;
	texdesc.CPUAccessFlags		= 0;
	texdesc.MiscFlags			= 0;

	MYVALID(device->CreateTexture2D(&texdesc, NULL, &brdfds));

	texdesc.Format				= DXGI_FORMAT_R16G16_FLOAT;
	texdesc.BindFlags			= D3D10_BIND_RENDER_TARGET|D3D10_BIND_SHADER_RESOURCE;
	texdesc.MiscFlags			= 0;

	MYVALID(device->CreateTexture2D(&texdesc, NULL, &brdftex));

	dsvdesc.Format				= DXGI_FORMAT_D32_FLOAT;
	dsvdesc.ViewDimension		= D3D10_DSV_DIMENSION_TEXTURE2D;
	dsvdesc.Texture2D.MipSlice	= 0;

	rtvdesc.Format				= texdesc.Format;
	rtvdesc.ViewDimension		= D3D10_RTV_DIMENSION_TEXTURE2D;
	rtvdesc.Texture2D.MipSlice	= 0;

	MYVALID(device->CreateDepthStencilView(brdfds, &dsvdesc, &dsv));
	MYVALID(device->CreateRenderTargetView(brdftex, &rtvdesc, &rtv));

	viewport.Width		= 256;
	viewport.Height		= 256;

	device->OMSetRenderTargets(1, &rtv, dsv);
	device->RSSetViewports(1, &viewport);
	device->ClearRenderTargetView(rtv, color);
	device->ClearDepthStencilView(dsv, D3D10_CLEAR_DEPTH|D3D10_CLEAR_STENCIL, 1.0f, 0);

	device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	device->IASetInputLayout(0);

	integratebrdftech->GetPassByIndex(0)->Apply(0);
	device->Draw(4, 0);

	D3DX10SaveTextureToFile(brdftex, D3DX10_IFF_DDS, "../media/textures/brdf.dds");

	SAFE_RELEASE(rtv);
	SAFE_RELEASE(dsv);
	SAFE_RELEASE(brdftex);
	SAFE_RELEASE(brdfds);
	SAFE_RELEASE(integratebrdf);
#endif

#if defined(GENERATE_BRDF) || defined(GENERATE_SPEC_IRRAD) || defined(GENERATE_DIFF_IRRAD)
	viewport.Width = screenwidth;
	viewport.Height = screenheight;

	device->OMSetRenderTargets(1, &rendertargetview, depthstencilview);
	device->RSSetViewports(1, &viewport);
#endif

#ifdef CONVERT_CUBEMAP
	ID3D10ShaderResourceView* map = 0;
	ID3D10Texture2D* tex = 0;

	MYVALID(D3DX10CreateShaderResourceViewFromFile(device, "../media/textures/uffizi.dds", 0, 0, &map, 0));

	map->GetResource((ID3D10Resource**)&tex);
	D3DX10SaveTextureToFile(tex, D3DX10_IFF_DDS, "../media/textures/uffizi_converted.dds");
	
	tex->Release();
	map->Release();
#endif

	MYVALID(D3DX10CreateShaderResourceViewFromFile(device, "../media/textures/" FILENAME "_diff_irrad.dds", 0, 0, &integrateddiff, 0));
	MYVALID(D3DX10CreateShaderResourceViewFromFile(device, "../media/textures/" FILENAME "_spec_irrad.dds", 0, 0, &integratedspec, 0));
	MYVALID(D3DX10CreateShaderResourceViewFromFile(device, "../media/textures/brdf.dds", 0, 0, &integratedbrdf, 0));

	return S_OK;
}
//*************************************************************************************************************
void UninitScene()
{
	SAFE_RELEASE(vertexlayout1);
	SAFE_RELEASE(vertexlayout1);
	SAFE_RELEASE(vertexlayout2);
	SAFE_RELEASE(object);
	SAFE_RELEASE(sky);
	SAFE_RELEASE(skyeffect);
	SAFE_RELEASE(metal);
	SAFE_RELEASE(skytexture);
	SAFE_RELEASE(integrateddiff);
	SAFE_RELEASE(integratedspec);
	SAFE_RELEASE(integratedbrdf);
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
}
//*************************************************************************************************************
void Render(float alpha, float elapsedtime)
{
	static float time = 0;

	D3DXMATRIX		view, proj, viewproj;
	D3DXMATRIX		world;
	D3DXMATRIX		inv;

	D3DXVECTOR3		eye(0, 0, -1);
	D3DXVECTOR3		look(0, 0, 0);
	D3DXVECTOR3		up(0, 1, 0);
	D3DXVECTOR2		orient = cameraangle.smooth(alpha);
	float			color[4] = { 0.0f, 0.125f, 0.3f, 1.0f };

	time += elapsedtime;

	// setup things
	float aspect	= (float)screenwidth / (float)screenheight;
	float fovy		= (float)D3DX_PI / 4.0f;
	float tfovx		= aspect * tanf(fovy * 0.5f);

	eye.z = -((NUM_SPHERES * 0.5f + SPHERE_SPACING * (NUM_SPHERES + 1) * 0.5f) / tfovx);

	D3DXMatrixRotationYawPitchRoll(&view, orient.x, orient.y, 0);
	D3DXVec3TransformCoord(&eye, &eye, &view);

	D3DXMatrixLookAtLH(&view, &eye, &look, &up);
	D3DXMatrixPerspectiveFovLH(&proj, fovy, aspect, 0.1f, 50);

	proj._33 = 1;
	proj._43 = -0.1f;

	//float width = NUM_SPHERES + (NUM_SPHERES + 1) * SPHERE_SPACING;
	//D3DXMatrixOrthoLH(&proj, width, width / aspect, -2, 2);

	D3DXMatrixMultiply(&viewproj, &view, &proj);

	device->ClearRenderTargetView(rendertargetview, color);
	device->ClearDepthStencilView(depthstencilview, D3D10_CLEAR_DEPTH|D3D10_CLEAR_STENCIL, 1.0f, 0);
	{
		device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		device->IASetInputLayout(vertexlayout1);

		// sky
		D3DXMatrixScaling(&world, 20, 20, 20);

		skyeffect->GetVariableByName("envmap")->AsShaderResource()->SetResource(skytexture);
		skyeffect->GetVariableByName("matViewProj")->AsMatrix()->SetMatrix((float*)&viewproj);
		skyeffect->GetVariableByName("matWorld")->AsMatrix()->SetMatrix((float*)&world);
		skyeffect->GetVariableByName("eyePos")->AsVector()->SetFloatVector((float*)&eye);
		skyeffect->GetVariableByName("gamma")->AsScalar()->SetFloat(2.2f);

		technique1->GetPassByIndex(0)->Apply(0);
		sky->DrawSubset(0);

		// spheres
		device->IASetInputLayout(vertexlayout2);

		for( int i = 0; i < NUM_SPHERES; ++i )
		{
			float roughness = (float)i / (NUM_SPHERES - 1);

			D3DXMatrixScaling(&world, 1, 1, 1);
			world._41 = (i - (NUM_SPHERES - 1) * 0.5f) - ((NUM_SPHERES - 1) * 0.5f) * SPHERE_SPACING + i * SPHERE_SPACING;

			D3DXMatrixInverse(&inv, 0, &world);

			metal->GetVariableByName("irradCubeDiff")->AsShaderResource()->SetResource(integrateddiff);
			metal->GetVariableByName("irradCubeSpec")->AsShaderResource()->SetResource(integratedspec);
			metal->GetVariableByName("brdfLUT")->AsShaderResource()->SetResource(integratedbrdf);
			
			metal->GetVariableByName("matViewProj")->AsMatrix()->SetMatrix((float*)&viewproj);
			metal->GetVariableByName("matWorld")->AsMatrix()->SetMatrix((float*)&world);
			metal->GetVariableByName("matWorldInv")->AsMatrix()->SetMatrix((float*)&inv);
			
			metal->GetVariableByName("eyePos")->AsVector()->SetFloatVector((float*)&eye);
			metal->GetVariableByName("gamma")->AsScalar()->SetFloat(2.2f);
			metal->GetVariableByName("roughness")->AsScalar()->SetFloat(roughness);

			technique2->GetPassByIndex(0)->Apply(0);
			object->DrawSubset(0);
		}

	}
	swapchain->Present(0, 0);
}
//*************************************************************************************************************
