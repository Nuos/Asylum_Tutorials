
#include <iostream>
#include <cassert>

#include "../common/vkx.h"

#define MAKE_COMMONVERTEX(_i, _x, _y, _z, _nx, _ny, _nz, _u, _v) \
	vdata[_i].x = _x; \
	vdata[_i].y = _y; \
	vdata[_i].z = _z; \
	vdata[_i].nx = _nx; \
	vdata[_i].ny = _ny; \
	vdata[_i].nz = _nz; \
	vdata[_i].u = _u; \
	vdata[_i].v = _v;
// END

extern long screenwidth;
extern long screenheight;

struct UniformData {
	float world[16];
	float viewproj[16];
	float lightpos[4];
	float eyepos[4];
	float color[4];
};

VkRenderPass			renderpass		= 0;
VkFramebuffer*			framebuffers	= 0;
VkCommandBuffer			commandbuffer	= 0;
VkSemaphore				framesema		= 0;
VkFence					drawfence		= 0;

VulkanGraphicsPipeline*	pipeline		= 0;
VulkanBuffer*			vertexbuffer	= 0;
VulkanBuffer*			indexbuffer		= 0;
VulkanBuffer*			uniformbuffer	= 0;
VulkanImage*			depthbuffer		= 0;
VulkanImage*			texture			= 0;

UniformData*			unidata			= 0;

bool InitScene()
{
	VkAttachmentDescription		rpattachments[2];
	VkImageView					fbattachments[2];
	VkSemaphoreCreateInfo		semainfo		= {};
	VkFenceCreateInfo			fenceinfo		= {};
	VkAttachmentReference		colorreference	= {};
	VkAttachmentReference		depthreference	= {};
	VkSubpassDescription		subpass			= {};
	VkRenderPassCreateInfo		renderpassinfo	= {};
	VkFramebufferCreateInfo		framebufferinfo = {};
	VkCommandBufferAllocateInfo	cmdbuffinfo		= {};
	VkResult					res;

	// create semaphore
	semainfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semainfo.pNext = NULL;
	semainfo.flags = 0;

	res = vkCreateSemaphore(driverinfo.device, &semainfo, NULL, &framesema);
	VK_ASSERT(res == VK_SUCCESS);

	// create render pass
	depthbuffer = VulkanImage::Create2D(VK_FORMAT_D24_UNORM_S8_UINT, screenwidth, screenheight, 1, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	rpattachments[0].format			= driverinfo.format;
	rpattachments[0].samples		= VK_SAMPLE_COUNT_1_BIT;
	rpattachments[0].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	rpattachments[0].storeOp		= VK_ATTACHMENT_STORE_OP_STORE;
	rpattachments[0].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	rpattachments[0].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[0].initialLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	rpattachments[0].finalLayout	= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	rpattachments[0].flags			= 0;

	rpattachments[1].format			= depthbuffer->GetFormat();
	rpattachments[1].samples		= VK_SAMPLE_COUNT_1_BIT;
	rpattachments[1].loadOp			= VK_ATTACHMENT_LOAD_OP_CLEAR;
	rpattachments[1].storeOp		= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[1].stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_CLEAR;
	rpattachments[1].stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	rpattachments[1].initialLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	rpattachments[1].finalLayout	= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	rpattachments[1].flags			= 0;

	colorreference.attachment		= 0;
	colorreference.layout			= VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	depthreference.attachment		= 1;
	depthreference.layout			= VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	subpass.pipelineBindPoint		= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.flags					= 0;
	subpass.inputAttachmentCount	= 0;
	subpass.pInputAttachments		= NULL;
	subpass.colorAttachmentCount	= 1;
	subpass.pColorAttachments		= &colorreference;
	subpass.pResolveAttachments		= NULL;
	subpass.pDepthStencilAttachment	= &depthreference;
	subpass.preserveAttachmentCount	= 0;
	subpass.pPreserveAttachments	= NULL;

	renderpassinfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpassinfo.pNext			= NULL;
	renderpassinfo.attachmentCount	= 2;
	renderpassinfo.pAttachments		= rpattachments;
	renderpassinfo.subpassCount		= 1;
	renderpassinfo.pSubpasses		= &subpass;
	renderpassinfo.dependencyCount	= 0;
	renderpassinfo.pDependencies	= NULL;

	res = vkCreateRenderPass(driverinfo.device, &renderpassinfo, NULL, &renderpass);
	VK_ASSERT(res == VK_SUCCESS);

	// create frame buffers
	framebuffers = new VkFramebuffer[driverinfo.swapchainimgcount];

	framebufferinfo.sType			= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferinfo.pNext			= NULL;
	framebufferinfo.renderPass		= renderpass;
	framebufferinfo.attachmentCount	= 2;
	framebufferinfo.pAttachments	= fbattachments;
	framebufferinfo.width			= screenwidth;
	framebufferinfo.height			= screenheight;
	framebufferinfo.layers			= 1;

	fbattachments[1] = depthbuffer->GetImageView();

	for( uint32_t i = 0; i < driverinfo.swapchainimgcount; ++i )
	{
		fbattachments[0] = driverinfo.swapchainimageviews[i];

		res = vkCreateFramebuffer(driverinfo.device, &framebufferinfo, NULL, &framebuffers[i]);
		VK_ASSERT(res == VK_SUCCESS);
	}

	// uniforms
	uniformbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(UniformData), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	unidata = (UniformData*)uniformbuffer->MapContents(0, 0);

	// create cube
	vertexbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, 24 * sizeof(VulkanCommonVertex), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
	indexbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, 16 * sizeof(uint16_t), VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

	VulkanCommonVertex* vdata = (VulkanCommonVertex*)vertexbuffer->MapContents(0, 0);
	float width = 1, height = 1, depth = 1;

	MAKE_COMMONVERTEX(0, width * -0.5f, height * -0.5f, depth * 0.5f, 0, -1, 0, 1, 0);
	MAKE_COMMONVERTEX(1, width * -0.5f, height * -0.5f, depth * -0.5f, 0, -1, 0, 1, 1);
	MAKE_COMMONVERTEX(2, width * 0.5f, height * -0.5f, depth * -0.5f, 0, -1, 0, 0, 1);
	MAKE_COMMONVERTEX(3, width * 0.5f, height * -0.5f, depth * 0.5f, 0, -1, 0, 0, 0);

	MAKE_COMMONVERTEX(4, width * -0.5f, height * 0.5f, depth * 0.5f, 0, 1, 0, 0, 0);
	MAKE_COMMONVERTEX(5, width * 0.5f, height * 0.5f, depth * 0.5f, 0, 1, 0, 1, 0);
	MAKE_COMMONVERTEX(6, width * 0.5f, height * 0.5f, depth * -0.5f, 0, 1, 0, 1, 1);
	MAKE_COMMONVERTEX(7, width * -0.5f, height * 0.5f, depth * -0.5f, 0, 1, 0, 0, 1);

	MAKE_COMMONVERTEX(8, width * -0.5f, height * -0.5f, depth * 0.5f, 0, 0, 1, 0, 0);
	MAKE_COMMONVERTEX(9, width * 0.5f, height * -0.5f, depth * 0.5f, 0, 0, 1, 1, 0);
	MAKE_COMMONVERTEX(10, width * 0.5f, height * 0.5f, depth * 0.5f, 0, 0, 1, 1, 1);
	MAKE_COMMONVERTEX(11, width * -0.5f, height * 0.5f, depth * 0.5f, 0, 0, 1, 0, 1);

	MAKE_COMMONVERTEX(12, width * 0.5f, height * -0.5f, depth * 0.5f, 1, 0, 0, 0, 0);
	MAKE_COMMONVERTEX(13, width * 0.5f, height * -0.5f, depth * -0.5f, 1, 0, 0, 1, 0);
	MAKE_COMMONVERTEX(14, width * 0.5f, height * 0.5f, depth * -0.5f, 1, 0, 0, 1, 1);
	MAKE_COMMONVERTEX(15, width * 0.5f, height * 0.5f, depth * 0.5f, 1, 0, 0, 0, 1);

	MAKE_COMMONVERTEX(16, width * 0.5f, height * -0.5f, depth * -0.5f, 0, 0, -1, 0, 0);
	MAKE_COMMONVERTEX(17, width * -0.5f, height * -0.5f, depth * -0.5f, 0, 0, -1, 1, 0);
	MAKE_COMMONVERTEX(18, width * -0.5f, height * 0.5f, depth * -0.5f, 0, 0, -1, 1, 1);
	MAKE_COMMONVERTEX(19, width * 0.5f, height * 0.5f, depth * -0.5f, 0, 0, -1, 0, 1);

	MAKE_COMMONVERTEX(20, width * -0.5f, height * -0.5f, depth * -0.5f, -1, 0, 0, 0, 0);
	MAKE_COMMONVERTEX(21, width * -0.5f, height * -0.5f, depth * 0.5f, -1, 0, 0, 1, 0);
	MAKE_COMMONVERTEX(22, width * -0.5f, height * 0.5f, depth * 0.5f, -1, 0, 0, 1, 1);
	MAKE_COMMONVERTEX(23, width * -0.5f, height * 0.5f, depth * -0.5f, -1, 0, 0, 0, 1);

	vertexbuffer->UnmapContents();

	static const uint16_t indices[36] = {
		0, 1, 2, 2, 3, 0, 
		4, 5, 6, 6, 7, 4,
		8, 9, 10, 10, 11, 8,
		12, 13, 14, 14, 15, 12,
		16, 17, 18, 18, 19, 16,
		20, 21, 22, 22, 23, 20
	};

	uint16_t* idata = (uint16_t*)indexbuffer->MapContents(0, 0);
	memcpy(idata, indices, 36 * sizeof(uint16_t));

	indexbuffer->UnmapContents();

	// create texture
	texture = VulkanImage::CreateFromFile("../media/textures/vk_logo.jpg", true);
	VK_ASSERT(texture);

	// synchronize memory to device
	VulkanPipelineBarrierBatch	barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	VkCommandBuffer				copycmd = VulkanCreateTempCommandBuffer();
	{
		texture->UploadToVRAM(copycmd);

		barrier.ImageLayoutTransfer(texture->GetImage(), 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		barrier.Enlist(copycmd);
	}
	VulkanSubmitTempCommandBuffer(copycmd);
	
	texture->DeleteStagingBuffer();
	texture->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// setup descriptor sets
	pipeline = new VulkanGraphicsPipeline();

	pipeline->SetDescriptorSetLayoutBufferBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	pipeline->SetDescriptorSetLayoutImageBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	pipeline->AllocateDescriptorSets(1);	// for group 0

	pipeline->SetDescriptorSetGroupBufferInfo(0, 0, uniformbuffer->GetBufferInfo());
	pipeline->SetDescriptorSetGroupImageInfo(0, 1, texture->GetImageInfo());

	pipeline->UpdateDescriptorSet(0, 0);

	// setup pipeline
	VK_ASSERT(pipeline->AddShader(VK_SHADER_STAGE_VERTEX_BIT, "../media/shadersVK/71_vulkan.vert"));
	VK_ASSERT(pipeline->AddShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../media/shadersVK/71_vulkan.frag"));
	//VK_ASSERT(pipeline->AddShader(VK_SHADER_STAGE_VERTEX_BIT, "../media/shadersVK/textured_vert.spirv"));
	//VK_ASSERT(pipeline->AddShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../media/shadersVK/textured_frag.spirv"));

	pipeline->SetInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);	// position
	pipeline->SetInputAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12);	// normal
	pipeline->SetInputAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, 24);		// texcoord

	pipeline->SetVertexInputBinding(0, VulkanMakeBindingDescription(0, VK_VERTEX_INPUT_RATE_VERTEX, sizeof(VulkanCommonVertex)));

	pipeline->SetDepth(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
	pipeline->SetViewport(0, 0, (float)screenwidth, (float)screenheight);
	pipeline->SetScissor(0, 0, screenwidth, screenheight);

	VK_ASSERT(pipeline->Assemble(renderpass));

	// create primary command buffer
	cmdbuffinfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdbuffinfo.pNext				= NULL;
	cmdbuffinfo.commandPool			= driverinfo.commandpool;
	cmdbuffinfo.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdbuffinfo.commandBufferCount	= 1;

	res = vkAllocateCommandBuffers(driverinfo.device, &cmdbuffinfo, &commandbuffer);
	VK_ASSERT(res == VK_SUCCESS);

	// create fence
	fenceinfo.sType					= VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceinfo.pNext					= NULL;
	fenceinfo.flags					= 0;

	vkCreateFence(driverinfo.device, &fenceinfo, NULL, &drawfence);

	return true;
}

void UninitScene()
{
	if( uniformbuffer )
		uniformbuffer->UnmapContents();

	VK_SAFE_RELEASE(texture);
	VK_SAFE_RELEASE(depthbuffer);

	delete pipeline;
	delete vertexbuffer;
	delete indexbuffer;
	delete uniformbuffer;

	if( commandbuffer )
		vkFreeCommandBuffers(driverinfo.device, driverinfo.commandpool, 1, &commandbuffer);

	for( uint32_t i = 0; i < driverinfo.swapchainimgcount; ++i ) {
		if( framebuffers[i] )
			vkDestroyFramebuffer(driverinfo.device, framebuffers[i], 0);
	}

	delete[] framebuffers;

	if( renderpass )
		vkDestroyRenderPass(driverinfo.device, renderpass, 0);

	if( drawfence )
		vkDestroyFence(driverinfo.device, drawfence, 0);

	if( framesema )
		vkDestroySemaphore(driverinfo.device, framesema, NULL);
}

void Event_KeyDown(unsigned char keycode)
{
	// do nothing
}

void Event_KeyUp(unsigned char keycode)
{
	// do nothing
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	// do nothing
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	// do nothing
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	// do nothing
}

void Update(float delta)
{
	// do nothing
}

void Render(float alpha, float elapsedtime)
{
	static float time = 0;

	float eye[3]	= { 0, 0, -2.5f };
	float look[3]	= { 0, 0, 0 };
	float up[3]		= { 0, 1, 0 };

	float view[16];
	float proj[16];
	float tmp1[16];
	float tmp2[16];

	// setup transforms
	VKMatrixLookAtLH(view, eye, look, up);
	VKMatrixPerspectiveFovLH(proj, (60.0f * 3.14159f) / 180.f,  (float)screenwidth / (float)screenheight, 0.1f, 100.0f);
	VKMatrixMultiply(unidata->viewproj, view, proj);

	VKMatrixRotationAxis(tmp1, VKDegreesToRadians(fmodf(time * 20.0f, 360.0f)), 1, 0, 0);
	VKMatrixRotationAxis(tmp2, VKDegreesToRadians(fmodf(time * 20.0f, 360.0f)), 0, 1, 0);
	VKMatrixMultiply(unidata->world, tmp1, tmp2);

	VKVec4Set(unidata->lightpos, 6, 3, -10, 1);
	VKVec4Set(unidata->eyepos, eye[0], eye[1], eye[2], 1);
	VKVec4Set(unidata->color, 1, 1, 1, 1);

	time += elapsedtime;

	// Vulkan declarations
	VkCommandBufferBeginInfo	begininfo			= {};
	VkRenderPassBeginInfo		passbegininfo		= {};
	VkImageMemoryBarrier		presentbarrier		= {};
	VkSubmitInfo				submitinfo			= {};
	VkPresentInfoKHR			presentinfo			= {};

	VkClearValue				clearcolors[2];
	VkResult					res;
	uint32_t					currentimage;

	vkResetCommandBuffer(commandbuffer, 0);

	// begin recording
	begininfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begininfo.pNext					= NULL;
	begininfo.flags					= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begininfo.pInheritanceInfo		= NULL;

	res = vkBeginCommandBuffer(commandbuffer, &begininfo);
	VK_ASSERT(res == VK_SUCCESS);

	// setup render pass
	res = vkAcquireNextImageKHR(driverinfo.device, driverinfo.swapchain, UINT64_MAX, framesema, NULL, &currentimage);
	VK_ASSERT(res == VK_SUCCESS);

	VulkanPipelineBarrierBatch barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	{
		barrier.ImageLayoutTransfer(driverinfo.swapchainimages[currentimage], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		barrier.ImageLayoutTransfer(depthbuffer->GetImage(), 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}
	barrier.Enlist(commandbuffer);

	clearcolors[0].color.float32[0] = 0.017f;
	clearcolors[0].color.float32[1] = 0;
	clearcolors[0].color.float32[2] = 0;
	clearcolors[0].color.float32[3] = 1;

	clearcolors[1].depthStencil.depth = 1.0f;
	clearcolors[1].depthStencil.stencil = 0;

	passbegininfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passbegininfo.pNext						= NULL;
	passbegininfo.renderPass				= renderpass;
	passbegininfo.framebuffer				= framebuffers[currentimage];
	passbegininfo.renderArea.offset.x		= 0;
	passbegininfo.renderArea.offset.y		= 0;
	passbegininfo.renderArea.extent.width	= screenwidth;
	passbegininfo.renderArea.extent.height	= screenheight;
	passbegininfo.clearValueCount			= 2;
	passbegininfo.pClearValues				= clearcolors;

	vkCmdBeginRenderPass(commandbuffer, &passbegininfo, VK_SUBPASS_CONTENTS_INLINE);
	{
		vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());
		vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, pipeline->GetDescriptorSets(0), 0, NULL);
		vkCmdSetViewport(commandbuffer, 0, 1, pipeline->GetViewport());
		vkCmdSetScissor(commandbuffer, 0, 1, pipeline->GetScissor());

		const VkDeviceSize offsets[1] = { 0 };
		VkBuffer vertbuffer = vertexbuffer->GetBuffer();

		vkCmdBindVertexBuffers(commandbuffer, 0, 1, &vertbuffer, offsets);
		vkCmdBindIndexBuffer(commandbuffer, indexbuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

		vkCmdDrawIndexed(commandbuffer, 36, 1, 0, 0, 0);
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

	// submit command buffer
	VkPipelineStageFlags pipestageflags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	submitinfo.pNext				= NULL;
	submitinfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.waitSemaphoreCount	= 1;
	submitinfo.pWaitSemaphores		= &framesema;
	submitinfo.pWaitDstStageMask	= &pipestageflags;
	submitinfo.commandBufferCount	= 1;
	submitinfo.pCommandBuffers		= &commandbuffer;
	submitinfo.signalSemaphoreCount	= 0;
	submitinfo.pSignalSemaphores	= NULL;

	vkResetFences(driverinfo.device, 1, &drawfence);

	res = vkQueueSubmit(driverinfo.graphicsqueue, 1, &submitinfo, drawfence);
	VK_ASSERT(res == VK_SUCCESS);

	// present to window
	do {
		res = vkWaitForFences(driverinfo.device, 1, &drawfence, VK_TRUE, 100000000);
	} while( res == VK_TIMEOUT );

	VK_ASSERT(res == VK_SUCCESS);

	presentinfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.pNext				= NULL;
	presentinfo.swapchainCount		= 1;
	presentinfo.pSwapchains			= &driverinfo.swapchain;
	presentinfo.pImageIndices		= &currentimage;
	presentinfo.pWaitSemaphores		= NULL;
	presentinfo.waitSemaphoreCount	= 0;
	presentinfo.pResults			= NULL;

	res = vkQueuePresentKHR(driverinfo.graphicsqueue, &presentinfo);
	VK_ASSERT(res == VK_SUCCESS);
}
