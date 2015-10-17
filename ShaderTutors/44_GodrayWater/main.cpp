
#include <iostream>
#include <string>

#include "../common/dxext.h"

// TODO: mask

// helper macros
#define TITLE				"Shader sample 44: Simple water and light shafts"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define MYVALID(x)			{ if( FAILED(hr = x) ) { MYERROR(#x); return hr; } }
#define SAFE_RELEASE(x)		{ if( (x) ) { (x)->Release(); (x) = NULL; } }

// external variables
extern long		screenwidth;
extern long		screenheight;
extern short	mousedx;
extern short	mousedy;
extern short	mousedown;

extern LPDIRECT3DDEVICE9 device;
extern HWND hwnd;

// sample variables
LPD3DXMESH						skymesh			= NULL;
LPD3DXEFFECT					skyeffect		= NULL;
LPD3DXEFFECT					ambient			= NULL;
LPD3DXEFFECT					specular		= NULL;
LPD3DXEFFECT					water			= NULL;
LPD3DXEFFECT					bloom			= NULL;
LPD3DXEFFECT					godray			= NULL;

DXObject*						palm			= NULL;
DXObject*						sandplane		= NULL;
DXObject*						waterplane		= NULL;

LPDIRECT3DTEXTURE9				refraction		= NULL;
LPDIRECT3DTEXTURE9				reflection		= NULL;
LPDIRECT3DTEXTURE9				occluders		= NULL;
LPDIRECT3DTEXTURE9				blurtex			= NULL;
LPDIRECT3DTEXTURE9				sceneldr		= NULL;
LPDIRECT3DTEXTURE9				bloomtex1		= NULL;
LPDIRECT3DTEXTURE9				bloomtex2		= NULL;

LPDIRECT3DTEXTURE9				bark			= NULL;
LPDIRECT3DTEXTURE9				leaves			= NULL;
LPDIRECT3DTEXTURE9				sand			= NULL;
LPDIRECT3DTEXTURE9				waves			= NULL;
LPDIRECT3DCUBETEXTURE9			skytex			= NULL;

LPDIRECT3DVERTEXDECLARATION9	quaddecl		= NULL;

LPDIRECT3DSURFACE9				refractsurf		= NULL;
LPDIRECT3DSURFACE9				reflectsurf		= NULL;
LPDIRECT3DSURFACE9				occludersurf	= NULL;
LPDIRECT3DSURFACE9				blursurf		= NULL;
LPDIRECT3DSURFACE9				sceneldrsurf	= NULL;
LPDIRECT3DSURFACE9				bloomsurf1		= NULL;
LPDIRECT3DSURFACE9				bloomsurf2		= NULL;

state<D3DXVECTOR2>				cameraangle;
D3DXVECTOR4						lightcolor(1, 1, 0.7f, 1);

float quadvertices[36] =
{
	-0.5f, -0.5f, 0, 1,		0, 0,
	-0.5f, -0.5f, 0, 1,		1, 0,
	-0.5f, -0.5f, 0, 1,		0, 1,

	-0.5f, -0.5f, 0, 1,		0, 1,
	-0.5f, -0.5f, 0, 1,		1, 0,
	-0.5f, -0.5f, 0, 1,		1, 1
};

HRESULT InitScene()
{
	HRESULT hr;
	D3DCAPS9 caps;

	SetWindowText(hwnd, TITLE);

	device->GetDeviceCaps(&caps);

	if( caps.VertexShaderVersion < D3DVS_VERSION(2, 0) || caps.PixelShaderVersion < D3DPS_VERSION(2, 0) )
	{
		MYERROR("This demo requires Shader Model 2.0 capable video card");
		return E_FAIL;
	}

	palm = new DXObject(device);
	sandplane = new DXObject(device);
	waterplane = new DXObject(device);

	if( !palm->Load("../media/meshes/palm.qm") )
	{
		MYERROR("Could not load palm");
		return E_FAIL;
	}

	if( !sandplane->CreatePlane(50, 50, 10, 10) )
	{
		MYERROR("Could not create sand plane");
		return E_FAIL;
	}

	if( !waterplane->CreatePlane(50, 50, 5, 5) )
	{
		MYERROR("Could not create water plane");
		return E_FAIL;
	}

	waterplane->GenerateTangentFrame();

	MYVALID(D3DXLoadMeshFromXA("../media/meshes/sky.X", D3DXMESH_MANAGED, device, NULL, NULL, NULL, NULL, &skymesh));
	MYVALID(D3DXCreateCubeTextureFromFileA(device, "../media/textures/sky7.dds", &skytex));

	MYVALID(D3DXCreateTextureFromFileA(device, "../media/textures/bark.jpg", &bark));
	MYVALID(D3DXCreateTextureFromFileA(device, "../media/textures/leaf.jpg", &leaves));
	MYVALID(D3DXCreateTextureFromFileA(device, "../media/textures/sand.jpg", &sand));
	MYVALID(D3DXCreateTextureFromFileA(device, "../media/textures/wave2.png", &waves));

	MYVALID(DXCreateEffect("../media/shaders/ambient.fx", device, &ambient));
	MYVALID(DXCreateEffect("../media/shaders/blinnphong.fx", device, &specular));
	MYVALID(DXCreateEffect("../media/shaders/water.fx", device, &water));
	MYVALID(DXCreateEffect("../media/shaders/simplebloom.fx", device, &bloom));
	MYVALID(DXCreateEffect("../media/shaders/godray.fx", device, &godray));
	MYVALID(DXCreateEffect("../media/shaders/sky.fx", device, &skyeffect));

	MYVALID(device->CreateTexture(screenwidth, screenheight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &refraction, NULL));
	MYVALID(device->CreateTexture(screenwidth, screenheight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A16B16G16R16F, D3DPOOL_DEFAULT, &reflection, NULL));
	MYVALID(device->CreateTexture(screenwidth, screenheight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8, D3DPOOL_DEFAULT, &occluders, NULL));
	MYVALID(device->CreateTexture(screenwidth, screenheight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8, D3DPOOL_DEFAULT, &blurtex, NULL));
	MYVALID(device->CreateTexture(screenwidth, screenheight, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &sceneldr, NULL));

	MYVALID(device->CreateTexture(screenwidth / 2, screenheight / 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &bloomtex1, NULL));
	MYVALID(device->CreateTexture(screenwidth / 2, screenheight / 2, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &bloomtex2, NULL));

	refraction->GetSurfaceLevel(0, &refractsurf);
	reflection->GetSurfaceLevel(0, &reflectsurf);
	occluders->GetSurfaceLevel(0, &occludersurf);
	blurtex->GetSurfaceLevel(0, &blursurf);
	sceneldr->GetSurfaceLevel(0, &sceneldrsurf);
	bloomtex1->GetSurfaceLevel(0, &bloomsurf1);
	bloomtex2->GetSurfaceLevel(0, &bloomsurf2);

	D3DVERTEXELEMENT9 elem[] =
	{
		{ 0, 0, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITIONT, 0 },
		{ 0, 16, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
		D3DDECL_END()
	};

	MYVALID(device->CreateVertexDeclaration(elem, &quaddecl));

	cameraangle = D3DXVECTOR2(-1.13f * D3DX_PI, 0.55f);

	return S_OK;
}

void UninitScene()
{
	delete palm;
	delete sandplane;
	delete waterplane;

	SAFE_RELEASE(refractsurf);
	SAFE_RELEASE(reflectsurf);
	SAFE_RELEASE(occludersurf);
	SAFE_RELEASE(blursurf);
	SAFE_RELEASE(sceneldrsurf);
	SAFE_RELEASE(bloomsurf1);
	SAFE_RELEASE(bloomsurf2);

	SAFE_RELEASE(refraction);
	SAFE_RELEASE(reflection);
	SAFE_RELEASE(occluders);
	SAFE_RELEASE(blurtex);
	SAFE_RELEASE(sceneldr);
	SAFE_RELEASE(bloomtex1);
	SAFE_RELEASE(bloomtex2);

	SAFE_RELEASE(skymesh);
	SAFE_RELEASE(quaddecl);

	SAFE_RELEASE(skyeffect);
	SAFE_RELEASE(ambient);
	SAFE_RELEASE(specular);
	SAFE_RELEASE(water);
	SAFE_RELEASE(bloom);
	SAFE_RELEASE(godray);

	SAFE_RELEASE(skytex);
	SAFE_RELEASE(bark);
	SAFE_RELEASE(leaves);
	SAFE_RELEASE(sand);
	SAFE_RELEASE(waves);

	DXKillAnyRogueObject();
}

void KeyPress(WPARAM wparam)
{
}

void Update(float delta)
{
	D3DXVECTOR2 velocity(mousedx, mousedy);

	cameraangle.prev = cameraangle.curr;

	if( mousedown == 1 )
		cameraangle.curr += velocity * 0.004f;

	// clamp to [-pi, pi]
	if( cameraangle.curr.y >= 1.5f )
		cameraangle.curr.y = 1.5f;

	if( cameraangle.curr.y <= -0.4f )
		cameraangle.curr.y = -0.4f;
}

void RenderScene(const D3DXMATRIX& viewproj, const D3DXVECTOR3& eye, const D3DXVECTOR4& lightpos, bool refl)
{
	D3DXMATRIX world, inv;

	// sky
	device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
	device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 1);

	D3DXMatrixScaling(&world, 20, 20, 20);
	skyeffect->SetMatrix("matWorld", &world);

	D3DXMatrixIdentity(&world);
	skyeffect->SetMatrix("matWorldSky", &world);

	skyeffect->SetMatrix("matViewProj", &viewproj);
	skyeffect->SetVector("eyePos", (D3DXVECTOR4*)&eye);

	skyeffect->Begin(0, 0);
	skyeffect->BeginPass(0);
	{
		device->SetTexture(0, skytex);
		skymesh->DrawSubset(0);
	}
	skyeffect->EndPass();
	skyeffect->End();

	device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);

	if( !refl )
	{
		// sand
		D3DXMatrixTranslation(&world, 0, -2, 0);

		ambient->SetMatrix("matViewProj", &viewproj);
		ambient->SetMatrix("matWorld", &world);

		ambient->Begin(0, 0);
		ambient->BeginPass(0);
		{
			device->SetTexture(0, sand);
			sandplane->DrawSubset(0, DXObject::Opaque);
		}
		ambient->EndPass();
		ambient->End();
	}
	else
		device->SetRenderState(D3DRS_CLIPPLANEENABLE, D3DCLIPPLANE0);

	// palm
	D3DXMatrixScaling(&world, 6, 6, 6);
	D3DXMatrixInverse(&inv, 0, &world);

	world._42 = -2;

	specular->SetMatrix("matViewProj", &viewproj);
	specular->SetMatrix("matWorld", &world);
	specular->SetMatrix("matWorldInv", &inv);
	specular->SetVector("eyePos", (D3DXVECTOR4*)&eye);
	specular->SetVector("lightPos", &lightpos);
	specular->SetVector("lightColor", &lightcolor);

	specular->Begin(0, 0);
	specular->BeginPass(0);
	{
		device->SetTexture(0, bark);
		palm->DrawSubset(0, DXObject::Opaque);

		device->SetTexture(0, leaves);
		palm->DrawSubset(1, DXObject::Opaque);
	}
	specular->EndPass();
	specular->End();

	device->SetSamplerState(0, D3DSAMP_SRGBTEXTURE, 0);

	if( refl )
		device->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
}

void RenderLightShafts(const D3DXMATRIX& view, const D3DXMATRIX& proj, const D3DXVECTOR3& eye, const D3DXVECTOR4& lightpos)
{
	D3DXMATRIX	world;
	D3DXVECTOR3	viewdir(view._13, view._23, view._33);
	D3DXVECTOR3	lightdir;
	D3DXVECTOR4	lightss;
	D3DXVECTOR4	texelsize(1.0f / screenwidth, 1.0f / screenheight, 0, 1);

	D3DXVec3Normalize(&lightdir, (D3DXVECTOR3*)&lightpos);
	float exposure = min(max(D3DXVec3Dot(&viewdir, &lightdir), 0), 1);

	lightss.x = eye.x + lightpos.x;
	lightss.y = eye.y + lightpos.y;
	lightss.z = eye.z + lightpos.z;
	lightss.w = 1;

	D3DXVec4Transform(&lightss, &lightss, &view);
	D3DXVec4Transform(&lightss, &lightss, &proj);

	lightss.x = (1.0f + lightss.x / lightss.w) * 0.5f;
	lightss.y = (1.0f - lightss.y / lightss.w) * 0.5f;

	device->SetRenderTarget(0, occludersurf);
	device->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 0x68686868, 1.0f, 0);

	D3DXMatrixScaling(&world, 6, 6, 6);
	world._42 = -2;

	device->SetTransform(D3DTS_WORLD, &world);
	device->SetTransform(D3DTS_VIEW, &view);
	device->SetTransform(D3DTS_PROJECTION, &proj);

	palm->DrawSubset(0, DXObject::Opaque);
	palm->DrawSubset(1, DXObject::Opaque);

	device->SetVertexDeclaration(quaddecl);
	device->SetRenderState(D3DRS_ZENABLE, FALSE);

	// first blur
	godray->SetTechnique("godray");
	godray->SetVector("lightPos", (D3DXVECTOR4*)&lightss);
	godray->SetFloat("exposure", exposure);

	device->SetRenderTarget(0, blursurf);

	godray->Begin(0, 0);
	godray->BeginPass(0);
	{
		device->SetTexture(0, occluders);
		device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quadvertices, 6 * sizeof(float));
	}
	godray->EndPass();
	godray->End();

	// second blur
	godray->SetTechnique("blur");
	godray->SetVector("texelSize", &texelsize);
	godray->SetVector("lightPos", (D3DXVECTOR4*)&lightss);

	device->SetRenderTarget(0, occludersurf);

	godray->Begin(0, 0);
	godray->BeginPass(0);
	{
		device->SetTexture(0, blurtex);
		device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quadvertices, 6 * sizeof(float));
	}
	godray->EndPass();
	godray->End();

	device->SetRenderState(D3DRS_ZENABLE, TRUE);
}

void Render(float alpha, float elapsedtime)
{
	static float time = 0;
	LPDIRECT3DSURFACE9 backbuffer = 0;

	D3DXMATRIX		view, proj, viewproj;
	D3DXMATRIX		world, inv;

	D3DXVECTOR4		texelsize;
	D3DXVECTOR4		lightpos(-600, 350, 1000, 1);
	D3DXVECTOR4		refllight;
	D3DXVECTOR3		eye(0, 0, -5.0f);
	D3DXVECTOR3		look(0, 1.2f, 0);
	D3DXVECTOR3		refleye, refllook;
	D3DXVECTOR3		up(0, 1, 0);
	D3DXVECTOR2		orient	= cameraangle.smooth(alpha);

	D3DXMatrixRotationYawPitchRoll(&view, orient.x, orient.y, 0);
	D3DXVec3TransformCoord(&eye, &eye, &view);
	
	eye.y += 1.2f;

	D3DXMatrixPerspectiveFovLH(&proj, D3DX_PI / 2, (float)screenwidth / (float)screenheight, 0.1f, 30);

	time += elapsedtime;

	if( SUCCEEDED(device->BeginScene()) )
	{
		device->GetRenderTarget(0, &backbuffer);

		// STEP 1: render reflection texture
		device->SetRenderTarget(0, reflectsurf);
		device->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 0xff6694ed, 1.0f, 0);

		D3DXPLANE plane(0, 1, 0, 1);

		refleye = eye - 2 * D3DXPlaneDotCoord(&plane, &eye) * (D3DXVECTOR3&)plane;
		refllook = look - 2 * D3DXPlaneDotCoord(&plane, &look) * (D3DXVECTOR3&)plane;

		refllight = lightpos - 2 * D3DXPlaneDot(&plane, &lightpos) * (D3DXVECTOR4&)plane;
		refllight.w = 1;

		D3DXMatrixLookAtLH(&view, &refleye, &refllook, &up);
		D3DXMatrixMultiply(&viewproj, &view, &proj);

		D3DXMatrixInverse(&inv, 0, &viewproj);
		D3DXMatrixTranspose(&inv, &inv);
		D3DXPlaneTransform(&plane, &plane, &inv);

		device->SetClipPlane(0, &plane.a);

		RenderScene(viewproj, refleye, refllight, true);

		// STEP 2: render scene (later used for refraction)
		D3DXMatrixLookAtLH(&view, &eye, &look, &up);
		D3DXMatrixMultiply(&viewproj, &view, &proj);

		device->SetRenderTarget(0, refractsurf);
		device->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER, 0xff6694ed, 1.0f, 0);

		RenderScene(viewproj, eye, lightpos, false);

		// render water surface into alpha channel for masking
		device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_ALPHA);

		D3DXMatrixTranslation(&world, 0, -1, 0);

		device->SetTransform(D3DTS_WORLD, &world);
		device->SetTransform(D3DTS_VIEW, &view);
		device->SetTransform(D3DTS_PROJECTION, &proj);

		waterplane->DrawSubset(0, DXObject::Opaque);

		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0x0f);

		// STEP 3: light shafts
		quadvertices[6] = quadvertices[24] = quadvertices[30]	= (float)screenwidth - 0.5f;
		quadvertices[13] = quadvertices[19] = quadvertices[31]	= (float)screenheight - 0.5f;

		RenderLightShafts(view, proj, eye, lightpos);

		// STEP 4: gamma correct
		device->SetRenderTarget(0, sceneldrsurf);

		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetVertexDeclaration(quaddecl);

		bloom->SetTechnique("gammacorrect");
		bloom->Begin(0, 0);
		bloom->BeginPass(0);
		{
			device->SetTexture(0, refraction);
			device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quadvertices, 6 * sizeof(float));
		}
		bloom->EndPass();
		bloom->End();

		device->SetRenderState(D3DRS_ZENABLE, TRUE);

		// STEP 5: water surface
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, TRUE);

		D3DXMatrixTranslation(&world, 0, -1, 0);
		D3DXMatrixIdentity(&inv);

		water->SetMatrix("matViewProj", &viewproj);
		water->SetMatrix("matWorld", &world);
		water->SetMatrix("matWorldInv", &inv);
		water->SetVector("eyePos", (D3DXVECTOR4*)&eye);
		water->SetVector("lightPos", &lightpos);
		water->SetVector("lightColor", &lightcolor);
		water->SetFloat("time", time);

		water->Begin(0, 0);
		water->BeginPass(0);
		{
			device->SetTexture(0, refraction);
			device->SetTexture(1, reflection);
			device->SetTexture(2, waves);

			waterplane->DrawSubset(0, DXObject::Opaque);
		}
		water->EndPass();
		water->End();

		device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);

		// STEP 6: downsample & blur
		quadvertices[6] = quadvertices[24] = quadvertices[30]	= (float)screenwidth * 0.5f - 0.5f;
		quadvertices[13] = quadvertices[19] = quadvertices[31]	= (float)screenheight * 0.5f - 0.5f;

		device->SetRenderTarget(0, bloomsurf1);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetVertexDeclaration(quaddecl);

		texelsize.x = 1.0f / screenwidth;
		texelsize.y = 1.0f / screenheight;

		bloom->SetTechnique("downsample");
		bloom->SetVector("texelSize", &texelsize);

		bloom->Begin(0, 0);
		bloom->BeginPass(0);
		{
			device->SetTexture(0, sceneldr);
			device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quadvertices, 6 * sizeof(float));
		}
		bloom->EndPass();
		bloom->End();

		device->SetRenderTarget(0, bloomsurf2);

		texelsize.x = 2.0f / screenwidth;
		texelsize.y = 2.0f / screenheight;

		bloom->SetTechnique("blur");
		bloom->SetVector("texelSize", &texelsize);

		bloom->Begin(0, 0);
		bloom->BeginPass(0);
		{
			device->SetTexture(0, bloomtex1);
			device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quadvertices, 6 * sizeof(float));
		}
		bloom->EndPass();
		bloom->End();

		// STEP 7: add light shafts
		quadvertices[6] = quadvertices[24] = quadvertices[30]	= (float)screenwidth - 0.5f;
		quadvertices[13] = quadvertices[19] = quadvertices[31]	= (float)screenheight - 0.5f;

		device->SetRenderTarget(0, backbuffer);

		godray->SetTechnique("final");
		godray->Begin(0, 0);
		godray->BeginPass(0);
		{
			device->SetTexture(0, sceneldr);
			device->SetTexture(1, occluders);
			device->SetTexture(2, bloomtex2);

			device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quadvertices, 6 * sizeof(float));
		}
		godray->EndPass();
		godray->End();

		backbuffer->Release();

		device->SetRenderState(D3DRS_ZENABLE, TRUE);
		device->EndScene();
	}

	device->Present(NULL, NULL, NULL, NULL);
}
