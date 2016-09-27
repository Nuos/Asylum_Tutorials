
#ifndef _VKX_H_
#define _VKX_H_

#include <vector>
#include <set>
#include <map>
#include <cassert>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <vulkan/spirv.h>
//#include <spirv-tools/libspirv.h>

//#define USE_VULKAN_PREFIX		// in project settings
#include "3Dmath.h"
#include "orderedarray.hpp"

#ifdef _DEBUG
#	define VK_ASSERT(x)	assert(x)
#else
#	define VK_ASSERT(x)	if( !(x) ) throw 1
#endif

#define VK_NUM_QUEUED_FRAMES	2
#define VK_ARRAY_SIZE(x)		(sizeof(x) / sizeof(x[0]))
#define VK_SAFE_DELETE(x)		if( x ) { delete (x); (x) = NULL; }
#define VK_SAFE_RELEASE(x)		if( x ) { (x)->Release(); (x) = NULL; }

class VulkanImage;

struct VulkanDriverInfo
{
	VkInstance							inst;
	VkSurfaceKHR						surface;
	VkDevice							device;
	VkCommandPool						commandpool;
	VkSwapchainKHR						swapchain;
	VkQueue								graphicsqueue;		// this will be used for drawing
	//VkPipelineCache					pipelinecache;
	VkDebugReportCallbackEXT			callback;

	VkPhysicalDevice*					gpus;
	VkPhysicalDeviceProperties			deviceprops;
	VkPhysicalDeviceFeatures			devicefeatures;
	VkPhysicalDeviceMemoryProperties	memoryprops;
	VkQueueFamilyProperties*			queueprops;			// for primary adapter
	VkSurfaceCapabilitiesKHR			surfacecaps;		// for primary adapter
	VkPresentModeKHR*					presentmodes;		// for primary adapter
	VkImage*							swapchainimages;
	VkImageView*						swapchainimageviews;

	VkFormat							format;				// backbuffer format
	uint32_t							gpucount;
	uint32_t							presentmodecount;
	uint32_t							queuecount;			// for primary adapter
	uint32_t							graphicsqueueid;
	uint32_t							computequeueid;
	uint32_t							swapchainimgcount;

	// already declared in global scope...
	PFN_vkCreateDebugReportCallbackEXT	vkCreateDebugReportCallbackEXT;
	PFN_vkDebugReportMessageEXT			vkDebugReportMessageEXT;
	PFN_vkDestroyDebugReportCallbackEXT	vkDestroyDebugReportCallbackEXT;
};

struct VulkanCommonVertex
{
	float x, y, z;
	float nx, ny, nz;
	float u, v;
};

struct VulkanTBNVertex
{
	float x, y, z;
	float nx, ny, nz;
	float u, v;
	float tx, ty, tz;
	float bx, by, bz;
};

struct VulkanScreenVertex
{
	float x, y, z;
	float u, v;
};

struct VulkanAttributeRange
{
	uint32_t	PrimitiveType;	// must match pipeline
	uint32_t	AttribId;
	uint32_t	IndexStart;
	uint32_t	IndexCount;
	uint32_t	VertexStart;
	uint32_t	VertexCount;
};

struct VulkanMaterial
{
	VulkanColor		Diffuse;
	VulkanColor		Ambient;
	VulkanColor		Specular;
	VulkanColor		Emissive;
	float			Power;
	VulkanImage*	Texture;
	VulkanImage*	NormalMap;

	VulkanMaterial();
	~VulkanMaterial();
};

typedef enum VkMemoryPropertyFlagBitsEx {
	VK_MEMORY_PROPERTY_SHARED_BIT = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
	VK_MEMORY_PROPERTY_SHARED_COHERENT_BIT = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
} VkMemoryPropertyFlagBitsEx;

struct VulkanSubAllocation
{
	VkDeviceMemory	memory;
	VkDeviceSize	offset;
	uint32_t		heap;

	VulkanSubAllocation() {
		memory	= NULL;
		offset	= 0;
		heap	= VK_MAX_MEMORY_HEAPS;
	}

	inline operator bool() {
		return (memory != NULL);
	}
};

/**
 * \brief Max memory allocation count is 4096 so we must suballocate.
 */
class VulkanMemorySubAllocator
{
	struct AllocationRecord
	{
		VkDeviceSize	offset;
		VkDeviceSize	size;
		mutable bool	mapped;

		AllocationRecord() {
			offset = size = 0;
			mapped = false;
		}

		AllocationRecord(VkDeviceSize off) {
			offset = off;
			size = 0;
			mapped = false;
		}

		inline bool operator <(const AllocationRecord& other) const {
			return (offset < other.offset);
		}
	};

	typedef std::set<AllocationRecord> AllocationSet;

	struct MemoryBatch
	{
		AllocationSet		allocations;
		VkDeviceMemory		memory;
		VkDeviceSize		totalsize;
		mutable uint8_t*	mappedrange;
		uint32_t			heapindex;
		mutable uint16_t	mappedcount;
		bool				isoptimal;

		const AllocationRecord& FindRecord(const VulkanSubAllocation& alloc) const;

		inline bool operator <(const MemoryBatch& other) const {
			return (memory < other.memory);
		}
	};

	typedef mystl::orderedarray<MemoryBatch> MemoryBatchArray;

private:
	static VulkanMemorySubAllocator* _inst;

	MemoryBatchArray batchesforheap[VK_MAX_MEMORY_HEAPS];

	VulkanMemorySubAllocator();
	~VulkanMemorySubAllocator();

	VkDeviceSize		AdjustBufferOffset(VkDeviceSize offset, VkDeviceSize alignment, VkBufferUsageFlags usageflags);
	VkDeviceSize		AdjustImageOffset(VkDeviceSize offset, VkDeviceSize alignment);
	const MemoryBatch&	FindBatchForAlloc(const VulkanSubAllocation& alloc);
	const MemoryBatch&	FindSuitableBatch(VkDeviceSize& outoffset, VkMemoryRequirements memreqs, VkFlags requirements, VkFlags usageflags, bool optimal);

public:
	static VulkanMemorySubAllocator& Instance();
	static void Release();
	
	VulkanSubAllocation	AllocateForBuffer(VkMemoryRequirements memreqs, VkFlags requirements, VkBufferUsageFlags usageflags);
	VulkanSubAllocation	AllocateForImage(VkMemoryRequirements memreqs, VkFlags requirements, VkImageTiling tiling);
	void				Deallocate(VulkanSubAllocation& alloc);
	uint32_t			GetMemoryTypeForFlags(uint32_t memtype, VkFlags requirements);
	void*				MapMemory(const VulkanSubAllocation& alloc, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags);
	void				UnmapMemory(const VulkanSubAllocation& alloc);
};

inline VulkanMemorySubAllocator& VulkanMemoryManager() {
	return VulkanMemorySubAllocator::Instance();
}

/**
 * \brief Instead of smart ptrs.
 */
class VulkanRefCountable
{
private:
	uint32_t refcount;

protected:
	VulkanRefCountable();
	virtual ~VulkanRefCountable();

public:
	void AddRef();
	void Release();
};

/**
 * \brief Don't load content items more than once.
 */
class VulkanContentRegistry
{
	typedef std::map<std::string, VulkanImage*> ImageMap;

private:
	static VulkanContentRegistry* _inst;

	ImageMap images;

	VulkanContentRegistry();
	~VulkanContentRegistry();

public:
	static VulkanContentRegistry& Instance();
	static void Release();

	void RegisterImage(const std::string& file, VulkanImage* image);
	void UnregisterImage(VulkanImage* image);

	VulkanImage* PointerImage(const std::string& file);
};

inline VulkanContentRegistry& VulkanContentManager() {
	return VulkanContentRegistry::Instance();
}

/**
 * \brief Buffer with arbitrary data.
 */
class VulkanBuffer
{
private:
	VkDescriptorBufferInfo	bufferinfo;
	VkMemoryRequirements	memreqs;
	VkMappedMemoryRange		mappedrange;

	VulkanSubAllocation		memory;
	VulkanSubAllocation		stagingmemory;

	VkBuffer				buffer;
	VkBuffer				stagingbuffer;

	VkDeviceSize			originalsize;
	VkFlags					exflags;
	void*					contents;

	VulkanBuffer();

public:
	~VulkanBuffer();

	static VulkanBuffer* Create(VkBufferUsageFlags usage, VkDeviceSize size, VkFlags flags);

	void* MapContents(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags = 0);
	void UnmapContents();
	void UploadToVRAM(VkCommandBuffer commandbuffer);
	void DeleteStagingBuffer();

	inline VkBuffer GetBuffer()										{ return buffer; }
	inline const VkDescriptorBufferInfo* GetBufferInfo() const		{ return &bufferinfo; }	// TODO: ne pointer
	inline VkDeviceSize GetSize() const								{ return memreqs.size; }
};

/**
 * \brief Texture or attachment.
 */
class VulkanImage : public VulkanRefCountable
{
private:
	VkDescriptorImageInfo	imageinfo;
	VkMemoryRequirements	memreqs;
	VkExtent3D				extents;

	VkImage					image;
	VkImageView				imageview;
	VkSampler				sampler;
	VkFormat				format;
	VkAccessFlags			access;

	VulkanSubAllocation		memory;
	VulkanBuffer*			stagingbuffer;
	uint32_t				mipmapcount;
	bool					cubemap;

	VulkanImage();
	~VulkanImage();

public:
	static VulkanImage* Create2D(VkFormat format, uint32_t width, uint32_t height, uint32_t miplevels, VkImageUsageFlags usage);
	static VulkanImage* CreateFromFile(const char* file, bool srgb);
	static VulkanImage* CreateFromDDSCubemap(const char* file, bool srgb);

	static size_t CalculateImageSizeAndMipmapCount(uint32_t& nummipsout, VkFormat format, uint32_t width, uint32_t height);
	static size_t CalculateSliceSize(VkFormat format, uint32_t width, uint32_t height);
	static size_t CalculateByteSize(VkFormat format);

	void* MapContents(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags = 0);
	void StoreLayout(VkAccessFlags newaccess, VkImageLayout newlayout);
	void UnmapContents();
	void UploadToVRAM(VkCommandBuffer commandbuffer, bool generatemips = true);
	void DeleteStagingBuffer();

	inline VkImage GetImage()									{ return image; }
	inline VkImageView GetImageView()							{ return imageview; }
	inline const VkDescriptorImageInfo* GetImageInfo() const	{ return &imageinfo; }
	inline VkSampler GetSampler()								{ return sampler; }
	inline VkFormat GetFormat() const							{ return format; }
	inline VkImageLayout GetLayout() const						{ return imageinfo.imageLayout; }
	inline VkAccessFlags GetAccess() const						{ return access; }
	inline uint32_t GetMipMapCount() const						{ return mipmapcount; }
	inline uint32_t GetArraySize() const						{ return (cubemap ? 6 : 1); }
};

/**
 * \brief Render pass.
 */
class VulkanRenderPass
{
	typedef std::vector<VkAttachmentDescription> AttachmentArray;
	typedef std::vector<VulkanImage*> ImageArray;

private:
	AttachmentArray				attachmentdescs;
	ImageArray					attachmentimages;
	VkRenderPass				renderpass;
	VkFramebuffer				framebuffer;
	VkClearValue*				clearcolors;
	uint32_t					framewidth;
	uint32_t					frameheight;

public:
	VulkanRenderPass(uint32_t width, uint32_t height);
	~VulkanRenderPass();

	void AttachImage(VulkanImage* image, VkAttachmentLoadOp loadop, VkAttachmentStoreOp storeop, VkImageLayout initiallayout, VkImageLayout finallayout);
	void Begin(VkCommandBuffer commandbuffer, VkSubpassContents contents, const VulkanColor& clearcolor, float cleardepth, uint8_t clearstencil);
	void End(VkCommandBuffer commandbuffer);
	void NextSubpass(VkSubpassContents contents);

	//void SetSubpassColorAttachments(uint32_t subpass, ...);
	//void SetSubpassDepthAttachment(uint32_t subpass, ...);

	bool Assemble();

	inline VkRenderPass GetRenderPass()	{ return renderpass; }
};

/**
 * \brief For specialization constants.
 */
class VulkanSpecializationInfo
{
	typedef std::vector<VkSpecializationMapEntry> EntryArray;

private:
	VkSpecializationInfo	specinfo;
	EntryArray				entries;

public:
	VulkanSpecializationInfo();
	VulkanSpecializationInfo(const VulkanSpecializationInfo& other);
	~VulkanSpecializationInfo();

	void AddInt(uint32_t constantID, int32_t value);
	void AddUInt(uint32_t constantID, uint32_t value);
	void AddFloat(uint32_t constantID, float value);

	inline const VkSpecializationInfo* GetSpecializationInfo() const	{ return &specinfo; }
};

/**
 * \brief Common elements of pipelines.
 */
class VulkanBasePipeline
{
	typedef std::vector<VkPipelineShaderStageCreateInfo> ShaderStageArray;
	typedef std::vector<VkDescriptorSetLayoutBinding> DescSetLayoutBindingArray;
	typedef std::vector<VkDescriptorBufferInfo> DescBufferInfoArray;
	typedef std::vector<VkDescriptorImageInfo> DescImageInfoArray;
	typedef std::vector<VkPushConstantRange> PushConstantRangeArray;
	typedef std::vector<VkShaderModule> ShaderModuleArray;
	typedef std::vector<VkDescriptorSet> DescriptorSetArray;

	struct DescriptorSetGroup {
		VkDescriptorPool			descriptorpool;
		VkDescriptorSetLayout		descsetlayout;
		
		DescSetLayoutBindingArray	bindings;			// what kind of buffers will be bound
		DescriptorSetArray			descriptorsets;
		DescBufferInfoArray			descbufferinfos;	// bound buffers
		DescImageInfoArray			descimageinfos;		// bound images

		DescriptorSetGroup();

		void Reset();
	};

	typedef std::vector<DescriptorSetGroup> DescriptorSetGroupArray;
	typedef std::vector<VulkanSpecializationInfo> SpecInfoArray;

protected:
	typedef std::vector<unsigned int> SPIRVByteCode;

	struct BaseTemporaryData {
		ShaderStageArray		shaderstages;
		SpecInfoArray			specinfos;
		PushConstantRangeArray	pushconstantranges;

		BaseTemporaryData();
	};

	ShaderModuleArray			shadermodules;
	DescriptorSetGroupArray		descsetgroups;			// preallocated descriptor sets organized by layout
	DescriptorSetGroup			currentgroup;

	VkPipeline					pipeline;
	VkPipelineLayout			pipelinelayout;
	BaseTemporaryData*			basetempdata;

	VulkanBasePipeline();
	virtual ~VulkanBasePipeline();

	bool Assemble();

public:
	bool AddShader(VkShaderStageFlagBits type, const char* file, const VulkanSpecializationInfo& specinfo = VulkanSpecializationInfo());
	void AddPushConstantRange(VkShaderStageFlags stages, uint32_t offset, uint32_t size);

	// descriptor set related methods
	void AllocateDescriptorSets(uint32_t numsets);
	void SetDescriptorSetLayoutBufferBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage);
	void SetDescriptorSetLayoutImageBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage);
	void SetDescriptorSetGroupBufferInfo(uint32_t group, uint32_t binding, const VkDescriptorBufferInfo* info);
	void SetDescriptorSetGroupImageInfo(uint32_t group, uint32_t binding, const VkDescriptorImageInfo* info);
	void UpdateDescriptorSet(uint32_t group, uint32_t set);

	inline VkPipeline GetPipeline()											{ return pipeline; }
	inline VkPipelineLayout GetPipelineLayout()								{ return pipelinelayout; }
	inline const VkDescriptorSet* GetDescriptorSets(uint32_t group) const	{ return descsetgroups[group].descriptorsets.data(); }
	inline size_t GetNumDescriptorSets(uint32_t group) const				{ return descsetgroups[group].descriptorsets.size(); }
};

/**
 * \brief Graphics pipeline.
 */
class VulkanGraphicsPipeline : public VulkanBasePipeline
{
	typedef std::vector<VkPipelineColorBlendAttachmentState> ColorAttachmentBlendStateArray;
	typedef std::vector<VkVertexInputAttributeDescription> AttributeList;
	typedef std::vector<VkVertexInputBindingDescription> VertexInputBindingArray;

	struct GraphicsTemporaryData : BaseTemporaryData {
		AttributeList							attributes;				// vertex layout
		ColorAttachmentBlendStateArray			blendstates;			// blending mode of render targets
		VertexInputBindingArray					vertexbindings;			// bound vertex buffer descriptions

		VkPipelineDynamicStateCreateInfo		dynamicstate;
		VkPipelineVertexInputStateCreateInfo	vertexinputstate;
		VkPipelineInputAssemblyStateCreateInfo	inputassemblystate;
		VkPipelineRasterizationStateCreateInfo	rasterizationstate;
		VkPipelineColorBlendStateCreateInfo		colorblendstate;
		VkPipelineViewportStateCreateInfo		viewportstate;
		VkPipelineDepthStencilStateCreateInfo	depthstencilstate;
		VkPipelineMultisampleStateCreateInfo	multisamplestate;

		GraphicsTemporaryData();
	};

private:
	GraphicsTemporaryData*	tempdata;
	VkViewport				viewport;
	VkRect2D				scissor;

public:
	VulkanGraphicsPipeline();
	~VulkanGraphicsPipeline();

	bool Assemble(VkRenderPass renderpass);

	// graphics related methods
	void SetInputAttribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
	void SetVertexInputBinding(uint32_t location, const VkVertexInputBindingDescription& desc);
	void SetInputAssembler(VkPrimitiveTopology topology, VkBool32 primitiverestart);
	void SetRasterizer(VkPolygonMode fillmode, VkCullModeFlags cullmode);
	void SetDepth(VkBool32 depthenable, VkBool32 depthwriteenable, VkCompareOp compareop);
	void SetBlendState(uint32_t attachment, VkBool32 enable, VkBlendOp colorop, VkBlendFactor srcblend, VkBlendFactor destblend);
	void SetViewport(float x, float y, float width, float height, float minz = 0.0f, float maxz = 1.0f);
	void SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height);

	inline const VkViewport* GetViewport() const							{ return &viewport; }
	inline const VkRect2D* GetScissor() const								{ return &scissor; }
};

/**
 * \brief Compute pipeline.
 */
class VulkanComputePipeline : public VulkanBasePipeline
{
public:
	VulkanComputePipeline();
	~VulkanComputePipeline();

	bool Assemble();
};

/**
 * \brief Mesh of triangle lists.
 */
class VulkanBasicMesh
{
private:
	VulkanAABox							boundingbox;
	VulkanBuffer*						vertexbuffer;
	VulkanBuffer*						indexbuffer;
	VulkanBuffer*						unibuffer;
	VulkanAttributeRange*				subsettable;
	VulkanMaterial*						materials;

	VkVertexInputAttributeDescription*	descriptor;
	VkDescriptorBufferInfo				unibufferinfo;

	uint8_t*							mappedvdata;
	uint8_t*							mappedidata;
	uint8_t*							mappedudata;
	VkDeviceSize						baseoffset;
	VkDeviceSize						indexoffset;
	VkDeviceSize						uniformoffset;
	VkDeviceSize						totalsize;
	uint32_t							vstride;
	uint32_t							vertexcount;
	uint32_t							indexcount;
	uint32_t							numsubsets;
	VkIndexType							indexformat;
	bool								inherited;

public:
	static VulkanBasicMesh* LoadFromQM(const char* file, VulkanBuffer* buffer = 0, VkDeviceSize offset = 0);

	VulkanBasicMesh(uint32_t numvertices, uint32_t numindices, uint32_t vertexstride, VulkanBuffer* buff = 0, VkDeviceSize off = 0);
	~VulkanBasicMesh();

	void Draw(VkCommandBuffer commandbuffer, VulkanGraphicsPipeline* pipeline, bool rebind = true);
	void DrawSubset(VkCommandBuffer commandbuffer, uint32_t index, VulkanGraphicsPipeline* pipeline, bool rebind = true);
	void DrawSubsetInstanced(VkCommandBuffer commandbuffer, uint32_t index, VulkanGraphicsPipeline* pipeline, uint32_t numinstances, bool rebind = true);
	void UploadToVRAM(VkCommandBuffer commandbuffer);
	void DeleteStagingBuffers();
	void GenerateTangentFrame();

	void* GetVertexBufferPointer();
	void* GetIndexBufferPointer();
	void* GetUniformBufferPointer();

	inline VkDeviceSize GetTotalSize() const							{ return totalsize; }

	inline uint32_t GetVertexStride() const								{ return vstride; }
	inline uint32_t GetIndexStride() const								{ return (indexformat == VK_INDEX_TYPE_UINT16 ? 2 : 4); }
	inline uint32_t GetNumVertices() const								{ return vertexcount; }
	inline uint32_t GetNumPolygons() const								{ return indexcount / 3; }
	inline uint32_t GetNumSubsets() const								{ return numsubsets; }

	inline const VkDescriptorBufferInfo* GetUniformBufferInfo() const	{ return &unibufferinfo; }
	inline const VulkanMaterial& GetMaterial(uint32_t subset) const		{ return materials[subset]; }
	inline const VulkanAABox& GetBoundingBox() const					{ return boundingbox; }
};

/**
 * \brief Compact class for barriers.
 */
class VulkanPipelineBarrierBatch
{
private:
	VkPipelineStageFlags	src;
	VkPipelineStageFlags	dst;

	std::vector<VkBufferMemoryBarrier>	buffbarriers;
	std::vector<VkImageMemoryBarrier>	imgbarriers;

public:
	VulkanPipelineBarrierBatch(VkPipelineStageFlags srcstage, VkPipelineStageFlags dststage);

	void BufferAccessBarrier(VkBuffer buffer, VkAccessFlags srcaccess, VkAccessFlags dstaccess, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
	void ImageLayoutTransfer(VkImage image, VkAccessFlags srcaccess, VkAccessFlags dstaccess, VkImageAspectFlags aspectmask, VkImageLayout oldlayout, VkImageLayout newlayout);
	void ImageLayoutTransfer(VulkanImage* image, VkAccessFlags dstaccess, VkImageAspectFlags aspectmask, VkImageLayout newlayout);
	void Enlist(VkCommandBuffer commandbuffer);
	void Reset();
	void Reset(VkPipelineStageFlags srcstage, VkPipelineStageFlags dststage);
};

/**
 * \brief For frame queueing.
 */
class VulkanFramePump
{
private:
	VkCommandBuffer	commandbuffers[VK_NUM_QUEUED_FRAMES];
	VkSemaphore		acquiresemas[VK_NUM_QUEUED_FRAMES];	// for vkAcquireNextImageKHR
	VkSemaphore		presentsemas[VK_NUM_QUEUED_FRAMES];	// for vkQueuePresentKHR
	VkFence			fences[VK_NUM_QUEUED_FRAMES];		// to know when a submit finished
	uint32_t		buffersinflight;
	uint32_t		currentframe;
	uint32_t		currentdrawable;

public:
	void (*FrameFinished)(uint32_t);

	VulkanFramePump();
	~VulkanFramePump();

	VkCommandBuffer GetNextCommandBuffer();
	uint32_t GetNextDrawable();
	void Present();

	inline uint32_t GetCurrentFrame() const	{ return currentframe; }
};

// functions
VkVertexInputBindingDescription VulkanMakeBindingDescription(uint32_t binding, VkVertexInputRate inputrate, uint32_t stride);
VkCommandBuffer VulkanCreateTempCommandBuffer(bool begin = true);
void VulkanSubmitTempCommandBuffer(VkCommandBuffer commandbuffer, bool wait = true);

bool VulkanQueryFormatSupport(VkFormat format, VkFormatFeatureFlags features);

extern VulkanDriverInfo driverinfo;

#endif
