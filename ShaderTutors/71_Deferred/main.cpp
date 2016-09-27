
#include <iostream>
#include <cassert>

#include "../common/vkx.h"
#include "../common/spectatorcamera.h"

#define METERS_PER_UNIT					0.01f
#define NUM_LIGHTS						512
#define LIGHT_RADIUS					2.0f

#define UNIFORM_BUFFER_SIZE				2048
#define GBUFFER_PASS_UNIFORM_OFFSET		0
#define ACCUM_PASS_UNIFORM_OFFSET		256
#define FORWARD_PASS_UNIFORM_OFFSET		1024
#define FLARES_PASS_UNIFORM_OFFSET		1280

// TODO:
// - alias
// - tempsubmit leak
// - uniform buffer duplication?

extern long screenwidth;
extern long screenheight;

struct GBufferPassUniformData {
	float world[16];
	float worldinv[16];
	float viewproj[16];
};

struct AccumPassUniformData {
	float view[16];
	float proj[16];
	float viewproj[16];
	float viewprojinv[16];
	float eyepos[4];
	float clip[4];
};

struct ForwardPassUniformData {
	float world[16];
	float viewproj[16];
};

struct FlaresPassUniformData {
	float view[16];
	float proj[16];
	float params[4];
};

struct LightParticle {
	VulkanColor	color;
	float		previous[4];
	float		current[4];
	float		velocity[3];
	float		radius;
};

VkRenderPass			mainrenderpass		= 0;
VkFramebuffer*			framebuffers		= 0;

VulkanRenderPass*		gbufferrenderpass	= 0;
VulkanImage*			gbuffernormals		= 0;
VulkanImage*			gbufferdepth		= 0;
VulkanGraphicsPipeline*	gbufferpasspipeline	= 0;

VulkanImage*			accumdiffirrad		= 0;
VulkanImage*			accumspecirrad		= 0;
VulkanComputePipeline*	accumpasspipeline	= 0;

VulkanGraphicsPipeline*	forwardpasspipeline	= 0;

VulkanImage*			tonemapinput		= 0;
VulkanGraphicsPipeline*	tonemappasspipeline	= 0;

VulkanImage*			flaretexture		= 0;
VulkanGraphicsPipeline*	flarespasspipeline	= 0;

VulkanBasicMesh*		model				= 0;
VulkanBasicMesh*		screenquad			= 0;
VulkanBuffer*			uniforms			= 0;
VulkanBuffer*			lightbuffer			= 0;
VulkanImage*			depthbuffer			= 0;
VulkanImage*			supplytexture		= 0;
VulkanImage*			supplynormalmap		= 0;
VulkanImage*			ambientcube			= 0;
VulkanFramePump*		framepump			= 0;
VulkanAABox				particlevolume(-13.2f, 2.0f, -5.7f, 13.2f, 14.4f, 5.7f);

SpectatorCamera			camera;
uint32_t				currentphysicsframe	= 0;

void InitializeGBufferPass();
void InitializeAccumPass();
void InitializeForwardPass();
void InitializeTonemapPass();
void InitializeFlaresPass();

void GenerateParticles();
void UpdateParticles(float dt);

bool InitScene()
{
	VkAttachmentDescription	rpattachments[5];
	VkImageView				fbattachments[5];

	VkAttachmentReference	color0reference		= {};
	VkAttachmentReference	color1reference		= {};
	VkAttachmentReference	forwardreferences[2];
	VkAttachmentReference	tonemapreference	= {};
	VkAttachmentReference	depthreference		= {};

	VkSubpassDescription	subpasses[2];
	VkSubpassDependency		dependencies[2];
	VkRenderPassCreateInfo	renderpassinfo		= {};
	VkFramebufferCreateInfo	framebufferinfo		= {};
	VkResult				res;

	framepump = new VulkanFramePump();

	// create main render pass
	VkFormat depthformat = VK_FORMAT_D24_UNORM_S8_UINT;

	if( VulkanQueryFormatSupport(VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) )
		depthformat = VK_FORMAT_D32_SFLOAT_S8_UINT;

	depthbuffer = VulkanImage::Create2D(depthformat, screenwidth, screenheight, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
	VK_ASSERT(depthbuffer);

	rpattachments[0].format			= driverinfo.format;
	rpattachments[0].samples		= VK_SAMPLE_COUNT_1_BIT;
	rpattachments[0].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	rpattachments[0].storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	rpattachments[0].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	rpattachments[0].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[0].initialLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	rpattachments[0].finalLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	rpattachments[0].flags			= 0;

	rpattachments[1].format			= VK_FORMAT_R16G16B16A16_SFLOAT;
	rpattachments[1].samples		= VK_SAMPLE_COUNT_1_BIT;
	rpattachments[1].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	rpattachments[1].storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	rpattachments[1].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	rpattachments[1].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[1].initialLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	rpattachments[1].finalLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	rpattachments[1].flags			= 0;

	rpattachments[2].format			= VK_FORMAT_R16G16B16A16_SFLOAT;
	rpattachments[2].samples		= VK_SAMPLE_COUNT_1_BIT;
	rpattachments[2].loadOp			= VK_ATTACHMENT_LOAD_OP_LOAD;
	rpattachments[2].storeOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[2].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	rpattachments[2].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[2].initialLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	rpattachments[2].finalLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	rpattachments[2].flags			= 0;

	rpattachments[3].format			= VK_FORMAT_R16G16B16A16_SFLOAT;
	rpattachments[3].samples		= VK_SAMPLE_COUNT_1_BIT;
	rpattachments[3].loadOp			= VK_ATTACHMENT_LOAD_OP_LOAD;
	rpattachments[3].storeOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[3].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	rpattachments[3].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[3].initialLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	rpattachments[3].finalLayout	= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	rpattachments[3].flags			= 0;

	rpattachments[4].format			= depthbuffer->GetFormat();
	rpattachments[4].samples		= VK_SAMPLE_COUNT_1_BIT;
	rpattachments[4].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	rpattachments[4].storeOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[4].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_CLEAR;
	rpattachments[4].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[4].initialLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	rpattachments[4].finalLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	rpattachments[4].flags			= 0;

	color0reference.attachment		= 0;
	color0reference.layout			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	color1reference.attachment		= 1;
	color1reference.layout			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	tonemapreference.attachment		= 1;
	tonemapreference.layout			= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	forwardreferences[0].attachment	= 2;
	forwardreferences[0].layout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	forwardreferences[1].attachment	= 3;
	forwardreferences[1].layout		= VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	depthreference.attachment		= 4;
	depthreference.layout			= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	uint32_t preserve = 0;

	// forward pass renders into color1
	subpasses[0].pipelineBindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].flags						= 0;
	subpasses[0].inputAttachmentCount		= 2;
	subpasses[0].pInputAttachments			= forwardreferences;
	subpasses[0].colorAttachmentCount		= 1;
	subpasses[0].pColorAttachments			= &color1reference;
	subpasses[0].pResolveAttachments		= NULL;
	subpasses[0].pDepthStencilAttachment	= &depthreference;
	subpasses[0].preserveAttachmentCount	= 1;
	subpasses[0].pPreserveAttachments		= &preserve;

	// tonemap renders into color0
	subpasses[1].pipelineBindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[1].flags						= 0;
	subpasses[1].inputAttachmentCount		= 1;
	subpasses[1].pInputAttachments			= &tonemapreference;
	subpasses[1].colorAttachmentCount		= 1;
	subpasses[1].pColorAttachments			= &color0reference;
	subpasses[1].pResolveAttachments		= NULL;
	subpasses[1].pDepthStencilAttachment	= &depthreference;
	subpasses[1].preserveAttachmentCount	= 0;
	subpasses[1].pPreserveAttachments		= NULL;

	dependencies[0].srcSubpass		= VK_SUBPASS_EXTERNAL;
	dependencies[0].srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].srcStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dstSubpass		= 0;
	dependencies[0].dstAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[0].dstStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass		= 0;
	dependencies[1].srcAccessMask	= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dependencies[1].srcStageMask	= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstSubpass		= 1;
	dependencies[1].dstAccessMask	= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	dependencies[1].dstStageMask	= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dependencies[1].dependencyFlags	= VK_DEPENDENCY_BY_REGION_BIT;

	renderpassinfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpassinfo.pNext			= NULL;
	renderpassinfo.attachmentCount	= VK_ARRAY_SIZE(rpattachments);
	renderpassinfo.pAttachments		= rpattachments;
	renderpassinfo.subpassCount		= 2;
	renderpassinfo.pSubpasses		= subpasses;
	renderpassinfo.dependencyCount	= VK_ARRAY_SIZE(dependencies);
	renderpassinfo.pDependencies	= dependencies;

	res = vkCreateRenderPass(driverinfo.device, &renderpassinfo, NULL, &mainrenderpass);
	VK_ASSERT(res == VK_SUCCESS);

	// load model
	model = VulkanBasicMesh::LoadFromQM("../media/meshes/sponza/sponza.qm");
	VK_ASSERT(model);

	std::cout << "Generating tangent frame...\n";
	model->GenerateTangentFrame();

	// create screenquad
	screenquad = new VulkanBasicMesh(4, 6, sizeof(VulkanScreenVertex));

	VulkanScreenVertex* vdata = (VulkanScreenVertex*)screenquad->GetVertexBufferPointer();
	uint16_t* idata = (uint16_t*)screenquad->GetIndexBufferPointer();

	vdata[0].x = -1;	vdata[0].y = 1;		vdata[0].z = 0;	vdata[0].u = 0;	vdata[0].v = 1;
	vdata[1].x = -1;	vdata[1].y = -1;	vdata[1].z = 0;	vdata[1].u = 0;	vdata[1].v = 0;
	vdata[2].x = 1;		vdata[2].y = 1;		vdata[2].z = 0;	vdata[2].u = 1;	vdata[2].v = 1;
	vdata[3].x = 1;		vdata[3].y = -1;	vdata[3].z = 0;	vdata[3].u = 1;	vdata[3].v = 0;

	idata[0] = 0;	idata[3] = 1;
	idata[1] = 1;	idata[4] = 3;
	idata[2] = 2;	idata[5] = 2;

	// create uniform buffer
	uniforms = VulkanBuffer::Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_NUM_QUEUED_FRAMES * UNIFORM_BUFFER_SIZE, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// create substitute textures
	supplytexture = VulkanImage::CreateFromFile("../media/textures/vk_logo.jpg", true);
	VK_ASSERT(supplytexture);

	supplynormalmap = VulkanImage::Create2D(VK_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT);
	VK_ASSERT(supplynormalmap);

	uint32_t* data = (uint32_t*)supplynormalmap->MapContents(0, 0);
	{
		*data = 0xff7f7fff;	// (0.5f, 0.5f, 1)
	}
	supplynormalmap->UnmapContents();

	// load other textures
	ambientcube = VulkanImage::CreateFromDDSCubemap("../media/textures/uffizi_diff_irrad.dds", false);
	VK_ASSERT(ambientcube);

	flaretexture = VulkanImage::CreateFromFile("../media/textures/flare1.png", true);
	VK_ASSERT(flaretexture);

	// generate particles
	GenerateParticles();

	// synchronize memory to device
	VulkanPipelineBarrierBatch	barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	VkCommandBuffer				copycmd = VulkanCreateTempCommandBuffer();
	{
		model->UploadToVRAM(copycmd);
		screenquad->UploadToVRAM(copycmd);

		supplytexture->UploadToVRAM(copycmd);
		supplynormalmap->UploadToVRAM(copycmd);
		ambientcube->UploadToVRAM(copycmd);
		flaretexture->UploadToVRAM(copycmd);

		barrier.ImageLayoutTransfer(supplytexture, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		barrier.ImageLayoutTransfer(supplynormalmap, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		barrier.ImageLayoutTransfer(ambientcube, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		barrier.ImageLayoutTransfer(flaretexture, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	
		barrier.Enlist(copycmd);
	}
	VulkanSubmitTempCommandBuffer(copycmd);

	model->DeleteStagingBuffers();
	screenquad->DeleteStagingBuffers();

	supplytexture->DeleteStagingBuffer();
	supplytexture->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	supplynormalmap->DeleteStagingBuffer();
	supplynormalmap->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	ambientcube->DeleteStagingBuffer();
	ambientcube->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	flaretexture->DeleteStagingBuffer();
	flaretexture->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// now can initialize pipelines
	InitializeGBufferPass();
	InitializeAccumPass();
	InitializeForwardPass();
	InitializeTonemapPass();
	InitializeFlaresPass();

	// create main frame buffers
	framebuffers = new VkFramebuffer[driverinfo.swapchainimgcount];

	framebufferinfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferinfo.pNext			= NULL;
	framebufferinfo.renderPass		= mainrenderpass;
	framebufferinfo.attachmentCount	= VK_ARRAY_SIZE(fbattachments);
	framebufferinfo.pAttachments	= fbattachments;
	framebufferinfo.width			= screenwidth;
	framebufferinfo.height			= screenheight;
	framebufferinfo.layers			= 1;

	fbattachments[1] = tonemapinput->GetImageView();
	fbattachments[2] = accumdiffirrad->GetImageView();
	fbattachments[3] = accumspecirrad->GetImageView();
	fbattachments[4] = depthbuffer->GetImageView();

	for( uint32_t i = 0; i < driverinfo.swapchainimgcount; ++i )
	{
		fbattachments[0] = driverinfo.swapchainimageviews[i];

		res = vkCreateFramebuffer(driverinfo.device, &framebufferinfo, NULL, &framebuffers[i]);
		VK_ASSERT(res == VK_SUCCESS);
	}

	// setup camera
	camera.Fov = VKDegreesToRadians(60);
	camera.Aspect = (float)screenwidth / (float)screenheight;
	camera.Far = 50.0f;

	camera.SetEyePosition(-0.12f, 3.0f, 0);
	//camera.SetEyePosition(-10.0f, 8.99f, -3.5f);
	camera.SetOrientation(-VK_HALF_PI, 0, 0);

	return true;
}

void InitializeGBufferPass()
{
	VkDescriptorBufferInfo gbufferuniforminfo(*uniforms->GetBufferInfo());

	gbufferuniforminfo.offset = GBUFFER_PASS_UNIFORM_OFFSET;
	gbufferuniforminfo.range = sizeof(GBufferPassUniformData);

	gbuffernormals = VulkanImage::Create2D(VK_FORMAT_R16G16B16A16_SFLOAT, screenwidth, screenheight, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_STORAGE_BIT);
	gbufferdepth = VulkanImage::Create2D(VK_FORMAT_R32_SFLOAT, screenwidth, screenheight, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_STORAGE_BIT);

	// render pass
	gbufferrenderpass = new VulkanRenderPass(screenwidth, screenheight);

	gbufferrenderpass->AttachImage(
		gbuffernormals,
		VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	gbufferrenderpass->AttachImage(
		gbufferdepth,
		VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	gbufferrenderpass->AttachImage(
		depthbuffer,
		VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_DONT_CARE,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	VK_ASSERT(gbufferrenderpass->Assemble());

	// pipeline
	gbufferpasspipeline = new VulkanGraphicsPipeline();

	VK_ASSERT(gbufferpasspipeline->AddShader(VK_SHADER_STAGE_VERTEX_BIT, "../media/shadersVK/gbuffer.vert"));
	VK_ASSERT(gbufferpasspipeline->AddShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../media/shadersVK/gbuffer.frag"));

	gbufferpasspipeline->SetInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);	// position
	gbufferpasspipeline->SetInputAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12);	// normal
	gbufferpasspipeline->SetInputAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, 24);		// texcoord
	gbufferpasspipeline->SetInputAttribute(3, 0, VK_FORMAT_R32G32B32_SFLOAT, 32);	// tangent
	gbufferpasspipeline->SetInputAttribute(4, 0, VK_FORMAT_R32G32B32_SFLOAT, 44);	// bitangent

	gbufferpasspipeline->SetVertexInputBinding(0, VulkanMakeBindingDescription(0, VK_VERTEX_INPUT_RATE_VERTEX, model->GetVertexStride()));

	gbufferpasspipeline->SetDescriptorSetLayoutBufferBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	gbufferpasspipeline->SetDescriptorSetLayoutImageBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	gbufferpasspipeline->AllocateDescriptorSets(model->GetNumSubsets());

	for( uint32_t i = 0; i < model->GetNumSubsets(); ++i ) {
		const VulkanMaterial& material = model->GetMaterial(i);

		gbufferpasspipeline->SetDescriptorSetGroupBufferInfo(0, 0, &gbufferuniforminfo);

		if( material.NormalMap )
			gbufferpasspipeline->SetDescriptorSetGroupImageInfo(0, 1, material.NormalMap->GetImageInfo());
		else
			gbufferpasspipeline->SetDescriptorSetGroupImageInfo(0, 1, supplynormalmap->GetImageInfo());

		gbufferpasspipeline->UpdateDescriptorSet(0, i);
	}

	gbufferpasspipeline->SetBlendState(0, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO);
	gbufferpasspipeline->SetBlendState(1, VK_FALSE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO);

	gbufferpasspipeline->SetDepth(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
	gbufferpasspipeline->SetViewport(0, 0, (float)screenwidth, (float)screenheight);
	gbufferpasspipeline->SetScissor(0, 0, screenwidth, screenheight);

	VK_ASSERT(gbufferpasspipeline->Assemble(gbufferrenderpass->GetRenderPass()));
}

void InitializeAccumPass()
{
	VkDescriptorBufferInfo accumuniforminfo(*uniforms->GetBufferInfo());

	accumuniforminfo.offset = ACCUM_PASS_UNIFORM_OFFSET;
	accumuniforminfo.range = sizeof(AccumPassUniformData);

	accumdiffirrad = VulkanImage::Create2D(VK_FORMAT_R16G16B16A16_SFLOAT, screenwidth, screenheight, 1, VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
	accumspecirrad = VulkanImage::Create2D(VK_FORMAT_R16G16B16A16_SFLOAT, screenwidth, screenheight, 1, VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

	VK_ASSERT(accumdiffirrad);
	VK_ASSERT(accumspecirrad);

	// pipeline
	VulkanSpecializationInfo specinfo;

	specinfo.AddUInt(1, NUM_LIGHTS);

	accumpasspipeline = new VulkanComputePipeline();
	VK_ASSERT(accumpasspipeline->AddShader(VK_SHADER_STAGE_COMPUTE_BIT, "../media/shadersVK/deferredaccum.comp", specinfo));

	accumpasspipeline->SetDescriptorSetLayoutBufferBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT);
	accumpasspipeline->SetDescriptorSetLayoutImageBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
	accumpasspipeline->SetDescriptorSetLayoutImageBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
	accumpasspipeline->SetDescriptorSetLayoutImageBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
	accumpasspipeline->SetDescriptorSetLayoutImageBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT);
	accumpasspipeline->SetDescriptorSetLayoutImageBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT);
	accumpasspipeline->SetDescriptorSetLayoutBufferBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_COMPUTE_BIT);

	VkDescriptorImageInfo imginfo1 = *gbuffernormals->GetImageInfo();
	VkDescriptorImageInfo imginfo2 = *gbufferdepth->GetImageInfo();
	VkDescriptorImageInfo imginfo3 = *accumdiffirrad->GetImageInfo();
	VkDescriptorImageInfo imginfo4 = *accumspecirrad->GetImageInfo();
	VkDescriptorImageInfo imginfo5 = *ambientcube->GetImageInfo();

	imginfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imginfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imginfo3.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imginfo4.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
	imginfo5.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkDescriptorBufferInfo buffinfo1 = *lightbuffer->GetBufferInfo();
	buffinfo1.range = NUM_LIGHTS * sizeof(LightParticle);

	accumpasspipeline->AllocateDescriptorSets(1);
	{
		accumpasspipeline->SetDescriptorSetGroupBufferInfo(0, 0, &accumuniforminfo);
		accumpasspipeline->SetDescriptorSetGroupImageInfo(0, 1, &imginfo1);
		accumpasspipeline->SetDescriptorSetGroupImageInfo(0, 2, &imginfo2);
		accumpasspipeline->SetDescriptorSetGroupImageInfo(0, 3, &imginfo3);
		accumpasspipeline->SetDescriptorSetGroupImageInfo(0, 4, &imginfo4);
		accumpasspipeline->SetDescriptorSetGroupImageInfo(0, 5, &imginfo5);
		accumpasspipeline->SetDescriptorSetGroupBufferInfo(0, 6, &buffinfo1);
	}
	accumpasspipeline->UpdateDescriptorSet(0, 0);

	VK_ASSERT(accumpasspipeline->Assemble());
}

void InitializeForwardPass()
{
	VkDescriptorBufferInfo forwarduniforminfo(*uniforms->GetBufferInfo());

	forwarduniforminfo.offset = FORWARD_PASS_UNIFORM_OFFSET;
	forwarduniforminfo.range = sizeof(ForwardPassUniformData);

	forwardpasspipeline = new VulkanGraphicsPipeline();

	VK_ASSERT(forwardpasspipeline->AddShader(VK_SHADER_STAGE_VERTEX_BIT, "../media/shadersVK/forward.vert"));
	VK_ASSERT(forwardpasspipeline->AddShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../media/shadersVK/forward.frag"));

	forwardpasspipeline->SetInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);	// position
	forwardpasspipeline->SetInputAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12);	// normal
	forwardpasspipeline->SetInputAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, 24);		// texcoord

	forwardpasspipeline->SetVertexInputBinding(0, VulkanMakeBindingDescription(0, VK_VERTEX_INPUT_RATE_VERTEX, model->GetVertexStride()));
	
	forwardpasspipeline->SetDescriptorSetLayoutBufferBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	forwardpasspipeline->SetDescriptorSetLayoutImageBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	forwardpasspipeline->SetDescriptorSetLayoutImageBinding(2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);
	forwardpasspipeline->SetDescriptorSetLayoutImageBinding(3, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorImageInfo imginfo1 = *accumdiffirrad->GetImageInfo();
	VkDescriptorImageInfo imginfo2 = *accumspecirrad->GetImageInfo();

	imginfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imginfo2.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	forwardpasspipeline->AllocateDescriptorSets(model->GetNumSubsets());

	for( uint32_t i = 0; i < model->GetNumSubsets(); ++i ) {
		const VulkanMaterial& material = model->GetMaterial(i);

		forwardpasspipeline->SetDescriptorSetGroupBufferInfo(0, 0, &forwarduniforminfo);

		if( material.Texture )
			forwardpasspipeline->SetDescriptorSetGroupImageInfo(0, 1, material.Texture->GetImageInfo());
		else
			forwardpasspipeline->SetDescriptorSetGroupImageInfo(0, 1, supplytexture->GetImageInfo());

		forwardpasspipeline->SetDescriptorSetGroupImageInfo(0, 2, &imginfo1);
		forwardpasspipeline->SetDescriptorSetGroupImageInfo(0, 3, &imginfo2);

		forwardpasspipeline->UpdateDescriptorSet(0, i);
	}

	forwardpasspipeline->SetDepth(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
	forwardpasspipeline->SetViewport(0, 0, (float)screenwidth, (float)screenheight);
	forwardpasspipeline->SetScissor(0, 0, screenwidth, screenheight);

	VK_ASSERT(forwardpasspipeline->Assemble(mainrenderpass));
}

void InitializeTonemapPass()
{
	//tonemapinput = VulkanImage::CreateAlias(gbuffernormals);
	tonemapinput = VulkanImage::Create2D(VK_FORMAT_R16G16B16A16_SFLOAT, screenwidth, screenheight, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
	VK_ASSERT(tonemapinput);

	tonemappasspipeline = new VulkanGraphicsPipeline();

	VK_ASSERT(tonemappasspipeline->AddShader(VK_SHADER_STAGE_VERTEX_BIT, "../media/shadersVK/basic2D.vert"));
	VK_ASSERT(tonemappasspipeline->AddShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../media/shadersVK/tonemap.frag"));

	tonemappasspipeline->SetInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);	// position
	tonemappasspipeline->SetInputAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, 12);		// texcoord

	tonemappasspipeline->SetVertexInputBinding(0, VulkanMakeBindingDescription(0, VK_VERTEX_INPUT_RATE_VERTEX, screenquad->GetVertexStride()));
	tonemappasspipeline->SetDescriptorSetLayoutImageBinding(0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT);

	tonemappasspipeline->AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64);

	VkDescriptorImageInfo imginfo1 = *tonemapinput->GetImageInfo();
	imginfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	tonemappasspipeline->AllocateDescriptorSets(1);
	tonemappasspipeline->SetDescriptorSetGroupImageInfo(0, 0, &imginfo1);
	tonemappasspipeline->UpdateDescriptorSet(0, 0);

	tonemappasspipeline->SetDepth(VK_FALSE, VK_FALSE, VK_COMPARE_OP_ALWAYS);
	tonemappasspipeline->SetViewport(0, 0, (float)screenwidth, (float)screenheight);
	tonemappasspipeline->SetScissor(0, 0, screenwidth, screenheight);

	VK_ASSERT(tonemappasspipeline->Assemble(mainrenderpass));
}

void InitializeFlaresPass()
{
	VkDescriptorBufferInfo flaresuniforminfo(*uniforms->GetBufferInfo());

	flaresuniforminfo.offset = FLARES_PASS_UNIFORM_OFFSET;
	flaresuniforminfo.range = sizeof(FlaresPassUniformData);

	flarespasspipeline = new VulkanGraphicsPipeline();

	VK_ASSERT(flarespasspipeline->AddShader(VK_SHADER_STAGE_VERTEX_BIT, "../media/shadersVK/flares.vert"));
	VK_ASSERT(flarespasspipeline->AddShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../media/shadersVK/flares.frag"));

	flarespasspipeline->SetInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);	// position
	flarespasspipeline->SetInputAttribute(1, 0, VK_FORMAT_R32G32_SFLOAT, 12);	// texcoord

	flarespasspipeline->SetVertexInputBinding(0, VulkanMakeBindingDescription(0, VK_VERTEX_INPUT_RATE_VERTEX, screenquad->GetVertexStride()));
	
	flarespasspipeline->SetDescriptorSetLayoutBufferBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	flarespasspipeline->SetDescriptorSetLayoutBufferBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT);
	flarespasspipeline->SetDescriptorSetLayoutImageBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	VkDescriptorImageInfo imginfo1 = *flaretexture->GetImageInfo();
	imginfo1.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	
	VkDescriptorBufferInfo buffinfo1 = *lightbuffer->GetBufferInfo();
	buffinfo1.range = NUM_LIGHTS * sizeof(LightParticle);

	flarespasspipeline->AllocateDescriptorSets(1);
	flarespasspipeline->SetDescriptorSetGroupBufferInfo(0, 0, &flaresuniforminfo);
	flarespasspipeline->SetDescriptorSetGroupBufferInfo(0, 1, &buffinfo1);
	flarespasspipeline->SetDescriptorSetGroupImageInfo(0, 2, &imginfo1);
	flarespasspipeline->UpdateDescriptorSet(0, 0);

	flarespasspipeline->SetBlendState(0, VK_TRUE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE);
	flarespasspipeline->SetDepth(VK_TRUE, VK_FALSE, VK_COMPARE_OP_LESS);
	flarespasspipeline->SetViewport(0, 0, (float)screenwidth, (float)screenheight);
	flarespasspipeline->SetScissor(0, 0, screenwidth, screenheight);

	VK_ASSERT(flarespasspipeline->Assemble(mainrenderpass));
}

void UninitScene()
{
	vkDeviceWaitIdle(driverinfo.device);

	VK_SAFE_RELEASE(gbuffernormals);
	VK_SAFE_RELEASE(gbufferdepth);
	VK_SAFE_DELETE(gbufferrenderpass);
	VK_SAFE_DELETE(gbufferpasspipeline);

	VK_SAFE_RELEASE(accumdiffirrad);
	VK_SAFE_RELEASE(accumspecirrad);
	VK_SAFE_DELETE(accumpasspipeline);

	VK_SAFE_DELETE(forwardpasspipeline);
	VK_SAFE_DELETE(tonemappasspipeline);
	VK_SAFE_DELETE(flarespasspipeline);

	VK_SAFE_RELEASE(flaretexture);
	VK_SAFE_RELEASE(ambientcube);
	VK_SAFE_RELEASE(supplynormalmap);
	VK_SAFE_RELEASE(supplytexture);
	VK_SAFE_RELEASE(tonemapinput);
	VK_SAFE_RELEASE(depthbuffer);

	VK_SAFE_DELETE(screenquad);
	VK_SAFE_DELETE(model);
	VK_SAFE_DELETE(uniforms);
	VK_SAFE_DELETE(lightbuffer);

	for( uint32_t i = 0; i < driverinfo.swapchainimgcount; ++i ) {
		if( framebuffers[i] )
			vkDestroyFramebuffer(driverinfo.device, framebuffers[i], 0);
	}

	delete[] framebuffers;

	if( mainrenderpass )
		vkDestroyRenderPass(driverinfo.device, mainrenderpass, 0);

	VK_SAFE_DELETE(framepump);
}

void GenerateParticles()
{
	static VulkanColor colors[] =
	{
		VulkanColor(1, 0, 0, 1),
		VulkanColor(0, 1, 0, 1),
		VulkanColor(0, 0, 1, 1),

		VulkanColor(1, 0, 1, 1),
		VulkanColor(0, 1, 1, 1),
		VulkanColor(1, 1, 0, 1),
	};

	lightbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_NUM_QUEUED_FRAMES * NUM_LIGHTS * sizeof(LightParticle), VK_MEMORY_PROPERTY_SHARED_BIT);
	VK_ASSERT(lightbuffer != NULL);

	LightParticle* particles = (LightParticle*)lightbuffer->MapContents(0, 0);

	for( int i = 0; i < NUM_LIGHTS; ++i ) {
		float x = VKRandomFloat();
		float y = VKRandomFloat();
		float z = VKRandomFloat();
		float v = VKRandomFloat();
		float d = ((rand() % 2) ? 1.0f : -1.0f);
		int c = rand() % VK_ARRAY_SIZE(colors);

		x = particlevolume.Min[0] + x * (particlevolume.Max[0] - particlevolume.Min[0]);
		y = particlevolume.Min[1] + y * (particlevolume.Max[1] - particlevolume.Min[1]);
		z = particlevolume.Min[2] + z * (particlevolume.Max[2] - particlevolume.Min[2]);
		v = 0.4f + v * (3.0f - 0.4f);

		for( int j = 0; j < VK_NUM_QUEUED_FRAMES; ++j ) {
			particles[i + j * NUM_LIGHTS].color	= colors[c];
			particles[i + j * NUM_LIGHTS].radius = LIGHT_RADIUS;

			VKVec4Set(particles[i + j * NUM_LIGHTS].previous, x, y, z, 1);
			VKVec4Set(particles[i + j * NUM_LIGHTS].current, x, y, z, 1);
			VKVec3Set(particles[i + j * NUM_LIGHTS].velocity, 0, v * d, 0);
		}
	}

	lightbuffer->UnmapContents();

	// upload
	VulkanPipelineBarrierBatch barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

	VkCommandBuffer copycmd = VulkanCreateTempCommandBuffer();
	{
		lightbuffer->UploadToVRAM(copycmd);

		barrier.BufferAccessBarrier(lightbuffer->GetBuffer(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
		barrier.Enlist(copycmd);
	}
	VulkanSubmitTempCommandBuffer(copycmd, false);
}

void UpdateParticles(float dt)
{
	float vy[3];
	float vx[3], vz[3];
	float b[3];
	float A[16], Ainv[16];
	float planes[6][4];
	float denom, energy;
	float toi, besttoi;
	float impulse, noise;
	float (*bestplane)[4];
	bool pastcollision;

	particlevolume.GetPlanes(planes);

	uint32_t prevphysicsframe = (currentphysicsframe + VK_NUM_QUEUED_FRAMES - 1) % VK_NUM_QUEUED_FRAMES;

	LightParticle* particles = (LightParticle*)lightbuffer->MapContents(0, 0);
	LightParticle* readparticles = particles + prevphysicsframe * NUM_LIGHTS;
	LightParticle* writeparticles = particles + currentphysicsframe * NUM_LIGHTS;

	for( int i = 0; i < NUM_LIGHTS; ++i ) {
		const LightParticle& oldp = readparticles[i];
		LightParticle& newp = writeparticles[i];

		VKVec3Assign(newp.velocity, oldp.velocity);
		VKVec3Assign(newp.previous, oldp.current);

		// integrate
		VKVec3Mad(newp.current, oldp.current, oldp.velocity, dt);

		// detect collision
		besttoi = 2;

		VKVec3Subtract(b, newp.current, newp.previous);

		for( int j = 0; j < 6; ++j ) {
			// use radius == 0.5
			denom = VKVec3Dot(b, planes[j]);
			pastcollision = (VKVec3Dot(newp.previous, planes[j]) + planes[j][3] < 0.5f);

			if( denom < -1e-4f ) {
				toi = (0.5f - VKVec3Dot(newp.previous, planes[j]) - planes[j][3]) / denom;

				if( ((toi <= 1 && toi >= 0) ||		// normal case
					(toi < 0 && pastcollision)) &&	// allow past collision
					toi < besttoi )
				{
					besttoi = toi;
					bestplane = &planes[j];
				}
			}
		}

		if( besttoi <= 1 ) {
			// resolve constraint
			newp.current[0] = (1 - besttoi) * newp.previous[0] + besttoi * newp.current[0];
			newp.current[1] = (1 - besttoi) * newp.previous[1] + besttoi * newp.current[1];
			newp.current[2] = (1 - besttoi) * newp.previous[2] + besttoi * newp.current[2];

			impulse = -VKVec3Dot(*bestplane, newp.velocity);

			// perturb normal vector
			noise = ((rand() % 100) / 100.0f) * VK_PI * 0.333333f - VK_PI * 0.166666f; // [-pi/6, pi/6]

			b[0] = cosf(noise + VK_PI * 0.5f);
			b[1] = cosf(noise);
			b[2] = 0;

			VKVec3Normalize(vy, (*bestplane));
			VKGetOrthogonalVectors(vx, vz, vy);

			A[0] = vx[0];	A[1] = vy[0];	A[2] = vz[0];	A[3] = 0;
			A[4] = vx[1];	A[5] = vy[1];	A[6] = vz[1];	A[7] = 0;
			A[8] = vx[2];	A[9] = vy[2];	A[10] = vz[2];	A[11] = 0;
			A[12] = 0;		A[13] = 0;		A[14] = 0;		A[15] = 1;

			VKMatrixInverse(Ainv, A);
			VKVec3Transform(vy, b, Ainv);

			energy = VKVec3Length(newp.velocity);

			newp.velocity[0] += 2 * impulse * vy[0];
			newp.velocity[1] += 2 * impulse * vy[1];
			newp.velocity[2] += 2 * impulse * vy[2];

			// must conserve energy
			VKVec3Normalize(newp.velocity, newp.velocity);

			newp.velocity[0] *= energy;
			newp.velocity[1] *= energy;
			newp.velocity[2] *= energy;
		}
	}

	lightbuffer->UnmapContents();

	// Update() is called before render
	VulkanPipelineBarrierBatch barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

	VkCommandBuffer copycmd = VulkanCreateTempCommandBuffer();
	{
		barrier.BufferAccessBarrier(lightbuffer->GetBuffer(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, currentphysicsframe * NUM_LIGHTS, NUM_LIGHTS * sizeof(LightParticle));
		barrier.Enlist(copycmd);

		lightbuffer->UploadToVRAM(copycmd);

		barrier.Reset();
		barrier.BufferAccessBarrier(lightbuffer->GetBuffer(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, currentphysicsframe * NUM_LIGHTS, NUM_LIGHTS * sizeof(LightParticle));
		barrier.Enlist(copycmd);
	}
	VulkanSubmitTempCommandBuffer(copycmd, false);

	currentphysicsframe = (currentphysicsframe + 1) % VK_NUM_QUEUED_FRAMES;
}

void Event_KeyDown(unsigned char keycode)
{
	camera.Event_KeyDown(keycode);
}

void Event_KeyUp(unsigned char keycode)
{
	if( keycode == 0x50 ) {
		float eye[3];

		camera.GetEyePosition(eye);
		printf("Camera pos is: (%.2f, %.2f, %.2f)\n", eye[0], eye[1], eye[2]);
	}

	camera.Event_KeyUp(keycode);
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	camera.Event_MouseMove(dx, dy);
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	camera.Event_MouseDown(button);
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	camera.Event_MouseUp(button);
}

void Update(float delta)
{
	UpdateParticles(delta);

	camera.Update(delta);
}

void Render(float alpha, float elapsedtime)
{
	static float time = 0;

	GBufferPassUniformData	gbufferpassuniforms;
	AccumPassUniformData	accumpassuniforms;
	ForwardPassUniformData	forwardpassuniforms;
	FlaresPassUniformData	flarespassuniforms;

	float	view[16];
	float	proj[16];
	float	texworld[16];
	float	eye[3];

	VKMatrixIdentity(texworld);

	// setup view transform
	camera.Animate(alpha);
	camera.GetViewMatrix(view);
	camera.GetProjectionMatrix(proj);
	camera.GetEyePosition(eye);

	VKMatrixMultiply(gbufferpassuniforms.viewproj, view, proj);

	// gbuffer pass uniforms (NOTE: transform model to origin + [model y starts at 0])
	VKMatrixScaling(gbufferpassuniforms.world, METERS_PER_UNIT, METERS_PER_UNIT, METERS_PER_UNIT);

	gbufferpassuniforms.world[12] = 60.518921f * METERS_PER_UNIT;
	gbufferpassuniforms.world[13] = (778.0f - 651.495361f) * METERS_PER_UNIT;
	gbufferpassuniforms.world[14] = -38.690552f * METERS_PER_UNIT;

	VKMatrixInverse(gbufferpassuniforms.worldinv, gbufferpassuniforms.world);

	// accumulation pass uniforms
	VKMatrixInverse(accumpassuniforms.viewprojinv, gbufferpassuniforms.viewproj);
	VKMatrixAssign(accumpassuniforms.viewproj, gbufferpassuniforms.viewproj);
	VKMatrixAssign(accumpassuniforms.proj, proj);
	VKMatrixAssign(accumpassuniforms.view, view);

	VKVec4Set(accumpassuniforms.eyepos, eye[0], eye[1], eye[2], 1);
	VKVec4Set(accumpassuniforms.clip, camera.Near, camera.Far, alpha, 0);

	// forward pass uniforms
	VKMatrixAssign(forwardpassuniforms.world, gbufferpassuniforms.world);
	VKMatrixAssign(forwardpassuniforms.viewproj, gbufferpassuniforms.viewproj);

	// flares pass uniforms
	VKMatrixAssign(flarespassuniforms.view, view);
	VKMatrixAssign(flarespassuniforms.proj, proj);
	VKVec4Set(flarespassuniforms.params, alpha, time, 0, 0);

	time += elapsedtime;

	// Vulkan declarations
	VkCommandBufferBeginInfo	begininfo			= {};
	VkRenderPassBeginInfo		passbegininfo		= {};
	VkImageMemoryBarrier		presentbarrier		= {};
	VkSubmitInfo				submitinfo			= {};
	VkPresentInfoKHR			presentinfo			= {};

	VkCommandBuffer				commandbuffer;
	VkClearValue				clearcolors[5];
	VkResult					res;
	uint32_t					currentimage;

	commandbuffer = framepump->GetNextCommandBuffer();

	// begin recording
	begininfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begininfo.pNext					= NULL;
	begininfo.flags					= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begininfo.pInheritanceInfo		= NULL;

	res = vkBeginCommandBuffer(commandbuffer, &begininfo);
	VK_ASSERT(res == VK_SUCCESS);

	VulkanPipelineBarrierBatch barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	{
		barrier.BufferAccessBarrier(uniforms->GetBuffer(), VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
	}
	barrier.Enlist(commandbuffer);

	vkCmdUpdateBuffer(commandbuffer, uniforms->GetBuffer(), GBUFFER_PASS_UNIFORM_OFFSET, sizeof(GBufferPassUniformData), (const uint32_t*)&gbufferpassuniforms);
	vkCmdUpdateBuffer(commandbuffer, uniforms->GetBuffer(), ACCUM_PASS_UNIFORM_OFFSET, sizeof(AccumPassUniformData), (const uint32_t*)&accumpassuniforms);
	vkCmdUpdateBuffer(commandbuffer, uniforms->GetBuffer(), FORWARD_PASS_UNIFORM_OFFSET, sizeof(ForwardPassUniformData), (const uint32_t*)&forwardpassuniforms);
	vkCmdUpdateBuffer(commandbuffer, uniforms->GetBuffer(), FLARES_PASS_UNIFORM_OFFSET, sizeof(FlaresPassUniformData), (const uint32_t*)&flarespassuniforms);

	// setup images
	currentimage = framepump->GetNextDrawable();

	barrier.Reset();
	{
		barrier.BufferAccessBarrier(uniforms->GetBuffer(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

		barrier.ImageLayoutTransfer(driverinfo.swapchainimages[currentimage], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		barrier.ImageLayoutTransfer(depthbuffer->GetImage(), 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
		
		barrier.ImageLayoutTransfer(gbuffernormals->GetImage(), 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		barrier.ImageLayoutTransfer(gbufferdepth->GetImage(), 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		// not used by gbuffer pass
		barrier.ImageLayoutTransfer(accumdiffirrad->GetImage(), 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		barrier.ImageLayoutTransfer(accumspecirrad->GetImage(), 0, VK_ACCESS_SHADER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		barrier.ImageLayoutTransfer(tonemapinput->GetImage(), 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	}
	barrier.Enlist(commandbuffer);

	// G-buffer pass
	gbufferrenderpass->Begin(commandbuffer, VK_SUBPASS_CONTENTS_INLINE, VulkanColor(0, 0, 0, 1), 1.0f, 0);
	{
		vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gbufferpasspipeline->GetPipeline());
		vkCmdSetViewport(commandbuffer, 0, 1, gbufferpasspipeline->GetViewport());
		vkCmdSetScissor(commandbuffer, 0, 1, gbufferpasspipeline->GetScissor());

		model->Draw(commandbuffer, gbufferpasspipeline);
	}
	gbufferrenderpass->End(commandbuffer);

	gbuffernormals->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	gbufferdepth->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// accumulation pass
	uint32_t prevphysicsframe = (currentphysicsframe + VK_NUM_QUEUED_FRAMES - 1) % VK_NUM_QUEUED_FRAMES;
	uint32_t lightbufferoffset = prevphysicsframe * NUM_LIGHTS * sizeof(LightParticle);

	vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, accumpasspipeline->GetPipeline());
	vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_COMPUTE, accumpasspipeline->GetPipelineLayout(), 0, 1, accumpasspipeline->GetDescriptorSets(0), 1, &lightbufferoffset);

	uint32_t workgroupsx = (screenwidth + (screenwidth % 16)) / 16;
	uint32_t workgroupsy = (screenheight + (screenheight % 16)) / 16;

	vkCmdDispatch(commandbuffer, workgroupsx, workgroupsy, 1);

	barrier.Reset(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	{
		barrier.ImageLayoutTransfer(accumdiffirrad->GetImage(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		barrier.ImageLayoutTransfer(accumspecirrad->GetImage(), VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}
	barrier.Enlist(commandbuffer);

	accumdiffirrad->StoreLayout(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	accumspecirrad->StoreLayout(VK_ACCESS_INPUT_ATTACHMENT_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// must clear all attachments
	clearcolors[0].color.float32[0] = 0.017f;
	clearcolors[0].color.float32[1] = 0;
	clearcolors[0].color.float32[2] = 0;
	clearcolors[0].color.float32[3] = 1;

	clearcolors[1].color.float32[0] = 0.017f;
	clearcolors[1].color.float32[1] = 0;
	clearcolors[1].color.float32[2] = 0;
	clearcolors[1].color.float32[3] = 1;

	clearcolors[2].color.float32[0] = 0.017f;
	clearcolors[2].color.float32[1] = 0;
	clearcolors[2].color.float32[2] = 0;
	clearcolors[2].color.float32[3] = 1;

	clearcolors[3].color.float32[0] = 0.017f;
	clearcolors[3].color.float32[1] = 0;
	clearcolors[3].color.float32[2] = 0;
	clearcolors[3].color.float32[3] = 1;

	clearcolors[4].depthStencil.depth = 1.0f;
	clearcolors[4].depthStencil.stencil = 0;

	passbegininfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passbegininfo.pNext						= NULL;
	passbegininfo.renderPass				= mainrenderpass;
	passbegininfo.framebuffer				= framebuffers[currentimage];
	passbegininfo.renderArea.offset.x		= 0;
	passbegininfo.renderArea.offset.y		= 0;
	passbegininfo.renderArea.extent.width	= screenwidth;
	passbegininfo.renderArea.extent.height	= screenheight;
	passbegininfo.clearValueCount			= VK_ARRAY_SIZE(clearcolors);
	passbegininfo.pClearValues				= clearcolors;

	vkCmdBeginRenderPass(commandbuffer, &passbegininfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		// forward pass
		vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, forwardpasspipeline->GetPipeline());
		vkCmdSetViewport(commandbuffer, 0, 1, forwardpasspipeline->GetViewport());
		vkCmdSetScissor(commandbuffer, 0, 1, forwardpasspipeline->GetScissor());

		model->Draw(commandbuffer, forwardpasspipeline);

		// tonemap pass
		vkCmdNextSubpass(commandbuffer, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemappasspipeline->GetPipeline());
		vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemappasspipeline->GetPipelineLayout(), 0, 1, tonemappasspipeline->GetDescriptorSets(0), 0, NULL);
		vkCmdSetViewport(commandbuffer, 0, 1, tonemappasspipeline->GetViewport());
		vkCmdSetScissor(commandbuffer, 0, 1, tonemappasspipeline->GetScissor());
		vkCmdPushConstants(commandbuffer, tonemappasspipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 64, &texworld);

		screenquad->Draw(commandbuffer, NULL);

		// render flares
		vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, flarespasspipeline->GetPipeline());
		vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, flarespasspipeline->GetPipelineLayout(), 0, 1, flarespasspipeline->GetDescriptorSets(0), 1, &lightbufferoffset);
		vkCmdSetViewport(commandbuffer, 0, 1, flarespasspipeline->GetViewport());
		vkCmdSetScissor(commandbuffer, 0, 1, flarespasspipeline->GetScissor());

		screenquad->DrawSubsetInstanced(commandbuffer, 0, NULL, NUM_LIGHTS);
	}
	vkCmdEndRenderPass(commandbuffer);

	// add a barrier for present
	presentbarrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	presentbarrier.pNext							= NULL;
	presentbarrier.srcAccessMask					= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	presentbarrier.dstAccessMask					= VK_ACCESS_MEMORY_READ_BIT;
	presentbarrier.oldLayout						= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	presentbarrier.newLayout						= VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	presentbarrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
	presentbarrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
	presentbarrier.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
	presentbarrier.subresourceRange.baseMipLevel	= 0;
	presentbarrier.subresourceRange.levelCount		= 1;
	presentbarrier.subresourceRange.baseArrayLayer	= 0;
	presentbarrier.subresourceRange.layerCount		= 1;
	presentbarrier.image							= driverinfo.swapchainimages[currentimage];

	vkCmdPipelineBarrier(commandbuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &presentbarrier);
	vkEndCommandBuffer(commandbuffer);

	framepump->Present();
}
