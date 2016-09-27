
#ifdef _DEBUG
#	pragma comment(lib, "glslangd.lib")
#	pragma comment(lib, "OGLCompilerd.lib")
#	pragma comment(lib, "OSDependentd.lib")
#	pragma comment(lib, "SPIRVd.lib")
//#	pragma comment(lib, "SPIRV-Toolsd.lib")
#	pragma comment(lib, "HLSLd.lib")
#else
#	pragma comment(lib, "glslang.lib")
#	pragma comment(lib, "OGLCompiler.lib")
#	pragma comment(lib, "OSDependent.lib")
#	pragma comment(lib, "SPIRV.lib")
//#	pragma comment(lib, "SPIRV-Tools.lib")
#	pragma comment(lib, "HLSL.lib")
#endif

#pragma comment(lib, "vulkan-1.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "GdiPlus.lib")
#pragma comment(lib, "Shlwapi.lib")

#include <iostream>
#include <cmath>
#include <Windows.h>
#include <GdiPlus.h>
#include <Shlwapi.h>

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#include "../common/vkx.h"

#ifdef _DEBUG
#	define ENABLE_VALIDATION
#endif

#define TITLE				"Asylum's Vulkan sample"
#define MYERROR(x)			{ std::cout << "* Error: " << x << "!\n"; }
#define V_RETURN(r, e, x)	{ if( !(x) ) { MYERROR(e); return r; }}
#define ARRAY_SIZE(x)		(sizeof(x) / sizeof(x[0]))

HWND		hwnd			= NULL;
HDC			hdc				= NULL;
RECT		workarea;
long		screenwidth		= 1024;
long		screenheight	= 576;
bool		uninited		= false;

struct InputState {
	unsigned char Button;
	short X, Y;
	int dX, dY;
} inputstate;

extern ULONG_PTR gdiplustoken;

// must be implemented by sample
extern bool InitScene();

extern void UninitScene();
extern void Update(float delta);
extern void Render(float alpha, float elapsedtime);

extern void Event_KeyDown(unsigned char keycode);
extern void Event_KeyUp(unsigned char keycode);
extern void Event_MouseMove(int x, int y, short dx, short dy);
extern void Event_MouseDown(int x, int y, unsigned char button);
extern void Event_MouseUp(int x, int y, unsigned char button);

static const char* instancelayers[] = {
	"VK_LAYER_LUNARG_standard_validation",
	//"VK_LAYER_RENDERDOC_Capture"
};

static const char* instanceextensions[] = {
	"VK_KHR_surface",
	"VK_KHR_win32_surface",
	"VK_EXT_debug_report"	// TODO: not part of standard
};

static const char* deviceextensions[] = {
	"VK_KHR_swapchain"
};

VKAPI_ATTR VkBool32 VKAPI_CALL DebugReportCallback(
	VkDebugReportFlagsEXT		flags,
	VkDebugReportObjectTypeEXT	objectType,
	uint64_t					object,
	size_t						location,
	int32_t						messageCode,
	const char*					pLayerPrefix,
	const char*					pMessage,
	void*						pUserData)
{
	// validation layer bug
	if( strcmp(pMessage, "Cannot clear attachment 1 with invalid first layout VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL.") == 0 )
		return VK_FALSE;

	std::cerr << pMessage << std::endl;
	__debugbreak();

	return VK_FALSE;
}

static bool IsDeviceAcceptable(VkPhysicalDevice device)
{
	VkPhysicalDeviceProperties deviceprops;
	vkGetPhysicalDeviceProperties(device, &deviceprops);

	return (deviceprops.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
}

bool InitVK(HWND hwnd)
{
	VkApplicationInfo		app_info	= {};
	VkInstanceCreateInfo	inst_info	= {};
	VkResult				res;

	uint32_t				numlayers = 0;
	uint32_t				numsupportedlayers = 0;
	uint32_t				numtestlayers = ARRAY_SIZE(instancelayers);
	VkLayerProperties*		layerprops = 0;

	// look for layers
	vkEnumerateInstanceLayerProperties(&numlayers, 0);
	layerprops = new VkLayerProperties[numlayers];

	vkEnumerateInstanceLayerProperties(&numlayers, layerprops);

	for( uint32_t i = 0; i < numtestlayers; )
	{
		uint32_t found = UINT32_MAX;

		for( uint32_t j = 0; j < numlayers; ++j )
		{
			if( 0 == strcmp(layerprops[j].layerName, instancelayers[i]) )
			{
				found = j;
				break;
			}
		}

		if( found == UINT32_MAX )
		{
			// unsupported
			printf("Layer '%s' unsupported\n", instancelayers[i]);

			std::swap(instancelayers[i], instancelayers[numtestlayers - 1]);
			--numtestlayers;
		}
		else
		{
			++numsupportedlayers;
			++i;
		}
	}

	delete[] layerprops;

	// create instance
	app_info.sType						= VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pNext						= NULL;
	app_info.pApplicationName			= TITLE;
	app_info.applicationVersion			= 1;
	app_info.pEngineName				= "Asylum's sample engine";
	app_info.engineVersion				= 1;
	app_info.apiVersion					= VK_API_VERSION_1_0;

	inst_info.sType						= VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	inst_info.pNext						= NULL;
	inst_info.flags						= 0;
	inst_info.pApplicationInfo			= &app_info;
	inst_info.ppEnabledExtensionNames	= instanceextensions;
	inst_info.ppEnabledLayerNames		= instancelayers;

#ifdef ENABLE_VALIDATION
	inst_info.enabledExtensionCount		= ARRAY_SIZE(instanceextensions);
	inst_info.enabledLayerCount			= numsupportedlayers;
#else
	inst_info.enabledExtensionCount		= ARRAY_SIZE(instanceextensions) - 1;
	inst_info.enabledLayerCount			= 0;
#endif

	res = vkCreateInstance(&inst_info, NULL, &driverinfo.inst);

	V_RETURN(false, "Could not find Vulkan driver", res != VK_ERROR_INCOMPATIBLE_DRIVER);
	V_RETURN(false, "Some layers are not present, remove them from the code", res != VK_ERROR_LAYER_NOT_PRESENT);
	V_RETURN(false, "Some extensions are not present, remove them from the code", res != VK_ERROR_EXTENSION_NOT_PRESENT);
	V_RETURN(false, "Unknown error", res == VK_SUCCESS);

#ifdef ENABLE_VALIDATION
	driverinfo.vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(driverinfo.inst, "vkCreateDebugReportCallbackEXT"));
	driverinfo.vkDebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(vkGetInstanceProcAddr(driverinfo.inst, "vkDebugReportMessageEXT"));
	driverinfo.vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(driverinfo.inst, "vkDestroyDebugReportCallbackEXT"));

	VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;

	callbackCreateInfo.sType		= VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
	callbackCreateInfo.pNext		= nullptr;
	callbackCreateInfo.flags		= VK_DEBUG_REPORT_ERROR_BIT_EXT|VK_DEBUG_REPORT_WARNING_BIT_EXT|VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
	callbackCreateInfo.pfnCallback	= &DebugReportCallback;
	callbackCreateInfo.pUserData	= nullptr;

	res = driverinfo.vkCreateDebugReportCallbackEXT(driverinfo.inst, &callbackCreateInfo, nullptr, &driverinfo.callback);
	VK_ASSERT(res == VK_SUCCESS);
#endif

	// enumerate GPUs
	res = vkEnumeratePhysicalDevices(driverinfo.inst, &driverinfo.gpucount, 0);
	VK_ASSERT(driverinfo.gpucount > 0);

	driverinfo.gpus = new VkPhysicalDevice[driverinfo.gpucount];
	res = vkEnumeratePhysicalDevices(driverinfo.inst, &driverinfo.gpucount, driverinfo.gpus);

	uint32_t selectedgpu = UINT_MAX;
	
	for( uint32_t i = 0; i < driverinfo.gpucount; ++i ) {
		if( IsDeviceAcceptable(driverinfo.gpus[i]) ) {
			selectedgpu = i;
			break;
		}
	}

	VK_ASSERT(selectedgpu != UINT_MAX);

	vkGetPhysicalDeviceQueueFamilyProperties(driverinfo.gpus[selectedgpu], &driverinfo.queuecount, 0);
	VK_ASSERT(driverinfo.queuecount > 0);

	driverinfo.queueprops = new VkQueueFamilyProperties[driverinfo.queuecount];
	vkGetPhysicalDeviceQueueFamilyProperties(driverinfo.gpus[selectedgpu], &driverinfo.queuecount, driverinfo.queueprops);

	vkGetPhysicalDeviceProperties(driverinfo.gpus[selectedgpu], &driverinfo.deviceprops);
	vkGetPhysicalDeviceFeatures(driverinfo.gpus[selectedgpu], &driverinfo.devicefeatures);
	vkGetPhysicalDeviceMemoryProperties(driverinfo.gpus[selectedgpu], &driverinfo.memoryprops);

	// create surface
	VkWin32SurfaceCreateInfoKHR createinfo = {};

	createinfo.sType		= VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	createinfo.pNext		= NULL;
	createinfo.hinstance	= GetModuleHandle(NULL);
	createinfo.hwnd			= hwnd;

	driverinfo.presentmodecount = 0;

	res = vkCreateWin32SurfaceKHR(driverinfo.inst, &createinfo, NULL, &driverinfo.surface);
	VK_ASSERT(res == VK_SUCCESS);

	res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(driverinfo.gpus[selectedgpu], driverinfo.surface, &driverinfo.surfacecaps);
	VK_ASSERT(res == VK_SUCCESS);

	res = vkGetPhysicalDeviceSurfacePresentModesKHR(driverinfo.gpus[selectedgpu], driverinfo.surface, &driverinfo.presentmodecount, NULL);
	VK_ASSERT(driverinfo.presentmodecount > 0);

	driverinfo.presentmodes = new VkPresentModeKHR[driverinfo.presentmodecount];

	res = vkGetPhysicalDeviceSurfacePresentModesKHR(driverinfo.gpus[selectedgpu], driverinfo.surface, &driverinfo.presentmodecount, driverinfo.presentmodes);
	VK_ASSERT(res == VK_SUCCESS);

	// check for a presentation queue
	VkBool32* presentok = new VkBool32[driverinfo.queuecount];

	for( uint32_t i = 0; i < driverinfo.queuecount; ++i ) {
		vkGetPhysicalDeviceSurfaceSupportKHR(driverinfo.gpus[selectedgpu], i, driverinfo.surface, &presentok[i]);
	}

	driverinfo.graphicsqueueid = UINT32_MAX;
	driverinfo.computequeueid = UINT32_MAX;

	for( uint32_t i = 0; i < driverinfo.queuecount; ++i ) {
		if( (driverinfo.queueprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentok[i] == VK_TRUE )
			driverinfo.graphicsqueueid = i;

		if( driverinfo.queueprops[i].queueFlags & VK_QUEUE_COMPUTE_BIT )
			driverinfo.computequeueid = i;
	}
	
	V_RETURN(false, "Adapter does not have a queue that support graphics and presenting", driverinfo.graphicsqueueid != UINT32_MAX);

	// query formats
	uint32_t numformats;

	vkGetPhysicalDeviceSurfaceFormatsKHR(driverinfo.gpus[selectedgpu], driverinfo.surface, &numformats, 0);
	VK_ASSERT(numformats > 0);

	VkSurfaceFormatKHR* formats = new VkSurfaceFormatKHR[numformats];

	res = vkGetPhysicalDeviceSurfaceFormatsKHR(driverinfo.gpus[selectedgpu], driverinfo.surface, &numformats, formats);
	VK_ASSERT(res == VK_SUCCESS);

	driverinfo.format = VK_FORMAT_UNDEFINED;

	for( uint32_t i = 0; i < numformats; ++i )
	{
		if( formats[i].format == VK_FORMAT_B8G8R8A8_SRGB )
		{
			driverinfo.format = VK_FORMAT_B8G8R8A8_SRGB;
			break;
		}
	}

	if( driverinfo.format == VK_FORMAT_UNDEFINED )
		driverinfo.format = ((formats[0].format == VK_FORMAT_UNDEFINED) ? VK_FORMAT_B8G8R8A8_UNORM : formats[0].format);

	delete[] formats;
	delete[] presentok;

	// create device
	VkDeviceQueueCreateInfo	queueinfo		= {};
	VkDeviceCreateInfo		deviceinfo		= {};
	float					priorities[1]	= { 0.0f };

	queueinfo.sType						= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueinfo.pNext						= NULL;
	queueinfo.queueCount				= 1;
	queueinfo.pQueuePriorities			= priorities;
	queueinfo.queueFamilyIndex			= driverinfo.graphicsqueueid;

	deviceinfo.sType					= VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceinfo.pNext					= NULL;
	deviceinfo.queueCreateInfoCount		= 1;
	deviceinfo.pQueueCreateInfos		= &queueinfo;
	deviceinfo.enabledExtensionCount	= ARRAY_SIZE(deviceextensions);
	deviceinfo.ppEnabledLayerNames		= instancelayers;
	deviceinfo.ppEnabledExtensionNames	= deviceextensions;
	deviceinfo.pEnabledFeatures			= &driverinfo.devicefeatures;

#ifdef ENABLE_VALIDATION
	deviceinfo.enabledLayerCount		= numsupportedlayers;
#else
	deviceinfo.enabledLayerCount		= 0;
#endif

	res = vkCreateDevice(driverinfo.gpus[selectedgpu], &deviceinfo, NULL, &driverinfo.device);
	VK_ASSERT(res == VK_SUCCESS);

	vkGetDeviceQueue(driverinfo.device, driverinfo.graphicsqueueid, 0, &driverinfo.graphicsqueue);
	VK_ASSERT(driverinfo.graphicsqueue != VK_NULL_HANDLE);

	// create command pool
	VkCommandPoolCreateInfo poolinfo = {};

	poolinfo.sType				= VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolinfo.pNext				= NULL;
	poolinfo.queueFamilyIndex	= driverinfo.graphicsqueueid;
	poolinfo.flags				= VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

	res = vkCreateCommandPool(driverinfo.device, &poolinfo, NULL, &driverinfo.commandpool);
	VK_ASSERT(res == VK_SUCCESS);

	// create swapchain
	VkSwapchainCreateInfoKHR swapchaininfo = {};

	if( driverinfo.surfacecaps.currentExtent.width != -1 )
		screenwidth = driverinfo.surfacecaps.currentExtent.width;

	if( driverinfo.surfacecaps.currentExtent.height != -1 )
		screenheight = driverinfo.surfacecaps.currentExtent.height;

	swapchaininfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;

	for( uint32_t i = 0; i < driverinfo.presentmodecount; ++i )
	{
		if( driverinfo.presentmodes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR )
			swapchaininfo.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

		if( driverinfo.presentmodes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
		{
			// mailbox is best
			swapchaininfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}

	swapchaininfo.sType					= VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchaininfo.pNext					= NULL;
	swapchaininfo.surface				= driverinfo.surface;
	swapchaininfo.minImageCount			= 2;
	swapchaininfo.imageFormat			= driverinfo.format;
	swapchaininfo.imageExtent.width		= screenwidth;
	swapchaininfo.imageExtent.height	= screenheight;
	swapchaininfo.preTransform			= VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	swapchaininfo.compositeAlpha		= VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchaininfo.imageArrayLayers		= 1;
	swapchaininfo.oldSwapchain			= VK_NULL_HANDLE;
	swapchaininfo.clipped				= true;
	swapchaininfo.imageColorSpace		= VK_COLORSPACE_SRGB_NONLINEAR_KHR;
	swapchaininfo.imageUsage			= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	swapchaininfo.imageSharingMode		= VK_SHARING_MODE_EXCLUSIVE;
	swapchaininfo.queueFamilyIndexCount	= 0;
	swapchaininfo.pQueueFamilyIndices	= NULL;

	res = vkCreateSwapchainKHR(driverinfo.device, &swapchaininfo, NULL, &driverinfo.swapchain);
	VK_ASSERT(res == VK_SUCCESS);

	res = vkGetSwapchainImagesKHR(driverinfo.device, driverinfo.swapchain, &driverinfo.swapchainimgcount, NULL);
	VK_ASSERT(res == VK_SUCCESS);

	driverinfo.swapchainimages = new VkImage[driverinfo.swapchainimgcount];
	driverinfo.swapchainimageviews = new VkImageView[driverinfo.swapchainimgcount];

	res = vkGetSwapchainImagesKHR(driverinfo.device, driverinfo.swapchain, &driverinfo.swapchainimgcount, driverinfo.swapchainimages);
	VK_ASSERT(res == VK_SUCCESS);

	for( uint32_t i = 0; i < driverinfo.swapchainimgcount; ++i )
	{
		VkImageViewCreateInfo color_image_view = {};

		color_image_view.sType								= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		color_image_view.pNext								= NULL;
		color_image_view.format								= driverinfo.format;
		color_image_view.components.r						= VK_COMPONENT_SWIZZLE_R;
		color_image_view.components.g						= VK_COMPONENT_SWIZZLE_G;
		color_image_view.components.b						= VK_COMPONENT_SWIZZLE_B;
		color_image_view.components.a						= VK_COMPONENT_SWIZZLE_A;
		color_image_view.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		color_image_view.subresourceRange.baseMipLevel		= 0;
		color_image_view.subresourceRange.levelCount		= 1;
		color_image_view.subresourceRange.baseArrayLayer	= 0;
		color_image_view.subresourceRange.layerCount		= 1;
		color_image_view.viewType							= VK_IMAGE_VIEW_TYPE_2D;
		color_image_view.flags								= 0;
		color_image_view.image								= driverinfo.swapchainimages[i];

		res = vkCreateImageView(driverinfo.device, &color_image_view, NULL, &driverinfo.swapchainimageviews[i]);
		VK_ASSERT(res == VK_SUCCESS);
	}

	/*
	// pipeline cache
	VkPipelineCacheCreateInfo pipelinecacheinfo = {};

	pipelinecacheinfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	pipelinecacheinfo.pNext				= NULL;
	pipelinecacheinfo.initialDataSize	= 0;
	pipelinecacheinfo.pInitialData		= NULL;
	pipelinecacheinfo.flags				= 0;

	res = vkCreatePipelineCache(driverinfo.device, &pipelinecacheinfo, NULL, &driverinfo.pipelinecache);
	VK_ASSERT(res == VK_SUCCESS);
	*/

	glslang::InitializeProcess();
	return true;
}

void UninitVK()
{
	if( !uninited )
	{
		uninited = true;

		glslang::FinalizeProcess();
		UninitScene();

		VulkanContentRegistry::Release();
		VulkanMemorySubAllocator::Release();

		for( uint32_t i = 0; i < driverinfo.swapchainimgcount; ++i ) {
			vkDestroyImageView(driverinfo.device, driverinfo.swapchainimageviews[i], 0);
		}

		delete[] driverinfo.swapchainimageviews;
		delete[] driverinfo.swapchainimages;
		delete[] driverinfo.presentmodes;
		delete[] driverinfo.queueprops;
		delete[] driverinfo.gpus;

		//vkDestroyPipelineCache(driverinfo.device, driverinfo.pipelinecache, 0);
		vkDestroySwapchainKHR(driverinfo.device, driverinfo.swapchain, 0);
		vkDestroyCommandPool(driverinfo.device, driverinfo.commandpool, 0);
		vkDestroyDevice(driverinfo.device, 0);
		vkDestroySurfaceKHR(driverinfo.inst, driverinfo.surface, 0);

#ifdef ENABLE_VALIDATION
		driverinfo.vkDestroyDebugReportCallbackEXT(driverinfo.inst, driverinfo.callback, 0);
#endif

		vkDestroyInstance(driverinfo.inst, NULL);
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
		UninitVK();
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

	case WM_LBUTTONDOWN:
		inputstate.Button |= 1;
		Event_MouseDown(inputstate.X, inputstate.Y, 1);
		break;

	case WM_RBUTTONDOWN:
		inputstate.Button |= 2;
		Event_MouseDown(inputstate.X, inputstate.Y, 2);
		break;

	case WM_LBUTTONUP:
		Event_MouseUp(inputstate.X, inputstate.Y, inputstate.Button & 1);
		inputstate.Button &= (~1);
		break;

	case WM_RBUTTONUP:
		Event_MouseUp(inputstate.X, inputstate.Y, inputstate.Button & 2);
		inputstate.Button &= (~2);
		break;

	default:
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void AssembleSPIRV(std::string& result, const std::string& file)
{
	std::wstring aspath;
	std::wstring exedir;
	std::wstring filepath;

	WCHAR szExePath[MAX_PATH];
	WCHAR szFilePath[MAX_PATH];
	DWORD num = GetModuleFileNameW(GetModuleHandleW(NULL), szExePath, MAX_PATH);
	
	if( num == 0 )
		return;

	// convert relative path to wchar
	int size = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), (int)file.length(), 0, 0);
	
	filepath.resize(size);
	MultiByteToWideChar(CP_UTF8, 0, file.c_str(), (int)file.length(), &filepath[0], size);

	std::for_each(filepath.begin(), filepath.end(), [](wchar_t& ch) {
		if( ch == '/' )
			ch = '\\';
	});

	// find executable dir
	szExePath[num] = 0;

	aspath.resize(num + 1, 0);
	wcscpy_s(&aspath[0], num + 1, szExePath);

	size_t pos = aspath.find_last_of('\\');
	exedir = aspath.substr(0, pos + 1);

	// find assembler path
	pos = aspath.find(L"ShaderTutors");

	aspath = aspath.substr(0, pos + 13);
	aspath += L"extern\\bin\\spirv-as.exe";

	// find result path
	result.clear();
	result.resize(MAX_PATH);

	GetCurrentDirectoryA(MAX_PATH, &result[0]);
	
	pos = result.find_first_of('\0');
	result.resize(pos);

	result += "\\out.spv";

	if( 0 == DeleteFileA(result.c_str()) ) {
		DWORD err = GetLastError();
		VK_ASSERT(err == ERROR_FILE_NOT_FOUND);
	}

	// get full path for file
	PathCombineW(szFilePath, exedir.c_str(), filepath.c_str());
	
	// execute assembler
	SHELLEXECUTEINFOW execinfo;

	memset(&execinfo, 0, sizeof(SHELLEXECUTEINFOW));

	execinfo.cbSize			= sizeof(SHELLEXECUTEINFOW);
	execinfo.lpFile			= aspath.c_str();
	execinfo.lpParameters	= szFilePath;
	execinfo.lpVerb			= L"open";
	execinfo.nShow			= SW_HIDE;
	execinfo.hwnd			= hwnd;
	execinfo.hInstApp		= (HINSTANCE)SE_ERR_DDEFAIL;
	execinfo.fMask			= SEE_MASK_NOCLOSEPROCESS;
	
	BOOL success = ShellExecuteExW(&execinfo);

	if( execinfo.hProcess != NULL ) {
		WaitForSingleObject(execinfo.hProcess, INFINITE);
		CloseHandle(execinfo.hProcess);
	}
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

int main(int argc, char* argv[])
{
	//::_crtBreakAlloc = 5741;
	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

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

	if( !InitVK(hwnd) )
	{
		MYERROR("Failed to initialize Vulkan");
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
			}

			if( !uninited )
				Update(0.1f);
		}

		if( !uninited )
			Render((float)accum / 0.1f, (float)delta);
	}

_end:
	if( gdiplustoken )
		Gdiplus::GdiplusShutdown(gdiplustoken);

	std::cout << "Exiting...\n";

	UnregisterClass("TestClass", wc.hInstance);
	_CrtDumpMemoryLeaks();

	CoUninitialize();

#ifdef _DEBUG
	system("pause");
#endif

	return 0;
}
