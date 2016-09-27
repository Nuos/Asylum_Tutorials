
#include <iostream>
#include <cassert>

#include "../common/vkx.h"
#include "../common/basiccamera.h"
#include "../common/perfmeasure.h"

// vkCreateQueryPool!!!
// az nvidia meg csal mert primaryt hiv tovabb
// filterek!!!

// - 50k keves haromszogu objektumot rajzolj
// - bufferImageGranularity!!!!!
// - If vkCmdPipelineBarrier is called within a render pass instance, the render pass must declare at least one self-dependency from the current subpass to itself
// - barrier flusholja a GPU (cache)-t, erdemes batchelni
// - fence minden cache-t flushol
// - device lost lehetseges...mi tortenik majd fullscreenben?

#define TITLE				"Shader sample 71: Vulkan drawcall batching"
#define OBJECT_GRID_SIZE	64		// nxn objects
#define TILE_GRID_SIZE		32		// kxk tiles
#define SPACING				0.4f
#define CAMERA_SPEED		0.05f

//#define DEBUG_ENCODING
#define MEASURE_PERF

#ifdef MEASURE_PERF
#	define MEASURE_POINT(x)		perfmeasure.Measure(x)
#else
#	define MEASURE_POINT(x)
#endif

extern HWND		hwnd;
extern long		screenwidth;
extern long		screenheight;

struct GlobalUniformData {
	float viewproj[16];
	float lightpos[4];
	float eyepos[4];
};

struct MaterialData {
	float color[4];
	float padding[60];	// offset alignment must be at least 256 B
};

struct DebugUniformData {
	float viewproj[16];
	float color[4];
};

class SceneObjectPrototype
{
private:
	VulkanBasicMesh* mesh;

public:
	SceneObjectPrototype(const char* filename, VulkanBuffer* storage = 0, VkDeviceSize storageoffset = 0);
	~SceneObjectPrototype();

	void Draw(VkCommandBuffer commandbuffer);

	inline VulkanBasicMesh* GetMesh()				{ return mesh; }
	inline const VulkanBasicMesh* GetMesh() const	{ return mesh; }
};

class SceneObject
{
private:
	float		world[16];
	uint32_t	materialoffset;
	bool		encoded;

	SceneObjectPrototype* proto;

public:
	SceneObject(SceneObjectPrototype* prototype);

	void Draw(VkCommandBuffer commandbuffer);
	void GetBoundingBox(VulkanAABox& outbox) const;

	inline bool IsEncoded() const						{ return encoded; }
	inline float* GetTransform()						{ return world; }
	inline const SceneObjectPrototype* GetProto() const	{ return proto; }

	inline void SetMaterialOffset(uint32_t offset)		{ materialoffset = offset; }
	inline void SetEncoded(bool value)					{ encoded = value; }
};

class DrawBatch
{
public:
	struct TileObject {
		SceneObject*	object;
		bool			encoded[2];
	};

	typedef std::vector<TileObject> TileObjectArray;

private:
	TileObjectArray	tileobjects;
	VulkanAABox		boundingbox;
	VkCommandBuffer	commandbuffers[2];
	bool			markedfordiscard[2];
	bool			discarded[2];
	uint32_t		lastenqueuedframe;

public:
	DrawBatch();
	~DrawBatch();

	void AddObject(SceneObject* obj);
	void DebugDraw(VkCommandBuffer commandbuffer);
	void Discard(uint32_t finishedframe);
	void Regenerate(uint32_t currentimage);

	VkCommandBuffer SetForRender(uint32_t frameid, uint32_t currentimage);

	inline void MarkForDiscard(uint32_t currentimage)		{ markedfordiscard[currentimage] = true; }

	inline const VulkanAABox& GetBoundingBox() const		{ return boundingbox; }
	inline const TileObjectArray& GetObjects() const		{ return tileobjects; }
	inline bool IsDiscarded(uint32_t currentimage) const	{ return discarded[currentimage]; }
	inline bool IsMarkedForDiscard() const					{ return (markedfordiscard[0] || markedfordiscard[1]); }
};

typedef std::vector<SceneObject*> ObjectArray;
typedef std::vector<DrawBatch*> BatchArray;

VkRenderPass			renderpass			= 0;
VkFramebuffer*			framebuffers		= 0;

VulkanImage*			depthbuffer			= 0;
VulkanBuffer*			storage				= 0;
VulkanBuffer*			uniforms			= 0;
VulkanBuffer*			materials			= 0;
VulkanBasicMesh*		debugmesh			= 0;
VulkanGraphicsPipeline*	pipeline			= 0;
VulkanGraphicsPipeline*	debugpipeline		= 0;
VulkanFramePump*		framepump			= 0;

SceneObjectPrototype*	prototypes[3]		= { 0, 0, 0 };
ObjectArray				sceneobjects;
BatchArray				tiles;
BatchArray				visibletiles;
BasicCamera				debugcamera;
float					debugworld[16];
float					debugcolor[4];

#ifdef MEASURE_PERF
PerfMeasure				perfmeasure;
#endif

uint32_t				totalpolys			= 0;
float					totalwidth;
float					totaldepth;
float					totalheight;

short					mousedx;
short					mousedy;
short					mousedown;

float					time				= 0;
int						framerate			= 0;
bool					animating			= true;
bool					debugmode			= false;

void FrameFinished(uint32_t frameid);
void UpdateTiles(float* viewproj, uint32_t currentimage);

//*************************************************************************************************************
//
// SceneObjectPrototype impl
//
//*************************************************************************************************************

SceneObjectPrototype::SceneObjectPrototype(const char* filename, VulkanBuffer* storage, VkDeviceSize storageoffset)
{
	mesh = VulkanBasicMesh::LoadFromQM(filename, storage, storageoffset);
	
	VK_ASSERT(mesh);
	VK_ASSERT(mesh->GetVertexStride() == 32);
}

SceneObjectPrototype::~SceneObjectPrototype()
{
	delete mesh;
}

void SceneObjectPrototype::Draw(VkCommandBuffer commandbuffer)
{
	mesh->Draw(commandbuffer, 0);
}

//*************************************************************************************************************
//
// SceneObject impl
//
//*************************************************************************************************************

SceneObject::SceneObject(SceneObjectPrototype* prototype)
{
	VK_ASSERT(prototype);

	proto = prototype;
	materialoffset = 0;
	encoded = false;

	VKMatrixIdentity(world);
}

void SceneObject::Draw(VkCommandBuffer commandbuffer)
{
	if( !encoded )
	{
		vkCmdPushConstants(commandbuffer, pipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 64, world);
		vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, pipeline->GetDescriptorSets(0), 1, &materialoffset);

		proto->Draw(commandbuffer);
		encoded = true;
	}
}

void SceneObject::GetBoundingBox(VulkanAABox& outbox) const
{
	outbox = proto->GetMesh()->GetBoundingBox();
	outbox.TransformAxisAligned(world);
}

//*************************************************************************************************************
//
// DrawBatch impl
//
//*************************************************************************************************************

DrawBatch::DrawBatch()
{
	VkCommandBufferAllocateInfo cmdbuffinfo = {};
	VkResult res;

	cmdbuffinfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdbuffinfo.pNext				= NULL;
	cmdbuffinfo.commandPool			= driverinfo.commandpool;
	cmdbuffinfo.level				= VK_COMMAND_BUFFER_LEVEL_SECONDARY;
	cmdbuffinfo.commandBufferCount	= 2;

	res = vkAllocateCommandBuffers(driverinfo.device, &cmdbuffinfo, commandbuffers);
	VK_ASSERT(res == VK_SUCCESS);

	markedfordiscard[0] = false;
	markedfordiscard[1] = false;

	discarded[0] = true;
	discarded[1] = true;
}

DrawBatch::~DrawBatch()
{
	vkFreeCommandBuffers(driverinfo.device, driverinfo.commandpool, 2, commandbuffers);
	tileobjects.clear();
}

void DrawBatch::AddObject(SceneObject* obj)
{
	VulkanAABox	objbox;
	TileObject	tileobj;

	if( tileobjects.size() >= tileobjects.capacity() )
		tileobjects.reserve(tileobjects.capacity() + 16);

	tileobj.object = obj;
	tileobj.encoded[0] = false;
	tileobj.encoded[1] = false;

	tileobjects.push_back(tileobj);
	obj->GetBoundingBox(objbox);

	boundingbox.Add(objbox.Min);
	boundingbox.Add(objbox.Max);
}

void DrawBatch::DebugDraw(VkCommandBuffer commandbuffer)
{
	for( size_t j = 0; j < tileobjects.size(); ++j )
		tileobjects[j].object->Draw(commandbuffer);
}

void DrawBatch::Discard(uint32_t finishedframe)
{
	if( finishedframe == lastenqueuedframe ) {
		for( int j = 0; j < 2; ++j ) {
			if( markedfordiscard[j] && !discarded[j] ) {
				for( size_t i = 0; i < tileobjects.size(); ++i )
					tileobjects[i].encoded[j] = false;

				vkResetCommandBuffer(commandbuffers[j], VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
				
				discarded[j] = true;
				markedfordiscard[j] = false;
			}
		}
	}
}

void DrawBatch::Regenerate(uint32_t currentimage)
{
	if( !discarded[currentimage] ) {
		// not deleted yet, mark them encoded
		for( size_t i = 0; i < tileobjects.size(); ++i ) {
			if( tileobjects[i].encoded[currentimage] )
				tileobjects[i].object->SetEncoded(true);
		}

		markedfordiscard[currentimage] = false;
		return;
	}

	VkCommandBufferInheritanceInfo	inheritanceinfo	= {};
	VkCommandBufferBeginInfo		begininfo		= {};
	VkResult res;

	inheritanceinfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	inheritanceinfo.pNext					= NULL;
	inheritanceinfo.renderPass				= renderpass;
	inheritanceinfo.subpass					= 0;
	inheritanceinfo.occlusionQueryEnable	= VK_FALSE;
	inheritanceinfo.queryFlags				= 0;
	inheritanceinfo.pipelineStatistics		= 0;
	inheritanceinfo.framebuffer				= framebuffers[currentimage];

	begininfo.sType							= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begininfo.pNext							= NULL;
	begininfo.flags							= VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT|VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	begininfo.pInheritanceInfo				= &inheritanceinfo;

	res = vkBeginCommandBuffer(commandbuffers[currentimage], &begininfo);
	VK_ASSERT(res == VK_SUCCESS);
	{
		vkCmdBindPipeline(commandbuffers[currentimage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipeline());
		vkCmdSetViewport(commandbuffers[currentimage], 0, 1, pipeline->GetViewport());
		vkCmdSetScissor(commandbuffers[currentimage], 0, 1, pipeline->GetScissor());

		for( size_t j = 0; j < tileobjects.size(); ++j ) {
			bool alreadyencoded = tileobjects[j].object->IsEncoded();
			tileobjects[j].encoded[currentimage] = !alreadyencoded;

			if( !alreadyencoded ) {
				// don't encode more than once
				tileobjects[j].object->Draw(commandbuffers[currentimage]);
			}
		}
	}
	vkEndCommandBuffer(commandbuffers[currentimage]);

	discarded[currentimage] = false;
}

VkCommandBuffer DrawBatch::SetForRender(uint32_t frameid, uint32_t currentimage)
{
	lastenqueuedframe = frameid;
	return commandbuffers[currentimage];
}

//*************************************************************************************************************
//
// Sample impl
//
//*************************************************************************************************************

bool InitScene()
{
	SetWindowText(hwnd, TITLE);
	VK_ASSERT(driverinfo.swapchainimgcount == 2);

	VkAttachmentDescription		rpattachments[2];
	VkImageView					fbattachments[2];
	VkAttachmentReference		colorreference	= {};
	VkAttachmentReference		depthreference	= {};
	VkSubpassDescription		subpasses[2]	= {};
	VkSubpassDependency			dependency		= {};
	VkRenderPassCreateInfo		renderpassinfo	= {};
	VkFramebufferCreateInfo		framebufferinfo = {};
	VkResult					res;

	framepump = new VulkanFramePump();
	framepump->FrameFinished = FrameFinished;

	// create render pass
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

	subpasses[0].pipelineBindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].flags						= 0;
	subpasses[0].inputAttachmentCount		= 0;
	subpasses[0].pInputAttachments			= NULL;
	subpasses[0].colorAttachmentCount		= 1;
	subpasses[0].pColorAttachments			= &colorreference;
	subpasses[0].pResolveAttachments		= NULL;
	subpasses[0].pDepthStencilAttachment	= &depthreference;
	subpasses[0].preserveAttachmentCount	= 0;
	subpasses[0].pPreserveAttachments		= NULL;

	subpasses[1] = subpasses[0];

	dependency.dependencyFlags	= 0;
	dependency.srcSubpass		= 0;
	dependency.dstSubpass		= 1;
	dependency.srcStageMask		= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
	dependency.dstStageMask		= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;

	dependency.srcAccessMask	=
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	dependency.dstAccessMask	=
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

	renderpassinfo.sType			= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpassinfo.pNext			= NULL;
	renderpassinfo.attachmentCount	= 2;
	renderpassinfo.pAttachments		= rpattachments;
	renderpassinfo.subpassCount		= 2;
	renderpassinfo.pSubpasses		= subpasses;
	renderpassinfo.dependencyCount	= 1;
	renderpassinfo.pDependencies	= &dependency;

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

	// create materials
	materials = VulkanBuffer::Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 4 * sizeof(MaterialData), VK_MEMORY_PROPERTY_SHARED_BIT);

	MaterialData* matdata = (MaterialData*)materials->MapContents(0, 0);

	VKVec4Set(matdata[0].color, 1, 0, 0, 1);
	VKVec4Set(matdata[1].color, 0, 1, 0, 1);
	VKVec4Set(matdata[2].color, 0, 0, 1, 1);
	VKVec4Set(matdata[3].color, 1, 0.5f, 0, 1);

	materials->UnmapContents();

	// create objects
	float			objscales[3][16];
	float			objtranslate[16];
	float			objrotate[16];
	VulkanAABox		objboxes[3];
	VkDeviceSize	offset = 0;

	VKMatrixScaling(objscales[0], 1, 1, 1);
	//VKMatrixScaling(objscales[1], 0.01f, 0.01f, 0.01f);
	VKMatrixScaling(objscales[1], 0.5f, 0.5f, 0.5f);
	//VKMatrixScaling(objscales[2], 16.0f, 16.0f, 16.0f);
	//VKMatrixScaling(objscales[2], 4.0f, 4.0f, 4.0f);
	VKMatrixScaling(objscales[2], 0.55f, 0.55f, 0.55f);

	prototypes[0] = new SceneObjectPrototype("../media/meshes/teapot.qm", storage, offset);				// 375552	[3.216800, 1.575000, 2.000000]
	offset += prototypes[0]->GetMesh()->GetTotalSize();

	//prototypes[1] = new SceneObjectPrototype("../media/meshes/angel.qm", storage, offset);			// 1221632	[207.585205, 305.376404, 152.160202]
	prototypes[1] = new SceneObjectPrototype("../media/meshes/reventon/reventon.qm", storage, offset);	// 1292288	[4.727800, 1.143600, 2.175000]
	offset += prototypes[1]->GetMesh()->GetTotalSize();

	//prototypes[2] = new SceneObjectPrototype("../media/meshes/happy1.qm", storage, offset);			// 6883840	[0.081322, 0.198010, 0.081418]
	//prototypes[2] = new SceneObjectPrototype("../media/meshes/knot.qm", storage, offset);				// 1136640	[0.791200, 0.800800, 0.384800]
	prototypes[2] = new SceneObjectPrototype("../media/meshes/zonda.qm", storage, offset);				// 752128	[2.055100, 1.061800, 4.229200]
	offset += prototypes[2]->GetMesh()->GetTotalSize();

	objboxes[0] = prototypes[0]->GetMesh()->GetBoundingBox();
	objboxes[1] = prototypes[1]->GetMesh()->GetBoundingBox();
	objboxes[2] = prototypes[2]->GetMesh()->GetBoundingBox();

	objboxes[0].TransformAxisAligned(objscales[0]);
	objboxes[1].TransformAxisAligned(objscales[1]);
	objboxes[2].TransformAxisAligned(objscales[2]);

	srand(0);

	totalwidth = OBJECT_GRID_SIZE * 3.2168f + (OBJECT_GRID_SIZE - 1) * SPACING;
	totaldepth = OBJECT_GRID_SIZE * 2.0f + (OBJECT_GRID_SIZE - 1) * SPACING;
	totalheight = 5;

	for( size_t i = 0; i < OBJECT_GRID_SIZE; ++i )
	{
		for( size_t j = 0; j < OBJECT_GRID_SIZE; ++j )
		{
			int index = rand() % 3;
			float angle = VKDegreesToRadians((float)(rand() % 360));

			SceneObjectPrototype* proto = prototypes[index];

			sceneobjects.push_back(new SceneObject(proto));
			totalpolys += proto->GetMesh()->GetNumPolygons();

			VKMatrixRotationAxis(objrotate, angle, 0, 1, 0);
			VKMatrixTranslation(objtranslate, i * (3.2168f + SPACING) - totalwidth * 0.5f, 0, j * (2.0f + SPACING) - totaldepth * 0.5f);

			objtranslate[13] = -objboxes[index].Min[1];	// so they start at 0

			VKMatrixMultiply(sceneobjects.back()->GetTransform(), objscales[index], objrotate);
			VKMatrixMultiply(sceneobjects.back()->GetTransform(), sceneobjects.back()->GetTransform(), objtranslate);

			sceneobjects.back()->SetMaterialOffset((rand() % 4) * sizeof(MaterialData));
		}
	}

	printf("Total number of polygons: %u\n", totalpolys);

	uniforms = VulkanBuffer::Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT, sizeof(GlobalUniformData), VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// create pipeline
	pipeline = new VulkanGraphicsPipeline();

	VK_ASSERT(pipeline->AddShader(VK_SHADER_STAGE_VERTEX_BIT, "../media/shadersVK/71_drawbatching.vert"));
	VK_ASSERT(pipeline->AddShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../media/shadersVK/71_drawbatching.frag"));

	pipeline->SetInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);	// position
	pipeline->SetInputAttribute(1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12);	// normal
	pipeline->SetInputAttribute(2, 0, VK_FORMAT_R32G32_SFLOAT, 24);		// texcoord

	pipeline->SetVertexInputBinding(0, VulkanMakeBindingDescription(0, VK_VERTEX_INPUT_RATE_VERTEX, 32));

	VkDescriptorBufferInfo unibuffinfo;
	VkDescriptorBufferInfo matbuffinfo;

	unibuffinfo.buffer = uniforms->GetBuffer();
	unibuffinfo.offset = 0;
	unibuffinfo.range = sizeof(GlobalUniformData);

	matbuffinfo.buffer = materials->GetBuffer();
	matbuffinfo.offset = 0;
	matbuffinfo.range = sizeof(MaterialData);

	// setup descriptor sets
	pipeline->SetDescriptorSetLayoutBufferBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	pipeline->SetDescriptorSetLayoutBufferBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT);
	
	pipeline->AllocateDescriptorSets(1);	// for group 0

	pipeline->SetDescriptorSetGroupBufferInfo(0, 0, &unibuffinfo);
	pipeline->SetDescriptorSetGroupBufferInfo(0, 1, &matbuffinfo);

	pipeline->UpdateDescriptorSet(0, 0);

	// setup pipeline states
	pipeline->SetDepth(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
	pipeline->SetViewport(0, 0, (float)screenwidth, (float)screenheight);
	pipeline->SetScissor(0, 0, screenwidth, screenheight);

	pipeline->AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64);	// for world matrix
	VK_ASSERT(pipeline->Assemble(renderpass));

	// create tiles
	VulkanAABox	tilebox;
	VulkanAABox	objbox;
	float		tilewidth = totalwidth / TILE_GRID_SIZE;
	float		tiledepth = totaldepth / TILE_GRID_SIZE;

	tiles.resize(TILE_GRID_SIZE * TILE_GRID_SIZE, 0);

	for( size_t i = 0; i < TILE_GRID_SIZE; ++i )
	{
		for( size_t j = 0; j < TILE_GRID_SIZE; ++j )
		{
			VKVec3Set(tilebox.Min, totalwidth * -0.5f + j * tilewidth, 0, totaldepth * -0.5f + i * tiledepth);
			VKVec3Set(tilebox.Max, totalwidth * -0.5f + (j + 1) * tilewidth, totalheight, totaldepth * -0.5f + (i + 1) * tiledepth);

			DrawBatch* batch = (tiles[i * TILE_GRID_SIZE + j] = new DrawBatch());

			// now this is slow...
			for( size_t k = 0; k < sceneobjects.size(); ++k )
			{
				sceneobjects[k]->GetBoundingBox(objbox);

				if( tilebox.Intersects(objbox) )
					batch->AddObject(sceneobjects[k]);
			}
		}
	}

	visibletiles.reserve(tiles.size());

	// setup debug camera
	VulkanAABox worldbox;
	float center[3];

	VKVec3Set(worldbox.Min, totalwidth * -0.5f, totalheight * -0.5f, totaldepth * -0.5f);
	VKVec3Set(worldbox.Max, totalwidth * 0.5f, totalheight * 0.5f, totaldepth * 0.5f);

	worldbox.GetCenter(center);

	debugcamera.SetAspect((float)screenwidth / screenheight);
	debugcamera.SetFov(VK_PI / 3);
	debugcamera.SetPosition(center[0], center[1], center[2]);
	debugcamera.SetDistance(VKVec3Distance(worldbox.Min, worldbox.Max) * 0.65f);
	debugcamera.SetClipPlanes(0.1f, VKVec3Distance(worldbox.Min, worldbox.Max) * 2);
	debugcamera.OrbitRight(0);
	debugcamera.OrbitUp(VKDegreesToRadians(30));

	// create debug object
	debugmesh = new VulkanBasicMesh(8, 24, 12);

	float (*vdata)[3] = (float (*)[3])debugmesh->GetVertexBufferPointer();

	VKVec3Set(vdata[0], -1, -1, -1);
	VKVec3Set(vdata[1], -1, -1, 1);
	VKVec3Set(vdata[2], -1, 1, -1);
	VKVec3Set(vdata[3], -1, 1, 1);

	VKVec3Set(vdata[4], 1, -1, -1);
	VKVec3Set(vdata[5], 1, -1, 1);
	VKVec3Set(vdata[6], 1, 1, -1);
	VKVec3Set(vdata[7], 1, 1, 1);

	uint16_t* idata = (uint16_t*)debugmesh->GetIndexBufferPointer();

	uint16_t wireindices[] =
	{
		0, 1, 1, 3, 3, 2,
		2, 0, 0, 4, 4, 6,
		6, 2, 4, 5, 5, 7,
		7, 6, 7, 3, 1, 5
	};

	memcpy(idata, wireindices, 24 * sizeof(uint16_t));

	// create debug pipeline
	debugpipeline = new VulkanGraphicsPipeline();

	VK_ASSERT(debugpipeline->AddShader(VK_SHADER_STAGE_VERTEX_BIT, "../media/shadersVK/basiccolor.vert"));
	VK_ASSERT(debugpipeline->AddShader(VK_SHADER_STAGE_FRAGMENT_BIT, "../media/shadersVK/basiccolor.frag"));

	debugpipeline->SetInputAssembler(VK_PRIMITIVE_TOPOLOGY_LINE_LIST, VK_FALSE);
	debugpipeline->SetInputAttribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);	// position

	debugpipeline->SetVertexInputBinding(0, VulkanMakeBindingDescription(0, VK_VERTEX_INPUT_RATE_VERTEX, debugmesh->GetVertexStride()));

	debugpipeline->SetDescriptorSetLayoutBufferBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	debugpipeline->AllocateDescriptorSets(1);

	debugpipeline->SetDescriptorSetGroupBufferInfo(0, 0, debugmesh->GetUniformBufferInfo());
	debugpipeline->UpdateDescriptorSet(0, 0);

	debugpipeline->AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, 64);		// world matrix
	debugpipeline->AddPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 64, 16);	// extra

	debugpipeline->SetDepth(VK_TRUE, VK_TRUE, VK_COMPARE_OP_LESS);
	debugpipeline->SetViewport(0, 0, (float)screenwidth, (float)screenheight);
	debugpipeline->SetScissor(0, 0, screenwidth, screenheight);

	VK_ASSERT(debugpipeline->Assemble(renderpass));

	// synchronize memory to device
	VkCommandBuffer copycmd = VulkanCreateTempCommandBuffer();
	{
		materials->UploadToVRAM(copycmd);
		debugmesh->UploadToVRAM(copycmd);

		prototypes[0]->GetMesh()->UploadToVRAM(copycmd);
		prototypes[1]->GetMesh()->UploadToVRAM(copycmd);
		prototypes[2]->GetMesh()->UploadToVRAM(copycmd);
	}
	VulkanSubmitTempCommandBuffer(copycmd);
	
	materials->DeleteStagingBuffer();
	debugmesh->DeleteStagingBuffers();

	prototypes[0]->GetMesh()->DeleteStagingBuffers();
	prototypes[1]->GetMesh()->DeleteStagingBuffers();
	prototypes[2]->GetMesh()->DeleteStagingBuffers();

	return true;
}

void UpdateTiles(float* viewproj, uint32_t currentimage)
{
	// TODO: make it multithreaded
	float planes[6][4];

	VKFrustumPlanes(planes, viewproj);
	visibletiles.clear();

	// mark all objects non-encoded
	for( size_t i = 0; i < sceneobjects.size(); ++i )
		sceneobjects[i]->SetEncoded(false);

	for( size_t i = 0; i < tiles.size(); ++i ) {
		const VulkanAABox& tilebox = tiles[i]->GetBoundingBox();

		if( VKFrustumIntersect(planes, tilebox) > 0 ) {
			visibletiles.push_back(tiles[i]);

			// mark tile-encoded objects encoded (to avoid double encoding)
			if( !tiles[i]->IsDiscarded(currentimage) )
				tiles[i]->Regenerate(currentimage);
		} else {
			tiles[i]->MarkForDiscard(currentimage);
		}
	}

	// regenerate new tiles
	for( size_t i = 0; i < visibletiles.size(); ++i ){ 
		if( visibletiles[i]->IsDiscarded(currentimage) )
			visibletiles[i]->Regenerate(currentimage);
	}

#ifdef DEBUG_ENCODING
	VulkanAABox objbox;
	bool reallyencoded;
	bool encodedtwice;

	for( size_t i = 0; i < visibletiles.size(); ++i )
	{
		const DrawBatch::TileObjectArray& tileobjects = visibletiles[i]->GetObjects();

		for( size_t j = 0; j < tileobjects.size(); ++j )
		{
			tileobjects[j].object->GetBoundingBox(objbox);

			if( VKFrustumIntersect(planes, objbox) > 0 )
			{
				reallyencoded = false;
				encodedtwice = false;

				// it must be encoded somewhere
				for( size_t k = 0; k < visibletiles.size(); ++k )
				{
					const DrawBatch::TileObjectArray& othertileobjects = visibletiles[k]->GetObjects();

					for( size_t l = 0; l < othertileobjects.size(); ++l )
					{
						if( othertileobjects[l].object == tileobjects[j].object )
						{
							if( reallyencoded && othertileobjects[l].encoded[currentimage] )
							{
								encodedtwice = true;
								break;
							}

							reallyencoded = (reallyencoded || othertileobjects[l].encoded[currentimage]);
						}
					}
				}

				VK_ASSERT(!encodedtwice);
				VK_ASSERT(reallyencoded);
			}
		}
	}
#endif
}

void UninitScene()
{
#ifdef MEASURE_PERF
	perfmeasure.Dump();
#endif

	vkDeviceWaitIdle(driverinfo.device);

	for( size_t i = 0; i < tiles.size(); ++i )
		delete tiles[i];

	tiles.clear();
	tiles.swap(BatchArray());

	visibletiles.clear();
	visibletiles.swap(BatchArray());

	for( size_t i = 0; i < sceneobjects.size(); ++i )
		delete sceneobjects[i];

	sceneobjects.clear();
	sceneobjects.swap(ObjectArray());

	for( size_t i = 0; i < VK_ARRAY_SIZE(prototypes); ++i )
		delete prototypes[i];

	delete pipeline;
	delete debugpipeline;
	delete debugmesh;
	delete uniforms;
	delete materials;
	delete storage;

	VK_SAFE_RELEASE(depthbuffer);

	for( uint32_t i = 0; i < driverinfo.swapchainimgcount; ++i ) {
		vkDestroyFramebuffer(driverinfo.device, framebuffers[i], 0);
	}

	delete[] framebuffers;

	vkDestroyRenderPass(driverinfo.device, renderpass, NULL);
	
	VK_SAFE_DELETE(framepump);
}

void Event_KeyDown(unsigned char keycode)
{
	// do nothing
}

void Event_KeyUp(unsigned char keycode)
{
	if( keycode == 0x44 )
		debugmode = !debugmode;
	else if( keycode == 0x41 )
	{
		animating = !animating;

		if( !animating )
		{
			uint32_t visiblepolys = 0;

			for( size_t i = 0; i < visibletiles.size(); ++i )
			{
				const DrawBatch::TileObjectArray& tileobjects = visibletiles[i]->GetObjects();

				for( size_t j = 0; j < tileobjects.size(); ++j )
				{
					// doesn't really matter
					if( tileobjects[j].encoded[0] )
						visiblepolys += tileobjects[j].object->GetProto()->GetMesh()->GetNumPolygons();
				}
			}

			printf("Visible tiles: %llu\n", visibletiles.size());
			printf("Visible polygons: %u\n", visiblepolys);
			printf("Last FPS: %d\n", framerate);
		}
	}
}

void Event_MouseMove(int x, int y, short dx, short dy)
{
	if( mousedown == 1 && debugmode )
	{
		debugcamera.OrbitRight(VKDegreesToRadians(dx) * -0.5f);
		debugcamera.OrbitUp(VKDegreesToRadians(dy) * 0.5f);
	}
}

void Event_MouseDown(int x, int y, unsigned char button)
{
	mousedown = 1;
}

void Event_MouseUp(int x, int y, unsigned char button)
{
	mousedown = 0;
}

void Update(float delta)
{
	// do nothing
}

void Render(float alpha, float elapsedtime)
{
	GlobalUniformData	unidata;
	float				eye[3];
	float				look[3];
	float				tang[3];
	float				up[3]	= { 0, 1, 0 };

	float				view[16];
	float				proj[16];

	framerate = (int)(1.0f / elapsedtime);

#ifdef MEASURE_PERF
	perfmeasure.Start();
#endif

	// setup transforms
	float				halfw	= totalwidth * 0.45f;
	float				halfd	= totaldepth * 0.45f;
	float				t		= time * (CAMERA_SPEED * 32.0f) / OBJECT_GRID_SIZE;

	eye[0] = halfw * sinf(t * 2);
	eye[1] = totalheight + 8.0f;
	eye[2] = halfd * cosf(t * 3);

	tang[0] = halfw * cosf(t * 2) * 2;
	tang[2] = -halfd * sinf(t * 3) * 3;
	tang[1] = sqrtf(tang[0] * tang[0] + tang[2] * tang[2]) * -tanf(VKDegreesToRadians(60));

	VKVec3Normalize(tang, tang);
	VKVec3Add(look, eye, tang);

	VKMatrixLookAtLH(view, eye, look, up);
	VKMatrixPerspectiveFovLH(proj, (60.0f * 3.14159f) / 180.f,  (float)screenwidth / (float)screenheight, 1.5f, 50.0f);
	VKMatrixMultiply(unidata.viewproj, view, proj);

	// setup uniforms
	float lightpos[4] = { 1, 0.4f, -0.2f, 1 };
	float radius = sqrtf(totalwidth * totalwidth * 0.25f + totalheight * totalheight * 0.25f + totaldepth * totaldepth * 0.25f);

	VKVec3Normalize(lightpos, lightpos);
	VKVec3Scale(lightpos, lightpos, radius * 1.5f);

	VKVec4Assign(unidata.lightpos, lightpos);
	VKVec4Set(unidata.eyepos, eye[0], eye[1], eye[2], 1);

	// Vulkan declarations
	VkCommandBufferBeginInfo	begininfo			= {};
	VkRenderPassBeginInfo		passbegininfo		= {};
	VkImageMemoryBarrier		presentbarrier		= {};
	VkSubmitInfo				submitinfo			= {};
	VkPresentInfoKHR			presentinfo			= {};

	VkCommandBuffer				commandbuffer;
	VkClearValue				clearcolors[2];
	VkResult					res;
	uint32_t					currentimage;

	commandbuffer = framepump->GetNextCommandBuffer();

	// begin recording
	begininfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begininfo.pNext				= NULL;
	begininfo.flags				= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	begininfo.pInheritanceInfo	= NULL;

	res = vkBeginCommandBuffer(commandbuffer, &begininfo);
	VK_ASSERT(res == VK_SUCCESS);

	// setup render pass
	currentimage = framepump->GetNextDrawable();

	VulkanPipelineBarrierBatch barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	{
		barrier.ImageLayoutTransfer(driverinfo.swapchainimages[currentimage], 0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		barrier.ImageLayoutTransfer(depthbuffer->GetImage(), 0, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}
	barrier.Enlist(commandbuffer);

	// update visible tiles
	MEASURE_POINT("");

	UpdateTiles(unidata.viewproj, currentimage);

	if( animating )
		time += elapsedtime;

	MEASURE_POINT("Update tiles");

	if( debugmode )
	{
		DebugUniformData* debugunis = (DebugUniformData*)debugmesh->GetUniformBufferPointer();

		debugcamera.GetViewMatrix(view);
		debugcamera.GetProjectionMatrix(proj);

		VKMatrixInverse(debugworld, unidata.viewproj);
		VKMatrixMultiply(unidata.viewproj, view, proj);
		VKMatrixAssign(debugunis->viewproj, unidata.viewproj);

		VKVec4Set(debugunis->color, 1, 1, 1, 1);
	}

	vkCmdUpdateBuffer(commandbuffer, uniforms->GetBuffer(), 0, sizeof(GlobalUniformData), (const uint32_t*)&unidata);

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

	MEASURE_POINT("");

	vkCmdBeginRenderPass(commandbuffer, &passbegininfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
	{
		std::vector<VkCommandBuffer> cmds(visibletiles.size(), 0);

		for( size_t i = 0; i < visibletiles.size(); ++i )
			cmds[i] = visibletiles[i]->SetForRender(framepump->GetCurrentFrame(), currentimage);

		// only valid command here
		vkCmdExecuteCommands(commandbuffer, (uint32_t)cmds.size(), cmds.data());

		// very important!
		vkCmdNextSubpass(commandbuffer, VK_SUBPASS_CONTENTS_INLINE);

		if( debugmode )
		{
			VulkanAABox tilebox;
			float		zscale[16];
			float		center[3];
			float		halfsize[3];
			float		tilewidth = totalwidth / TILE_GRID_SIZE;
			float		tiledepth = totaldepth / TILE_GRID_SIZE;

			// scale z from [-1, 1] to [0, 1]
			VKMatrixIdentity(zscale);
			zscale[10] = zscale[14] = 0.5f;

			VKMatrixMultiply(debugworld, zscale, debugworld);

			vkCmdBindPipeline(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debugpipeline->GetPipeline());
			vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, debugpipeline->GetPipelineLayout(), 0, 1, debugpipeline->GetDescriptorSets(0), 0, NULL);
			vkCmdSetViewport(commandbuffer, 0, 1, debugpipeline->GetViewport());
			vkCmdSetScissor(commandbuffer, 0, 1, debugpipeline->GetScissor());
			
			VKVec4Set(debugcolor, 0, 1, 1, 1);

			// draw frustum
			vkCmdPushConstants(commandbuffer, debugpipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 64, debugworld);
			vkCmdPushConstants(commandbuffer, debugpipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 64, 16, debugcolor);

			debugmesh->Draw(commandbuffer, 0);

			// draw grid
			for( size_t i = 0, j = 0; i < tiles.size() && j < visibletiles.size(); ++i ) {
				bool visible = (visibletiles[j] == tiles[i]);
				bool marked = (tiles[i]->IsMarkedForDiscard() && !tiles[i]->IsDiscarded(currentimage));

				if( marked )
					VKVec4Set(debugcolor, 1, 0, 0, 1);
				else
					VKVec4Set(debugcolor, 1, 1, 1, 1);

				if( visible || marked )
				{
					size_t y = i / TILE_GRID_SIZE;
					size_t x = i % TILE_GRID_SIZE;

					VKVec3Set(tilebox.Min, totalwidth * -0.5f + x * tilewidth, 0, totaldepth * -0.5f + y * tiledepth);
					VKVec3Set(tilebox.Max, totalwidth * -0.5f + (x + 1) * tilewidth, totalheight, totaldepth * -0.5f + (y + 1) * tiledepth);

					tilebox.GetCenter(center);
					tilebox.GetHalfSize(halfsize);

					VKMatrixScaling(debugworld, halfsize[0], halfsize[1], halfsize[2]);

					debugworld[12] = center[0];
					debugworld[13] = center[1];
					debugworld[14] = center[2];

					vkCmdPushConstants(commandbuffer, debugpipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, 64, debugworld);
					vkCmdPushConstants(commandbuffer, debugpipeline->GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 64, 16, debugcolor);

					debugmesh->Draw(commandbuffer, false);
				}

				if( visible )
					++j;
			}
		}
	}
	vkCmdEndRenderPass(commandbuffer);

	MEASURE_POINT("Encode");

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

	// present to window
	MEASURE_POINT("");
	framepump->Present();

	MEASURE_POINT("Present");
}

void FrameFinished(uint32_t frameid)
{
	for( size_t i = 0; i < tiles.size(); ++i ) {
		tiles[i]->Discard(frameid);
	}
}
