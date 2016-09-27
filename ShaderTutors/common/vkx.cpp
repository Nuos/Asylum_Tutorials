
#include "vkx.h"
#include "dds.h"

#include <iostream>
#include <cmath>
#include <GdiPlus.h>

//#define __DEBUG_SUBALLOCATOR

extern void AssembleSPIRV(std::string& result, const std::string& file);

VulkanDriverInfo driverinfo;
ULONG_PTR gdiplustoken = 0;

// what: \.{[a-zA-Z0-9]+} = {[0-9]+};
// with: /*.\1 = */ \2,

static TBuiltInResource SPIRVResources = {
	/*.maxLights = */ 32,
	/*.maxClipPlanes = */ 6,
	/*.maxTextureUnits = */ 32,
	/*.maxTextureCoords = */ 32,
	/*.maxVertexAttribs = */ 64,
	/*.maxVertexUniformComponents = */ 4096,
	/*.maxVaryingFloats = */ 64,
	/*.maxVertexTextureImageUnits = */ 32,
	/*.maxCombinedTextureImageUnits = */ 80,
	/*.maxTextureImageUnits = */ 32,
	/*.maxFragmentUniformComponents = */ 4096,
	/*.maxDrawBuffers = */ 32,
	/*.maxVertexUniformVectors = */ 128,
	/*.maxVaryingVectors = */ 8,
	/*.maxFragmentUniformVectors = */ 16,
	/*.maxVertexOutputVectors = */ 16,
	/*.maxFragmentInputVectors = */ 15,
	/*.minProgramTexelOffset = */ -8,
	/*.maxProgramTexelOffset = */ 7,
	/*.maxClipDistances = */ 8,
	/*.maxComputeWorkGroupCountX = */ 65535,
	/*.maxComputeWorkGroupCountY = */ 65535,
	/*.maxComputeWorkGroupCountZ = */ 65535,
	/*.maxComputeWorkGroupSizeX = */ 1024,
	/*.maxComputeWorkGroupSizeY = */ 1024,
	/*.maxComputeWorkGroupSizeZ = */ 64,
	/*.maxComputeUniformComponents = */ 1024,
	/*.maxComputeTextureImageUnits = */ 16,
	/*.maxComputeImageUniforms = */ 8,
	/*.maxComputeAtomicCounters = */ 8,
	/*.maxComputeAtomicCounterBuffers = */ 1,
	/*.maxVaryingComponents = */ 60,
	/*.maxVertexOutputComponents = */ 64,
	/*.maxGeometryInputComponents = */ 64,
	/*.maxGeometryOutputComponents = */ 128,
	/*.maxFragmentInputComponents = */ 128,
	/*.maxImageUnits = */ 8,
	/*.maxCombinedImageUnitsAndFragmentOutputs = */ 8,
	/*.maxCombinedShaderOutputResources = */ 8,
	/*.maxImageSamples = */ 0,
	/*.maxVertexImageUniforms = */ 0,
	/*.maxTessControlImageUniforms = */ 0,
	/*.maxTessEvaluationImageUniforms = */ 0,
	/*.maxGeometryImageUniforms = */ 0,
	/*.maxFragmentImageUniforms = */ 8,
	/*.maxCombinedImageUniforms = */ 8,
	/*.maxGeometryTextureImageUnits = */ 16,
	/*.maxGeometryOutputVertices = */ 256,
	/*.maxGeometryTotalOutputComponents = */ 1024,
	/*.maxGeometryUniformComponents = */ 1024,
	/*.maxGeometryVaryingComponents = */ 64,
	/*.maxTessControlInputComponents = */ 128,
	/*.maxTessControlOutputComponents = */ 128,
	/*.maxTessControlTextureImageUnits = */ 16,
	/*.maxTessControlUniformComponents = */ 1024,
	/*.maxTessControlTotalOutputComponents = */ 4096,
	/*.maxTessEvaluationInputComponents = */ 128,
	/*.maxTessEvaluationOutputComponents = */ 128,
	/*.maxTessEvaluationTextureImageUnits = */ 16,
	/*.maxTessEvaluationUniformComponents = */ 1024,
	/*.maxTessPatchComponents = */ 120,
	/*.maxPatchVertices = */ 32,
	/*.maxTessGenLevel = */ 64,
	/*.maxViewports = */ 16,
	/*.maxVertexAtomicCounters = */ 0,
	/*.maxTessControlAtomicCounters = */ 0,
	/*.maxTessEvaluationAtomicCounters = */ 0,
	/*.maxGeometryAtomicCounters = */ 0,
	/*.maxFragmentAtomicCounters = */ 8,
	/*.maxCombinedAtomicCounters = */ 8,
	/*.maxAtomicCounterBindings = */ 1,
	/*.maxVertexAtomicCounterBuffers = */ 0,
	/*.maxTessControlAtomicCounterBuffers = */ 0,
	/*.maxTessEvaluationAtomicCounterBuffers = */ 0,
	/*.maxGeometryAtomicCounterBuffers = */ 0,
	/*.maxFragmentAtomicCounterBuffers = */ 1,
	/*.maxCombinedAtomicCounterBuffers = */ 1,
	/*.maxAtomicCounterBufferSize = */ 16384,
	/*.maxTransformFeedbackBuffers = */ 4,
	/*.maxTransformFeedbackInterleavedComponents = */ 64,
	/*.maxCullDistances = */ 8,
	/*.maxCombinedClipAndCullDistances = */ 8,
	/*.maxSamples = */ 4,

	/*.limits.nonInductiveForLoops = */ 1,
	/*.limits.whileLoops = */ 1,
	/*.limits.doWhileLoops = */ 1,
	/*.limits.generalUniformIndexing = */ 1,
	/*.limits.generalAttributeMatrixVectorIndexing = */ 1,
	/*.limits.generalVaryingIndexing = */ 1,
	/*.limits.generalSamplerIndexing = */ 1,
	/*.limits.generalVariableIndexing = */ 1,
	/*.limits.generalConstantMatrixVectorIndexing = */ 1
};

static EShLanguage FindLanguage(VkShaderStageFlagBits type)
{
	switch( type )
	{
	case VK_SHADER_STAGE_VERTEX_BIT:					return EShLangVertex;
	case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:		return EShLangTessControl;
	case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:	return EShLangTessEvaluation;
	case VK_SHADER_STAGE_GEOMETRY_BIT:					return EShLangGeometry;
	case VK_SHADER_STAGE_FRAGMENT_BIT:					return EShLangFragment;
	case VK_SHADER_STAGE_COMPUTE_BIT:					return EShLangCompute;
	default:											return EShLangVertex;
	}
}

static void ReadString(FILE* f, char* buff)
{
	size_t ind = 0;
	char ch = fgetc(f);

	while( ch != '\n' )
	{
		buff[ind] = ch;
		ch = fgetc(f);
		++ind;
	}

	buff[ind] = '\0';
}

static Gdiplus::Bitmap* Win32LoadPicture(const std::wstring& file)
{
	if( gdiplustoken == 0 )
	{
		Gdiplus::GdiplusStartupInput gdiplustartup;
		Gdiplus::GdiplusStartup(&gdiplustoken, &gdiplustartup, NULL);
	}

	Gdiplus::Bitmap* bitmap = Gdiplus::Bitmap::FromFile(file.c_str(), FALSE);

	if( bitmap->GetLastStatus() != Gdiplus::Ok ) {
		delete bitmap;
		bitmap = 0;
	}

	return bitmap;
}

VulkanMaterial::VulkanMaterial()
{
	Texture = 0;
	NormalMap = 0;
}

VulkanMaterial::~VulkanMaterial()
{
	if( Texture )
		Texture->Release();

	if( NormalMap )
		NormalMap->Release();
}

//*************************************************************************************************************
//
// VulkanMemorySubAllocator impl
//
//*************************************************************************************************************

VulkanMemorySubAllocator* VulkanMemorySubAllocator::_inst = 0;

const VulkanMemorySubAllocator::AllocationRecord& VulkanMemorySubAllocator::MemoryBatch::FindRecord(const VulkanSubAllocation& alloc) const
{
	AllocationSet::const_iterator it = allocations.find(alloc.offset);
	VK_ASSERT(it != allocations.end());

	return *it;
}

VulkanMemorySubAllocator& VulkanMemorySubAllocator::Instance()
{
	if( !_inst )
		_inst = new VulkanMemorySubAllocator();

	return *_inst;
}

void VulkanMemorySubAllocator::Release()
{
	if( _inst )
		delete _inst;

	_inst = 0;
}

VulkanMemorySubAllocator::VulkanMemorySubAllocator()
{
}

VulkanMemorySubAllocator::~VulkanMemorySubAllocator()
{
}

VkDeviceSize VulkanMemorySubAllocator::AdjustBufferOffset(VkDeviceSize offset, VkDeviceSize alignment, VkBufferUsageFlags usageflags)
{
	VkDeviceSize minalignment = 4;

	if( usageflags & VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT || usageflags & VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT )
		minalignment = driverinfo.deviceprops.limits.minTexelBufferOffsetAlignment;
	else if( usageflags & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT )
		minalignment = driverinfo.deviceprops.limits.minUniformBufferOffsetAlignment;
	else if( usageflags & VK_BUFFER_USAGE_STORAGE_BUFFER_BIT )
		minalignment = driverinfo.deviceprops.limits.minStorageBufferOffsetAlignment;

	VK_ASSERT((alignment % minalignment) == 0);
	return ((offset % alignment) ? (offset + alignment - (offset % alignment)) : offset);
}

VkDeviceSize VulkanMemorySubAllocator::AdjustImageOffset(VkDeviceSize offset, VkDeviceSize alignment)
{
	return ((offset % alignment) ? (offset + alignment - (offset % alignment)) : offset);
}

const VulkanMemorySubAllocator::MemoryBatch& VulkanMemorySubAllocator::FindBatchForAlloc(const VulkanSubAllocation& alloc)
{
	MemoryBatch temp;
	size_t index;

	temp.memory = alloc.memory;
	index = batchesforheap[alloc.heap].find(temp);

	VK_ASSERT(index != MemoryBatchArray::npos);
	return batchesforheap[alloc.heap][index];
}

const VulkanMemorySubAllocator::MemoryBatch& VulkanMemorySubAllocator::FindSuitableBatch(VkDeviceSize& outoffset, VkMemoryRequirements memreqs, VkFlags requirements, VkFlags usageflags, bool optimal)
{
	uint32_t index = GetMemoryTypeForFlags(memreqs.memoryTypeBits, requirements);
	VK_ASSERT(index < VK_MAX_MEMORY_HEAPS);

	MemoryBatchArray&	batches		= batchesforheap[index];
	VkDeviceSize		emptyspace	= 0;
	VkDeviceSize		offset		= 0;
	size_t				batchid		= SIZE_MAX;

	// find first hole which is good
	for( size_t i = 0; i < batches.size(); ++i )
	{
		MemoryBatch& batch = (MemoryBatch&)batches[i];
		AllocationSet& allocs = batch.allocations;

		if( batch.isoptimal != optimal )
			continue;

		AllocationSet::iterator prev = allocs.begin();
		AllocationSet::iterator it = allocs.begin();

		if( prev == allocs.end() ) {
			// batch is empty
			emptyspace = batches[i].totalsize;
			offset = 0;
			
			if( emptyspace >= memreqs.size ) {
				batchid = i;
				break;
			}
		} else while( prev != allocs.end() ) {
			if( prev == it ) {
				// space before first alloc
				emptyspace = it->offset;
				offset = 0;

				++it;
			} else if( it == allocs.end() ) {
				// space after last alloc
				if( optimal )
					offset = AdjustImageOffset(prev->offset + prev->size, memreqs.alignment);
				else
					offset = AdjustBufferOffset(prev->offset + prev->size, memreqs.alignment, usageflags);

				if( batches[i].totalsize > offset )
					emptyspace = batches[i].totalsize - offset;
				else
					emptyspace = 0;

				prev = it;
			} else {
				// space between allocs
				if( optimal )
					offset = AdjustImageOffset(prev->offset + prev->size, memreqs.alignment);
				else
					offset = AdjustBufferOffset(prev->offset + prev->size, memreqs.alignment, usageflags);

				if( it->offset > offset )
					emptyspace = it->offset - offset;
				else
					emptyspace = 0;

				prev = it;
				++it;
			}

			if( emptyspace >= memreqs.size ) {
				// found a good spot
				batchid = i;
				break;
			}
		}

		if( batchid != SIZE_MAX )
			break;
	}

	if( batchid < batches.size() )
	{
		// found suitable batch
		MemoryBatch& batch = (MemoryBatch&)batches[batchid];
		AllocationRecord record;

		VK_ASSERT(batch.isoptimal == optimal);

		record.offset	= offset;			// must be local (see 'emptyspace')
		record.size		= memreqs.size;

		outoffset = record.offset;
		batch.allocations.insert(record);

#ifdef __DEBUG_SUBALLOCATOR
		if( optimal )
			printf("Optimal memory allocated in batch %llu (offset = %llu, size = %llu)\n", batchid, record.offset, record.size);
#endif

		return batch;
	}

	// create a new batch if possible
	VkDeviceSize maxsize = driverinfo.memoryprops.memoryHeaps[index].size;
	VkDeviceSize allocatedsize = 0;

	if( maxsize == 0 )
		maxsize = SIZE_MAX;

	for( size_t i = 0; i < batches.size(); ++i )
		allocatedsize += batches[i].totalsize;

	if( allocatedsize + memreqs.size > maxsize )
		throw std::bad_alloc(); /* "Memory heap is out of memory" */

	MemoryBatch				newbatch;
	AllocationRecord		record;
	VkMemoryAllocateInfo	allocinfo = {};
	VkResult				res;

	allocinfo.sType				= VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocinfo.pNext				= NULL;
	allocinfo.memoryTypeIndex	= index;
	allocinfo.allocationSize	= std::max<VkDeviceSize>(memreqs.size, 64 * 1024 * 1024);

	// allocate in 64 MB chunks
	res = vkAllocateMemory(driverinfo.device, &allocinfo, NULL, &newbatch.memory);

	if( res != VK_SUCCESS )
		throw std::bad_alloc(); /* "Memory heap is out of memory" */

	newbatch.mappedcount	= 0;
	newbatch.totalsize		= allocinfo.allocationSize;
	newbatch.mappedrange	= 0;
	newbatch.isoptimal		= optimal;
	newbatch.heapindex		= index;

	record.offset			= 0;
	record.size				= memreqs.size;

	newbatch.allocations.insert(record);
	outoffset = record.offset;

#ifdef __DEBUG_SUBALLOCATOR
	if( optimal )
		printf("Optimal memory allocated in batch %llu (offset = %llu, size = %llu)\n", batchid, record.offset, record.size);
#endif

	MemoryBatchArray::pairib result = batches.insert(newbatch);
	VK_ASSERT(result.second);

	return (MemoryBatch&)batches[result.first];
}

VulkanSubAllocation VulkanMemorySubAllocator::AllocateForBuffer(VkMemoryRequirements memreqs, VkFlags requirements, VkBufferUsageFlags usageflags)
{
	VulkanSubAllocation result;
	const MemoryBatch& batch = FindSuitableBatch(result.offset, memreqs, requirements, usageflags, false);

	result.memory	= batch.memory;
	result.heap		= batch.heapindex;

	return result;
}

VulkanSubAllocation VulkanMemorySubAllocator::AllocateForImage(VkMemoryRequirements memreqs, VkFlags requirements, VkImageTiling tiling)
{
	VulkanSubAllocation result;
	const MemoryBatch& batch = FindSuitableBatch(result.offset, memreqs, requirements, 0, (tiling == VK_IMAGE_TILING_OPTIMAL));

	result.memory	= batch.memory;
	result.heap		= batch.heapindex;

	return result;
}

void VulkanMemorySubAllocator::Deallocate(VulkanSubAllocation& alloc)
{
	MemoryBatch& batch = (MemoryBatch&)FindBatchForAlloc(alloc);
	const AllocationRecord& record = batch.FindRecord(alloc);

	batch.allocations.erase(record);

	if( batch.allocations.empty() ) {
		vkFreeMemory(driverinfo.device, batch.memory, NULL);
		batchesforheap[alloc.heap].erase(batch);
	}

	alloc.memory = NULL;
	alloc.offset = 0;
}

uint32_t VulkanMemorySubAllocator::GetMemoryTypeForFlags(uint32_t memtype, VkFlags requirements)
{
	for( uint32_t i = 0; i < VK_MAX_MEMORY_HEAPS; ++i ) {
		if( memtype & 1 ) {
			if( (driverinfo.memoryprops.memoryTypes[i].propertyFlags & requirements) == requirements )
				return i;
		}

		memtype >>= 1;
	}

	return UINT32_MAX;
}

void* VulkanMemorySubAllocator::MapMemory(const VulkanSubAllocation& alloc, VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	const MemoryBatch& batch = FindBatchForAlloc(alloc);
	const AllocationRecord& record = batch.FindRecord(alloc);

	VK_ASSERT(!record.mapped);
	(void)size;

	// NOTE: vkMapMemory is not resursive...
	uint8_t* ret = 0;

	if( batch.mappedcount == 0 )
		vkMapMemory(driverinfo.device, alloc.memory, 0, VK_WHOLE_SIZE, flags, (void**)&batch.mappedrange);

	ret = batch.mappedrange + record.offset + offset;

	record.mapped = true;
	++batch.mappedcount;

	return ret;
}

void VulkanMemorySubAllocator::UnmapMemory(const VulkanSubAllocation& alloc)
{
	const MemoryBatch& batch = FindBatchForAlloc(alloc);
	const AllocationRecord& record = batch.FindRecord(alloc);

	VK_ASSERT(record.mapped);
	VK_ASSERT(batch.mappedcount > 0);

	record.mapped = false;
	--batch.mappedcount;

	if( batch.mappedcount == 0 ) {
		vkUnmapMemory(driverinfo.device, batch.memory);
		batch.mappedrange = 0;
	}
}

//*************************************************************************************************************
//
// VulkanRefCountable impl
//
//*************************************************************************************************************

VulkanRefCountable::VulkanRefCountable()
{
	refcount = 1;
}

VulkanRefCountable::~VulkanRefCountable()
{
	assert(refcount == 0);
}

void VulkanRefCountable::AddRef()
{
	++refcount;
}

void VulkanRefCountable::Release()
{
	VK_ASSERT(refcount > 0);
	--refcount;

	if( refcount == 0 )
		delete this;
}

//*************************************************************************************************************
//
// VulkanContentRegistry impl
//
//*************************************************************************************************************

VulkanContentRegistry* VulkanContentRegistry::_inst = 0;

VulkanContentRegistry& VulkanContentRegistry::Instance()
{
	if( !_inst )
		_inst = new VulkanContentRegistry();

	return *_inst;
}

void VulkanContentRegistry::Release()
{
	if( _inst )
		delete _inst;

	_inst = 0;
}

VulkanContentRegistry::VulkanContentRegistry()
{
}

VulkanContentRegistry::~VulkanContentRegistry()
{
}

void VulkanContentRegistry::RegisterImage(const std::string& file, VulkanImage* image)
{
	std::string name;
	VKGetFile(name, file);

	VK_ASSERT(images.count(name) == 0);
	images.insert(ImageMap::value_type(name, image));
}

void VulkanContentRegistry::UnregisterImage(VulkanImage* image)
{
	for( ImageMap::iterator it = images.begin(); it != images.end(); ++it ) {
		if( it->second == image ) {
			images.erase(it);
			break;
		}
	}
}

VulkanImage* VulkanContentRegistry::PointerImage(const std::string& file)
{
	std::string name;
	VKGetFile(name, file);

	ImageMap::iterator it = images.find(name);

	if( it == images.end() )
		return NULL;

	it->second->AddRef();
	return it->second;
}

//*************************************************************************************************************
//
// VulkanBuffer impl
//
//*************************************************************************************************************

VulkanBuffer::VulkanBuffer()
{
	buffer			= 0;
	stagingbuffer	= 0;
	originalsize	= 0;
	exflags			= 0;
	contents		= 0;

	mappedrange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedrange.pNext = NULL;
}

VulkanBuffer::~VulkanBuffer()
{
	if( contents && stagingmemory )
		VulkanMemoryManager().UnmapMemory(stagingmemory);

	if( buffer )
		vkDestroyBuffer(driverinfo.device, buffer, 0);

	if( stagingbuffer )
		vkDestroyBuffer(driverinfo.device, stagingbuffer, 0);

	if( memory )
		VulkanMemoryManager().Deallocate(memory);

	if( stagingmemory )
		VulkanMemoryManager().Deallocate(stagingmemory);

	buffer		= 0;
	contents	= 0;
}

VulkanBuffer* VulkanBuffer::Create(VkBufferUsageFlags usage, VkDeviceSize size, VkFlags flags)
{
	VkBufferCreateInfo		buffercreateinfo	= {};
	VulkanBuffer*			ret					= new VulkanBuffer();
	VkResult				res;
	bool					needsstaging		= ((flags & VK_MEMORY_PROPERTY_SHARED_BIT) == VK_MEMORY_PROPERTY_SHARED_BIT);

	ret->originalsize = size;
	ret->exflags = flags;

	// create device buffer first
	buffercreateinfo.sType					= VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffercreateinfo.pNext					= NULL;
	buffercreateinfo.usage					= usage;
	buffercreateinfo.size					= size;
	buffercreateinfo.queueFamilyIndexCount	= 0;
	buffercreateinfo.pQueueFamilyIndices	= NULL;
	buffercreateinfo.sharingMode			= VK_SHARING_MODE_EXCLUSIVE;
	buffercreateinfo.flags					= 0;

	if( needsstaging )
	{
		flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
		buffercreateinfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}

	res = vkCreateBuffer(driverinfo.device, &buffercreateinfo, NULL, &ret->buffer);
	
	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	vkGetBufferMemoryRequirements(driverinfo.device, ret->buffer, &ret->memreqs);

	if( needsstaging ) {
		// this is pretty retarded btw.
		needsstaging = (UINT32_MAX == VulkanMemoryManager().GetMemoryTypeForFlags(ret->memreqs.memoryTypeBits, ret->exflags));

		if( !needsstaging )
			flags = ret->exflags;
	}

	ret->memory = VulkanMemoryManager().AllocateForBuffer(ret->memreqs, flags, buffercreateinfo.usage);

	if( !ret->memory ) {
		delete ret;
		return NULL;
	}

	res = vkBindBufferMemory(driverinfo.device, ret->buffer, ret->memory.memory, ret->memory.offset);

	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	if( needsstaging )
	{
		VkMemoryRequirements stagingreqs;

		// create staging buffer
		flags						= (ret->exflags & (~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));
		buffercreateinfo.usage		= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		res = vkCreateBuffer(driverinfo.device, &buffercreateinfo, NULL, &ret->stagingbuffer);
	
		if( res != VK_SUCCESS ) {
			delete ret;
			return NULL;
		}

		vkGetBufferMemoryRequirements(driverinfo.device, ret->stagingbuffer, &stagingreqs);
		ret->stagingmemory = VulkanMemoryManager().AllocateForBuffer(stagingreqs, flags, buffercreateinfo.usage);

		if( !ret->stagingmemory ) {
			delete ret;
			return NULL;
		}

		res = vkBindBufferMemory(driverinfo.device, ret->stagingbuffer, ret->stagingmemory.memory, ret->stagingmemory.offset);

		if( res != VK_SUCCESS ) {
			delete ret;
			return NULL;
		}
	}

	ret->bufferinfo.buffer	= ret->buffer;
	ret->bufferinfo.offset	= 0;
	ret->bufferinfo.range	= size;

	return ret;
}

void* VulkanBuffer::MapContents(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	if( size == 0 )
		size = memreqs.size;

	if( !contents )
	{
		if( stagingmemory )
		{
			contents = VulkanMemoryManager().MapMemory(stagingmemory, offset, size, flags);
			mappedrange.memory = stagingmemory.memory;
		}
		else
		{
			VK_ASSERT(exflags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

			contents = VulkanMemoryManager().MapMemory(memory, offset, size, flags);
			mappedrange.memory = memory.memory;
		}

		mappedrange.offset = offset;
		mappedrange.size = size;
	}

	return contents;
}

void VulkanBuffer::UnmapContents()
{
	if( contents )
	{
		if( !(exflags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) )
			vkFlushMappedMemoryRanges(driverinfo.device, 1, &mappedrange);

		if( stagingmemory )
			VulkanMemoryManager().UnmapMemory(stagingmemory);
		else
			VulkanMemoryManager().UnmapMemory(memory);
	}

	contents = 0;
}

void VulkanBuffer::UploadToVRAM(VkCommandBuffer commandbuffer)
{
	if( stagingbuffer )
	{
		VkBufferCopy region;

		region.srcOffset	= 0;
		region.dstOffset	= 0;
		//region.size			= memreqs.size;
		region.size			= originalsize;

		vkCmdCopyBuffer(commandbuffer, stagingbuffer, buffer, 1, &region);
	}
}

void VulkanBuffer::DeleteStagingBuffer()
{
	if( stagingbuffer )
		vkDestroyBuffer(driverinfo.device, stagingbuffer, 0);

	if( stagingmemory )
		VulkanMemoryManager().Deallocate(stagingmemory);

	stagingbuffer = 0;
}

//*************************************************************************************************************
//
// VulkanImage impl
//
//*************************************************************************************************************

VulkanImage::VulkanImage()
{
	image			= 0;
	imageview		= 0;
	sampler			= 0;
	format			= VK_FORMAT_UNDEFINED;
	access			= 0;
	stagingbuffer	= 0;
	mipmapcount		= 0;
	cubemap			= false;

	imageinfo.imageLayout	= VK_IMAGE_LAYOUT_UNDEFINED;
	imageinfo.imageView		= NULL;
	imageinfo.sampler		= NULL;

	extents.width	= 1;
	extents.height	= 1;
	extents.depth	= 1;
}

VulkanImage::~VulkanImage()
{
	// TODO: szokasos problema...as aliasok tobbszor akarjak eldobni (refcount kell)
	VulkanContentManager().UnregisterImage(this);

	if( stagingbuffer )
		delete stagingbuffer;

	if( sampler )
		vkDestroySampler(driverinfo.device, sampler, 0);

	if( imageview )
		vkDestroyImageView(driverinfo.device, imageview, 0);

	if( image )
		vkDestroyImage(driverinfo.device, image, 0);

	if( memory )
		VulkanMemoryManager().Deallocate(memory);
}

VulkanImage* VulkanImage::Create2D(VkFormat format, uint32_t width, uint32_t height, uint32_t miplevels, VkImageUsageFlags usage)
{
	VkImageCreateInfo		imagecreateinfo		= {};
	VkImageViewCreateInfo	viewcreateinfo		= {};
	VkSamplerCreateInfo		samplercreateinfo	= {};
	VulkanImage*			ret					= new VulkanImage();
	VkFormatProperties		formatprops;
	VkFormatFeatureFlags	feature				= 0;
	VkImageAspectFlags		aspectmask			= 0;
	VkResult				res;

	ret->format = format;

	if( usage & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT )
		usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	if( usage & VK_IMAGE_USAGE_SAMPLED_BIT )
	{
		feature |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
		aspectmask |= VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if( usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT )
	{
		feature |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
		aspectmask |= VK_IMAGE_ASPECT_COLOR_BIT;
	}

	if( usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT )
	{
		feature |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
		aspectmask |= VK_IMAGE_ASPECT_DEPTH_BIT;
	}

	if( format == VK_FORMAT_D16_UNORM_S8_UINT ||
		format == VK_FORMAT_D24_UNORM_S8_UINT ||
		format == VK_FORMAT_D32_SFLOAT_S8_UINT )
	{
		aspectmask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vkGetPhysicalDeviceFormatProperties(driverinfo.gpus[0], format, &formatprops);

	if( formatprops.optimalTilingFeatures & feature )
		imagecreateinfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	else if( formatprops.linearTilingFeatures & feature )
		imagecreateinfo.tiling = VK_IMAGE_TILING_LINEAR;
	else {
		ret->Release();
		return NULL;
	}

	imagecreateinfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imagecreateinfo.pNext					= NULL;
	imagecreateinfo.arrayLayers				= 1;
	imagecreateinfo.extent.width			= width;
	imagecreateinfo.extent.height			= height;
	imagecreateinfo.extent.depth			= 1;
	imagecreateinfo.format					= format;
	imagecreateinfo.imageType				= VK_IMAGE_TYPE_2D;
	imagecreateinfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	imagecreateinfo.mipLevels				= miplevels;
	imagecreateinfo.queueFamilyIndexCount	= 0;
	imagecreateinfo.pQueueFamilyIndices		= NULL;
	imagecreateinfo.samples					= VK_SAMPLE_COUNT_1_BIT;
	imagecreateinfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
	imagecreateinfo.usage					= usage;
	imagecreateinfo.flags					= 0;

	ret->extents = imagecreateinfo.extent;
	ret->mipmapcount = miplevels;

	res = vkCreateImage(driverinfo.device, &imagecreateinfo, 0, &ret->image);

	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	vkGetImageMemoryRequirements(driverinfo.device, ret->image, &ret->memreqs);
	ret->memory = VulkanMemoryManager().AllocateForImage(ret->memreqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imagecreateinfo.tiling);
	
	if( !ret->memory ) {
		delete ret;
		return NULL;
	}

	res = vkBindImageMemory(driverinfo.device, ret->image, ret->memory.memory, ret->memory.offset);
	
	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	viewcreateinfo.sType							= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewcreateinfo.pNext							= NULL;
	viewcreateinfo.components.r						= VK_COMPONENT_SWIZZLE_R;
	viewcreateinfo.components.g						= VK_COMPONENT_SWIZZLE_G;
	viewcreateinfo.components.b						= VK_COMPONENT_SWIZZLE_B;
	viewcreateinfo.components.a						= VK_COMPONENT_SWIZZLE_A;
	viewcreateinfo.format							= format;
	viewcreateinfo.image							= ret->image;
	viewcreateinfo.subresourceRange.aspectMask		= aspectmask;
	viewcreateinfo.subresourceRange.baseArrayLayer	= 0;
	viewcreateinfo.subresourceRange.baseMipLevel	= 0;
	viewcreateinfo.subresourceRange.layerCount		= 1;
	viewcreateinfo.subresourceRange.levelCount		= miplevels;
	viewcreateinfo.viewType							= VK_IMAGE_VIEW_TYPE_2D;
	viewcreateinfo.flags							= 0;

	res = vkCreateImageView(driverinfo.device, &viewcreateinfo, 0, &ret->imageview);

	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	if( usage & VK_IMAGE_USAGE_SAMPLED_BIT )
	{
		samplercreateinfo.sType				= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplercreateinfo.magFilter			= VK_FILTER_LINEAR;
		samplercreateinfo.minFilter			= VK_FILTER_LINEAR;
		samplercreateinfo.mipmapMode		= VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplercreateinfo.addressModeU		= VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplercreateinfo.addressModeV		= VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplercreateinfo.addressModeW		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplercreateinfo.mipLodBias		= 0.0;
		samplercreateinfo.anisotropyEnable	= VK_FALSE,
		samplercreateinfo.maxAnisotropy		= 0;
		samplercreateinfo.compareOp			= VK_COMPARE_OP_NEVER;
		samplercreateinfo.minLod			= 0.0f;
		samplercreateinfo.maxLod			= (float)imagecreateinfo.mipLevels;
		samplercreateinfo.compareEnable		= VK_FALSE;
		samplercreateinfo.borderColor		= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

		res = vkCreateSampler(driverinfo.device, &samplercreateinfo, NULL, &ret->sampler);
	
		if( res != VK_SUCCESS ) {
			delete ret;
			return NULL;
		}
	}

	ret->imageinfo.sampler		= ret->sampler;
	ret->imageinfo.imageView	= ret->imageview;
	ret->imageinfo.imageLayout	= imagecreateinfo.initialLayout;

	return ret;
}

VulkanImage* VulkanImage::CreateFromFile(const char* file, bool srgb)
{
	VulkanImage* ret = VulkanContentManager().PointerImage(file);

	if( ret ) {
		printf("Pointer %s\n", file);
		return ret;
	}

	VkImageCreateInfo		imagecreateinfo		= {};
	VkImageViewCreateInfo	viewcreateinfo		= {};
	VkSamplerCreateInfo		samplercreateinfo	= {};
	VkFormatProperties		formatprops;
	VkResult				res;

	std::wstring			wstr;
	Gdiplus::Bitmap*		bitmap;
	int						length			= (int)strlen(file);
	int						size			= MultiByteToWideChar(CP_UTF8, 0, file, length, 0, 0);

	wstr.resize(size);
	MultiByteToWideChar(CP_UTF8, 0, file, length, &wstr[0], size);

	bitmap = Win32LoadPicture(wstr);

	if( !bitmap ) {
		delete ret;
		return NULL;
	}

	ret = new VulkanImage();
	vkGetPhysicalDeviceFormatProperties(driverinfo.gpus[0], VK_FORMAT_B8G8R8A8_UNORM, &formatprops);

	imagecreateinfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imagecreateinfo.pNext					= NULL;
	imagecreateinfo.arrayLayers				= 1;
	imagecreateinfo.extent.width			= bitmap->GetWidth();
	imagecreateinfo.extent.height			= bitmap->GetHeight();
	imagecreateinfo.extent.depth			= 1;
	imagecreateinfo.format					= (srgb ? VK_FORMAT_B8G8R8A8_SRGB : VK_FORMAT_B8G8R8A8_UNORM);
	imagecreateinfo.imageType				= VK_IMAGE_TYPE_2D;
	imagecreateinfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	imagecreateinfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
	imagecreateinfo.queueFamilyIndexCount	= 0;
	imagecreateinfo.pQueueFamilyIndices		= NULL;
	imagecreateinfo.samples					= VK_SAMPLE_COUNT_1_BIT;
	imagecreateinfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
	imagecreateinfo.usage					= VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
	imagecreateinfo.flags					= 0;

	VulkanImage::CalculateImageSizeAndMipmapCount(imagecreateinfo.mipLevels, imagecreateinfo.format, imagecreateinfo.extent.width, imagecreateinfo.extent.height);

	ret->extents = imagecreateinfo.extent;
	ret->mipmapcount = imagecreateinfo.mipLevels;

	res = vkCreateImage(driverinfo.device, &imagecreateinfo, 0, &ret->image);

	if( res != VK_SUCCESS ) {
		delete ret;
		delete bitmap;

		return NULL;
	}

	vkGetImageMemoryRequirements(driverinfo.device, ret->image, &ret->memreqs);
	ret->memory = VulkanMemoryManager().AllocateForImage(ret->memreqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imagecreateinfo.tiling);

	if( !ret->memory ) {
		delete ret;
		delete bitmap;

		return NULL;
	}

	res = vkBindImageMemory(driverinfo.device, ret->image, ret->memory.memory, ret->memory.offset);
	
	if( res != VK_SUCCESS ) {
		delete ret;
		delete bitmap;

		return NULL;
	}

	if( bitmap->GetLastStatus() == Gdiplus::Ok )
	{
		Gdiplus::BitmapData data;

		bitmap->LockBits(0, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);

		ret->stagingbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, data.Width * data.Height * 4, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_ASSERT(ret->stagingbuffer);

		void* memdata = ret->stagingbuffer->MapContents(0, 0);
		memcpy(memdata, data.Scan0, data.Width * data.Height * 4);

		ret->stagingbuffer->UnmapContents();
	}

	delete bitmap;

	viewcreateinfo.sType							= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewcreateinfo.pNext							= NULL;
	viewcreateinfo.components.r						= VK_COMPONENT_SWIZZLE_R;
	viewcreateinfo.components.g						= VK_COMPONENT_SWIZZLE_G;
	viewcreateinfo.components.b						= VK_COMPONENT_SWIZZLE_B;
	viewcreateinfo.components.a						= VK_COMPONENT_SWIZZLE_A;
	viewcreateinfo.format							= imagecreateinfo.format;
	viewcreateinfo.image							= ret->image;
	viewcreateinfo.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
	viewcreateinfo.subresourceRange.baseArrayLayer	= 0;
	viewcreateinfo.subresourceRange.baseMipLevel	= 0;
	viewcreateinfo.subresourceRange.layerCount		= 1;
	viewcreateinfo.subresourceRange.levelCount		= imagecreateinfo.mipLevels;
	viewcreateinfo.viewType							= VK_IMAGE_VIEW_TYPE_2D;
	viewcreateinfo.flags							= 0;

	res = vkCreateImageView(driverinfo.device, &viewcreateinfo, 0, &ret->imageview);

	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	samplercreateinfo.sType				= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplercreateinfo.magFilter			= VK_FILTER_LINEAR;
	samplercreateinfo.minFilter			= VK_FILTER_LINEAR;
	samplercreateinfo.mipmapMode		= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplercreateinfo.addressModeU		= VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplercreateinfo.addressModeV		= VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplercreateinfo.addressModeW		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplercreateinfo.mipLodBias		= 0.0;
	samplercreateinfo.anisotropyEnable	= VK_FALSE,
	samplercreateinfo.maxAnisotropy		= 0;
	samplercreateinfo.compareOp			= VK_COMPARE_OP_NEVER;
	samplercreateinfo.minLod			= 0.0f;
	samplercreateinfo.maxLod			= (float)imagecreateinfo.mipLevels;
	samplercreateinfo.compareEnable		= VK_FALSE;
	samplercreateinfo.borderColor		= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	res = vkCreateSampler(driverinfo.device, &samplercreateinfo, NULL, &ret->sampler);
	
	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	ret->imageinfo.sampler		= ret->sampler;
	ret->imageinfo.imageView	= ret->imageview;
	ret->imageinfo.imageLayout	= imagecreateinfo.initialLayout;

	printf("Loaded %s\n", file);
	VulkanContentManager().RegisterImage(file, ret);

	return ret;
}

VulkanImage* VulkanImage::CreateFromDDSCubemap(const char* file, bool srgb)
{
	VulkanImage* ret = VulkanContentManager().PointerImage(file);

	if( ret ) {
		printf("Pointer %s\n", file);
		return ret;
	}

	DDS_Image_Info			info;
	VkImageCreateInfo		imagecreateinfo		= {};
	VkImageViewCreateInfo	viewcreateinfo		= {};
	VkSamplerCreateInfo		samplercreateinfo	= {};
	VkFormatProperties		formatprops;
	VkResult				res;

	if( !LoadFromDDS(file, &info) )
	{
		std::cout << "Error: Could not load cube texture!";
		return 0;
	}

	VK_ASSERT(info.Data);

	ret = new VulkanImage();
	vkGetPhysicalDeviceFormatProperties(driverinfo.gpus[0], VK_FORMAT_B8G8R8A8_UNORM, &formatprops);

	imagecreateinfo.sType					= VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imagecreateinfo.pNext					= NULL;
	imagecreateinfo.arrayLayers				= 6;
	imagecreateinfo.extent.width			= info.Width;
	imagecreateinfo.extent.height			= info.Height;
	imagecreateinfo.extent.depth			= 1;
	imagecreateinfo.format					= (srgb ? VK_FORMAT_B8G8R8A8_SRGB : (VkFormat)info.Format);
	imagecreateinfo.imageType				= VK_IMAGE_TYPE_2D;
	imagecreateinfo.initialLayout			= VK_IMAGE_LAYOUT_UNDEFINED;
	imagecreateinfo.tiling					= VK_IMAGE_TILING_OPTIMAL;
	imagecreateinfo.queueFamilyIndexCount	= 0;
	imagecreateinfo.pQueueFamilyIndices		= NULL;
	imagecreateinfo.samples					= VK_SAMPLE_COUNT_1_BIT;
	imagecreateinfo.sharingMode				= VK_SHARING_MODE_EXCLUSIVE;
	imagecreateinfo.usage					= VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
	imagecreateinfo.flags					= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VulkanImage::CalculateImageSizeAndMipmapCount(imagecreateinfo.mipLevels, imagecreateinfo.format, imagecreateinfo.extent.width, imagecreateinfo.extent.height);

	ret->extents = imagecreateinfo.extent;
	ret->mipmapcount = imagecreateinfo.mipLevels;
	ret->cubemap = true;

	res = vkCreateImage(driverinfo.device, &imagecreateinfo, 0, &ret->image);

	if( res != VK_SUCCESS ) {
		delete ret;
		free(info.Data);

		return NULL;
	}

	vkGetImageMemoryRequirements(driverinfo.device, ret->image, &ret->memreqs);
	ret->memory = VulkanMemoryManager().AllocateForImage(ret->memreqs, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, imagecreateinfo.tiling);

	if( !ret->memory ) {
		delete ret;
		free(info.Data);

		return NULL;
	}

	res = vkBindImageMemory(driverinfo.device, ret->image, ret->memory.memory, ret->memory.offset);
	
	if( res != VK_SUCCESS ) {
		delete ret;
		free(info.Data);

		return NULL;
	}

	size_t slicesize = CalculateSliceSize((VkFormat)info.Format, info.Width, info.Height);

	ret->stagingbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 6 * slicesize, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	VK_ASSERT(ret->stagingbuffer);

	void* memdata = ret->stagingbuffer->MapContents(0, 0);
	memcpy(memdata, info.Data, 6 * slicesize);

	ret->stagingbuffer->UnmapContents();
	free(info.Data);

	viewcreateinfo.sType							= VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewcreateinfo.pNext							= NULL;
	viewcreateinfo.components.r						= VK_COMPONENT_SWIZZLE_R;
	viewcreateinfo.components.g						= VK_COMPONENT_SWIZZLE_G;
	viewcreateinfo.components.b						= VK_COMPONENT_SWIZZLE_B;
	viewcreateinfo.components.a						= VK_COMPONENT_SWIZZLE_A;
	viewcreateinfo.format							= imagecreateinfo.format;
	viewcreateinfo.image							= ret->image;
	viewcreateinfo.subresourceRange.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
	viewcreateinfo.subresourceRange.baseArrayLayer	= 0;
	viewcreateinfo.subresourceRange.baseMipLevel	= 0;
	viewcreateinfo.subresourceRange.layerCount		= 6;
	viewcreateinfo.subresourceRange.levelCount		= imagecreateinfo.mipLevels;
	viewcreateinfo.viewType							= VK_IMAGE_VIEW_TYPE_CUBE;
	viewcreateinfo.flags							= 0;

	res = vkCreateImageView(driverinfo.device, &viewcreateinfo, 0, &ret->imageview);

	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	samplercreateinfo.sType				= VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplercreateinfo.magFilter			= VK_FILTER_LINEAR;
	samplercreateinfo.minFilter			= VK_FILTER_LINEAR;
	samplercreateinfo.mipmapMode		= VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplercreateinfo.addressModeU		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplercreateinfo.addressModeV		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplercreateinfo.addressModeW		= VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplercreateinfo.mipLodBias		= 0.0;
	samplercreateinfo.anisotropyEnable	= VK_FALSE,
	samplercreateinfo.maxAnisotropy		= 0;
	samplercreateinfo.compareOp			= VK_COMPARE_OP_NEVER;
	samplercreateinfo.minLod			= 0.0f;
	samplercreateinfo.maxLod			= (float)imagecreateinfo.mipLevels;
	samplercreateinfo.compareEnable		= VK_FALSE;
	samplercreateinfo.borderColor		= VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	res = vkCreateSampler(driverinfo.device, &samplercreateinfo, NULL, &ret->sampler);
	
	if( res != VK_SUCCESS ) {
		delete ret;
		return NULL;
	}

	ret->imageinfo.sampler		= ret->sampler;
	ret->imageinfo.imageView	= ret->imageview;
	ret->imageinfo.imageLayout	= imagecreateinfo.initialLayout;

	printf("Loaded %s\n", file);
	VulkanContentManager().RegisterImage(file, ret);

	return ret;
}

size_t VulkanImage::CalculateImageSizeAndMipmapCount(uint32_t& nummipsout, VkFormat format, uint32_t width, uint32_t height)
{
	nummipsout = VKMax<uint32_t>(1, (uint32_t)floor(log(VKMax<double>(width, height)) / 0.69314718055994530941723212));

	size_t w		= width;
	size_t h		= height;
	size_t bytesize	= 0;
	size_t bytes	= CalculateByteSize(format);

	for( uint32_t i = 0; i < nummipsout; ++i )
	{
		bytesize += VKMax<size_t>(1, w) * VKMax<size_t>(1, h) * bytes;

		w = VKMax<size_t>(w / 2, 1);
		h = VKMax<size_t>(h / 2, 1);
	}

	return bytesize;
}

size_t VulkanImage::CalculateSliceSize(VkFormat format, uint32_t width, uint32_t height)
{
	return width * height * CalculateByteSize(format);
}

size_t VulkanImage::CalculateByteSize(VkFormat format)
{
	size_t bytes = 0;

	// TODO: all formats
	switch( format )
	{
	case VK_FORMAT_R8G8B8A8_UNORM:
	case VK_FORMAT_R8G8B8A8_SRGB:
	case VK_FORMAT_B8G8R8A8_UNORM:
	case VK_FORMAT_B8G8R8A8_SRGB:
		bytes = 4;
		break;

	case VK_FORMAT_R16G16B16A16_SFLOAT:
		bytes = 8;
		break;

	default:
		VK_ASSERT(false);
		break;
	}

	return bytes;
}

void* VulkanImage::MapContents(VkDeviceSize offset, VkDeviceSize size, VkMemoryMapFlags flags)
{
	if( !stagingbuffer ) {
		stagingbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, memreqs.size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
		VK_ASSERT(stagingbuffer);
	}

	return stagingbuffer->MapContents(offset, size, flags);
}

void VulkanImage::StoreLayout(VkAccessFlags newaccess, VkImageLayout newlayout)
{
	access = newaccess;
	imageinfo.imageLayout = newlayout;
}

void VulkanImage::UnmapContents()
{
	VK_ASSERT(stagingbuffer);
	stagingbuffer->UnmapContents();
}

void VulkanImage::UploadToVRAM(VkCommandBuffer commandbuffer, bool generatemips)
{
	VK_ASSERT(stagingbuffer != 0);

	VulkanPipelineBarrierBatch barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);
	VkBufferImageCopy region;

	region.bufferOffset			= 0;
	region.bufferRowLength		= 0;
	region.bufferImageHeight	= 0;

	region.imageOffset.x		= 0;
	region.imageOffset.y		= 0;
	region.imageOffset.z		= 0;
	region.imageExtent			= extents;

	region.imageSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.baseArrayLayer	= 0;
	region.imageSubresource.layerCount		= (cubemap ? 6 : 1);
	region.imageSubresource.mipLevel		= 0;

	// NOTE: this is not really good here...
	barrier.ImageLayoutTransfer(this, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	barrier.Enlist(commandbuffer);

	this->StoreLayout(VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	vkCmdCopyBufferToImage(commandbuffer, stagingbuffer->GetBuffer(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	if( generatemips ) {
		VkImageBlit blit;
		uint32_t width, height;

		blit.srcOffsets[0].x = blit.srcOffsets[0].y = blit.srcOffsets[0].z = 0;
		blit.srcOffsets[1].x = extents.width;
		blit.srcOffsets[1].y = extents.height;
		blit.srcOffsets[1].z = extents.depth;

		blit.srcSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.baseArrayLayer	= 0;
		blit.srcSubresource.layerCount		= (cubemap ? 6 : 1);
		blit.srcSubresource.mipLevel		= 0;

		blit.dstOffsets[0].x = blit.dstOffsets[0].y = blit.dstOffsets[0].z = 0;

		blit.dstSubresource.aspectMask		= VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.baseArrayLayer	= 0;
		blit.dstSubresource.layerCount		= (cubemap ? 6 : 1);

		for( uint32_t i = 1; i < mipmapcount; ++i ) {
			width = std::max<uint32_t>(1, extents.width >> i);
			height = std::max<uint32_t>(1, extents.height >> i);

			blit.dstOffsets[1].x = width;
			blit.dstOffsets[1].y = height;
			blit.dstOffsets[1].z = 1;
			blit.dstSubresource.mipLevel = i;

			vkCmdBlitImage(commandbuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
		}
	}
}

void VulkanImage::DeleteStagingBuffer()
{
	if( stagingbuffer )
		delete stagingbuffer;

	stagingbuffer = 0;
}

//*************************************************************************************************************
//
// VulkanRenderPass impl
//
//*************************************************************************************************************

VulkanRenderPass::VulkanRenderPass(uint32_t width, uint32_t height)
{
	renderpass	= 0;
	framebuffer	= 0;
	framewidth	= width;
	frameheight	= height;
	clearcolors	= 0;

	attachmentdescs.reserve(6);
	attachmentimages.reserve(6);
}

VulkanRenderPass::~VulkanRenderPass()
{
	if( clearcolors )
		delete[] clearcolors;

	if( framebuffer )
		vkDestroyFramebuffer(driverinfo.device, framebuffer, NULL);

	if( renderpass )
		vkDestroyRenderPass(driverinfo.device, renderpass, NULL);
}

void VulkanRenderPass::AttachImage(VulkanImage* image, VkAttachmentLoadOp loadop, VkAttachmentStoreOp storeop, VkImageLayout initiallayout, VkImageLayout finallayout)
{
	VK_ASSERT(image != 0);

	VkAttachmentDescription desc;

	desc.format			= image->GetFormat();	// GetViewFormat() !!!
	desc.samples		= VK_SAMPLE_COUNT_1_BIT;
	desc.loadOp			= loadop;
	desc.storeOp		= storeop;
	desc.stencilLoadOp	= VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	desc.stencilStoreOp	= VK_ATTACHMENT_STORE_OP_DONT_CARE;
	desc.initialLayout	= initiallayout;
	desc.finalLayout	= finallayout;
	desc.flags			= 0;

	attachmentdescs.push_back(desc);
	attachmentimages.push_back(image);
}

void VulkanRenderPass::Begin(VkCommandBuffer commandbuffer, VkSubpassContents contents, const VulkanColor& clearcolor, float cleardepth, uint8_t clearstencil)
{
	VkRenderPassBeginInfo	passbegininfo	= {};

	for( size_t i = 0; i < attachmentdescs.size(); ++i ) {
		bool isdepth = (
			attachmentdescs[i].initialLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL ||
			attachmentdescs[i].initialLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL);

		if( isdepth ) {
			clearcolors[i].depthStencil.depth = cleardepth;
			clearcolors[i].depthStencil.stencil = clearstencil;
		} else {
			clearcolors[i].color.float32[0] = clearcolor.r;
			clearcolors[i].color.float32[1] = clearcolor.g;
			clearcolors[i].color.float32[2] = clearcolor.b;
			clearcolors[i].color.float32[3] = clearcolor.a;
		}
	}

	passbegininfo.sType						= VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	passbegininfo.pNext						= NULL;
	passbegininfo.renderPass				= renderpass;
	passbegininfo.framebuffer				= framebuffer;
	passbegininfo.renderArea.offset.x		= 0;
	passbegininfo.renderArea.offset.y		= 0;
	passbegininfo.renderArea.extent.width	= framewidth;
	passbegininfo.renderArea.extent.height	= frameheight;
	passbegininfo.clearValueCount			= (uint32_t)attachmentdescs.size();
	passbegininfo.pClearValues				= clearcolors;

	vkCmdBeginRenderPass(commandbuffer, &passbegininfo, contents);
}

void VulkanRenderPass::End(VkCommandBuffer commandbuffer)
{
	vkCmdEndRenderPass(commandbuffer);
}

void VulkanRenderPass::NextSubpass(VkSubpassContents contents)
{
	VK_ASSERT(false);
}

bool VulkanRenderPass::Assemble()
{
	VK_ASSERT(renderpass == 0);

	if( attachmentdescs.empty() )
		return false;

	VkRenderPassCreateInfo	renderpassinfo	= {};
	VkFramebufferCreateInfo	framebufferinfo	= {};
	VkSubpassDescription	subpass0		= {};
	VkAttachmentReference*	references		= new VkAttachmentReference[attachmentdescs.size()];
	VkResult				res;
	size_t					depthid			= SIZE_MAX;

	for( size_t i = 0; i < attachmentdescs.size(); ++i ) {
		references[i].attachment = (uint32_t)i;
		references[i].layout = attachmentdescs[i].initialLayout;

		// ez nem feltetlenul igaz...
		if( references[i].layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL || references[i].layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL ) {
			VK_ASSERT(depthid == SIZE_MAX);
			depthid = i;
		}
	}

	if( depthid < attachmentdescs.size() - 1 ) {
		std::swap(references[depthid], references[attachmentdescs.size() - 1]);
		depthid = attachmentdescs.size() - 1;
	}

	subpass0.pipelineBindPoint			= VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass0.flags						= 0;
	subpass0.inputAttachmentCount		= 0;
	subpass0.pInputAttachments			= NULL;
	subpass0.colorAttachmentCount		= (uint32_t)((depthid == SIZE_MAX) ? attachmentdescs.size() : (attachmentdescs.size() - 1));
	subpass0.pColorAttachments			= references;
	subpass0.pResolveAttachments		= NULL;
	subpass0.pDepthStencilAttachment	= ((depthid == SIZE_MAX) ? 0 : &references[depthid]);
	subpass0.preserveAttachmentCount	= 0;
	subpass0.pPreserveAttachments		= NULL;

	renderpassinfo.sType				= VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderpassinfo.pNext				= NULL;
	renderpassinfo.attachmentCount		= (uint32_t)attachmentdescs.size();
	renderpassinfo.pAttachments			= attachmentdescs.data();
	renderpassinfo.subpassCount			= 1;
	renderpassinfo.pSubpasses			= &subpass0;
	renderpassinfo.dependencyCount		= 0;
	renderpassinfo.pDependencies		= NULL;

	res = vkCreateRenderPass(driverinfo.device, &renderpassinfo, NULL, &renderpass);
	delete[] references;

	if( res != VK_SUCCESS )
		return false;

	VkImageView* fbattachments = new VkImageView[attachmentimages.size()];

	for( size_t i = 0; i < attachmentimages.size(); ++i )
		fbattachments[i] = attachmentimages[i]->GetImageView();

	framebufferinfo.sType				= VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferinfo.pNext				= NULL;
	framebufferinfo.renderPass			= renderpass;
	framebufferinfo.attachmentCount		= (uint32_t)attachmentdescs.size();
	framebufferinfo.pAttachments		= fbattachments;
	framebufferinfo.width				= framewidth;
	framebufferinfo.height				= frameheight;
	framebufferinfo.layers				= 1;

	res = vkCreateFramebuffer(driverinfo.device, &framebufferinfo, NULL, &framebuffer);
	delete[] fbattachments;

	if( res != VK_SUCCESS )
		return false;

	clearcolors = new VkClearValue[attachmentdescs.size()];
	return true;
}

//*************************************************************************************************************
//
// VulkanSpecializationInfo impl
//
//*************************************************************************************************************

VulkanSpecializationInfo::VulkanSpecializationInfo()
{
	specinfo.mapEntryCount = 0;
	specinfo.dataSize = 0;
	specinfo.pData = malloc(256);

	entries.reserve(16);
}

VulkanSpecializationInfo::VulkanSpecializationInfo(const VulkanSpecializationInfo& other)
{
	entries.reserve(16);

	specinfo = other.specinfo;
	entries = other.entries;

	specinfo.pData = malloc(256);
	memcpy((void*)specinfo.pData, other.specinfo.pData, specinfo.dataSize);
}

VulkanSpecializationInfo::~VulkanSpecializationInfo()
{
	free((void*)specinfo.pData);
}

void VulkanSpecializationInfo::AddInt(uint32_t constantID, int32_t value)
{
	VK_ASSERT(specinfo.dataSize + sizeof(value) < 256);

	VkSpecializationMapEntry entry;

	entry.constantID	= constantID;
	entry.offset		= (uint32_t)specinfo.dataSize;
	entry.size			= sizeof(value);

	*((int32_t*)((uint8_t*)specinfo.pData + entry.offset)) = value;
	specinfo.dataSize += entry.size;

	entries.push_back(entry);

	++specinfo.mapEntryCount;
	specinfo.pMapEntries = entries.data();
}

void VulkanSpecializationInfo::AddUInt(uint32_t constantID, uint32_t value)
{
	VK_ASSERT(specinfo.dataSize + sizeof(value) < 256);

	VkSpecializationMapEntry entry;

	entry.constantID	= constantID;
	entry.offset		= (uint32_t)specinfo.dataSize;
	entry.size			= sizeof(value);

	*((uint32_t*)((uint8_t*)specinfo.pData + entry.offset)) = value;
	specinfo.dataSize += entry.size;

	entries.push_back(entry);

	++specinfo.mapEntryCount;
	specinfo.pMapEntries = entries.data();
}

void VulkanSpecializationInfo::AddFloat(uint32_t constantID, float value)
{
	VK_ASSERT(specinfo.dataSize + sizeof(value) < 256);

	VkSpecializationMapEntry entry;

	entry.constantID	= constantID;
	entry.offset		= (uint32_t)specinfo.dataSize;
	entry.size			= sizeof(value);

	*((float*)((uint8_t*)specinfo.pData + entry.offset)) = value;
	specinfo.dataSize += entry.size;

	entries.push_back(entry);

	++specinfo.mapEntryCount;
	specinfo.pMapEntries = entries.data();
}

//*************************************************************************************************************
//
// VulkanBasePipeline impl
//
//*************************************************************************************************************

VulkanBasePipeline::BaseTemporaryData::BaseTemporaryData()
{
	shaderstages.reserve(5);
	pushconstantranges.reserve(2);
	specinfos.reserve(5);
}

VulkanBasePipeline::DescriptorSetGroup::DescriptorSetGroup()
{
	descriptorpool = 0;
	descsetlayout = 0;
}

void VulkanBasePipeline::DescriptorSetGroup::Reset()
{
	descriptorpool = 0;
	descsetlayout = 0;

	bindings.clear();
	descriptorsets.clear();
	descbufferinfos.clear();
	descimageinfos.clear();
}

VulkanBasePipeline::VulkanBasePipeline()
{
	pipeline				= 0;
	pipelinelayout			= 0;
	basetempdata			= 0;
}

VulkanBasePipeline::~VulkanBasePipeline()
{
	VK_SAFE_DELETE(basetempdata);

	for( size_t i = 0; i < shadermodules.size(); ++i) {
		vkDestroyShaderModule(driverinfo.device, shadermodules[i], 0);
	}

	for( size_t i = 0; i < descsetgroups.size(); ++i ) {
		vkDestroyDescriptorSetLayout(driverinfo.device, descsetgroups[i].descsetlayout, 0);
		//vkFreeDescriptorSets(driverinfo.device, descriptorpool, descriptorsets.size(), descriptorsets.data());

		if( descsetgroups[i].descriptorpool )
			vkDestroyDescriptorPool(driverinfo.device, descsetgroups[i].descriptorpool, 0);
	}

	if( pipeline )
		vkDestroyPipeline(driverinfo.device, pipeline, 0);

	if( pipelinelayout )
		vkDestroyPipelineLayout(driverinfo.device, pipelinelayout, 0);
}

bool VulkanBasePipeline::AddShader(VkShaderStageFlagBits type, const char* file, const VulkanSpecializationInfo& specinfo)
{
	VkPipelineShaderStageCreateInfo	shaderstageinfo = {};
	VkShaderModuleCreateInfo		modulecreateinfo = {};
	SPIRVByteCode					spirvcode;
	FILE*							infile = 0;
	std::string						ext;
	std::string						sourcefile(file);
	bool							isspirv = false;

	VKGetExtension(ext, file);
	isspirv = (ext[0] == 's' && ext[1] == 'p');

	if( isspirv ) {
		AssembleSPIRV(sourcefile, file);
	}

#ifdef _MSC_VER
	fopen_s(&infile, sourcefile.c_str(), "rb");
#else
	infile = fopen(sourcefile.c_str(), "rb")
#endif

	if( !infile )
		return false;

	fseek(infile, 0, SEEK_END);
	long filelength = ftell(infile);
	fseek(infile, 0, SEEK_SET);

	if( isspirv ) {
		VK_ASSERT((filelength % 4) == 0);
		spirvcode.resize(filelength / 4);

		fread(&spirvcode[0], 1, filelength, infile);
	} else {
		// GLSL
		EShLanguage			stage = FindLanguage(type);
		EShMessages			messages = (EShMessages)(EShMsgSpvRules|EShMsgVulkanRules);
		glslang::TShader	shader(stage);
		glslang::TProgram	program;

		char* source = (char*)malloc(filelength + 1);

		fread(source, 1, filelength, infile);
		source[filelength] = 0;

		shader.setStrings(&source, 1);

		if( !shader.parse(&SPIRVResources, 100, false, messages) ) {
			std::cout << shader.getInfoLog() << "\n";
			std::cout << shader.getInfoDebugLog() << std::endl;
		
			return false;
		}

		// translate
		program.addShader(&shader);

		if( !program.link(messages) ) {
			std::cout << program.getInfoLog() << "\n";
			std::cout << program.getInfoDebugLog() << std::endl;

			return false;
		}

		glslang::GlslangToSpv(*program.getIntermediate(stage), spirvcode);
		free(source);
	}

	fclose(infile);

	// create shader module
	shaderstageinfo.sType				= VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderstageinfo.pNext				= NULL;
	shaderstageinfo.pSpecializationInfo	= NULL;
	shaderstageinfo.flags				= 0;
	shaderstageinfo.stage				= type;
	shaderstageinfo.pName				= "main";

	modulecreateinfo.sType				= VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	modulecreateinfo.pNext				= NULL;
	modulecreateinfo.flags				= 0;
	modulecreateinfo.codeSize			= spirvcode.size() * sizeof(unsigned int);
	modulecreateinfo.pCode				= spirvcode.data();

	VkResult res = vkCreateShaderModule(driverinfo.device, &modulecreateinfo, NULL, &shaderstageinfo.module);

	if( res != VK_SUCCESS )
		return false;

	basetempdata->specinfos.push_back(specinfo);
	basetempdata->shaderstages.push_back(shaderstageinfo);
	shadermodules.push_back(shaderstageinfo.module);

	return true;
}

void VulkanBasePipeline::SetDescriptorSetLayoutBufferBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage)
{
	if( currentgroup.bindings.size() <= binding )
		currentgroup.bindings.resize(binding + 1);

	if( currentgroup.descbufferinfos.size() <= binding )
		currentgroup.descbufferinfos.resize(binding + 1);

	currentgroup.bindings[binding].binding				= binding;
	currentgroup.bindings[binding].descriptorCount		= 1;	// array size
	currentgroup.bindings[binding].descriptorType		= type;
	currentgroup.bindings[binding].pImmutableSamplers	= NULL;
	currentgroup.bindings[binding].stageFlags			= stage;
}

void VulkanBasePipeline::SetDescriptorSetLayoutImageBinding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage)
{
	if( currentgroup.bindings.size() <= binding )
		currentgroup.bindings.resize(binding + 1);

	if( currentgroup.descimageinfos.size() <= binding )
		currentgroup.descimageinfos.resize(binding + 1);

	currentgroup.bindings[binding].binding				= binding;
	currentgroup.bindings[binding].descriptorCount		= 1;	// array size
	currentgroup.bindings[binding].descriptorType		= type;
	currentgroup.bindings[binding].pImmutableSamplers	= NULL;
	currentgroup.bindings[binding].stageFlags			= stage;
}

void VulkanBasePipeline::AllocateDescriptorSets(uint32_t numsets)
{
	// set bindings first to define layout, then call this function
	VK_ASSERT(currentgroup.bindings.size() > 0);

	VkDescriptorSetLayoutCreateInfo	descsetlayoutinfo	= {};
	VkDescriptorPoolCreateInfo		descpoolinfo		= {};
	VkDescriptorSetAllocateInfo		descsetallocinfo	= {};
	VkResult						res;

	// descriptor set layout
	descsetlayoutinfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descsetlayoutinfo.pNext			= NULL;
	descsetlayoutinfo.bindingCount	= (uint32_t)currentgroup.bindings.size();
	descsetlayoutinfo.pBindings		= currentgroup.bindings.data();

	res = vkCreateDescriptorSetLayout(driverinfo.device, &descsetlayoutinfo, 0, &currentgroup.descsetlayout);
	VK_ASSERT(res == VK_SUCCESS);

	// calculate number of descriptors for each type
	uint32_t numdescriptors[VK_DESCRIPTOR_TYPE_RANGE_SIZE];
	uint32_t numnonemptytypes = 0;
	uint32_t typeswritten = 0;

	memset(numdescriptors, 0, sizeof(numdescriptors));

	for( size_t i = 0; i < currentgroup.bindings.size(); ++i ) {
		const VkDescriptorSetLayoutBinding& layoutbinding = currentgroup.bindings[i];
		numdescriptors[layoutbinding.descriptorType] += layoutbinding.descriptorCount;
	}

	for( size_t i = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i ) {
		if( numdescriptors[i] > 0 )
			++numnonemptytypes;
	}

	// create descriptor pool
	VkDescriptorPoolSize* poolsizes = new VkDescriptorPoolSize[numnonemptytypes];

	for( size_t i = VK_DESCRIPTOR_TYPE_BEGIN_RANGE; i < VK_DESCRIPTOR_TYPE_RANGE_SIZE; ++i ) {
		if( numdescriptors[i] > 0 ) {
			poolsizes[typeswritten].type = (VkDescriptorType)i;
			poolsizes[typeswritten].descriptorCount = numdescriptors[i] * numsets;

			++typeswritten;
		}
	}

	descpoolinfo.sType			= VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descpoolinfo.pNext			= NULL;
	descpoolinfo.maxSets		= numsets;
	descpoolinfo.poolSizeCount	= numnonemptytypes;
	descpoolinfo.pPoolSizes		= poolsizes;

	res = vkCreateDescriptorPool(driverinfo.device, &descpoolinfo, NULL, &currentgroup.descriptorpool);
	VK_ASSERT(res == VK_SUCCESS);

	delete[] poolsizes;

	// descriptor sets for this group
	VkDescriptorSetLayout* descsetlayouts = new VkDescriptorSetLayout[numsets];

	for( uint32_t i = 0; i < numsets; ++i )
		descsetlayouts[i] = currentgroup.descsetlayout;

	descsetallocinfo.sType				= VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descsetallocinfo.pNext				= NULL;
	descsetallocinfo.descriptorPool		= currentgroup.descriptorpool;
	descsetallocinfo.descriptorSetCount	= numsets;
	descsetallocinfo.pSetLayouts		= descsetlayouts;

	currentgroup.descriptorsets.resize(numsets);

	res = vkAllocateDescriptorSets(driverinfo.device, &descsetallocinfo, currentgroup.descriptorsets.data());
	VK_ASSERT(res == VK_SUCCESS);

	delete[] descsetlayouts;

	descsetgroups.push_back(currentgroup);
	currentgroup.Reset();
}

void VulkanBasePipeline::SetDescriptorSetGroupBufferInfo(uint32_t group, uint32_t binding, const VkDescriptorBufferInfo* info)
{
	VK_ASSERT(group < descsetgroups.size());
	VK_ASSERT(info);

	descsetgroups[group].descbufferinfos[binding] = *info;
}

void VulkanBasePipeline::SetDescriptorSetGroupImageInfo(uint32_t group, uint32_t binding, const VkDescriptorImageInfo* info)
{
	VK_ASSERT(group < descsetgroups.size());
	VK_ASSERT(info);

	descsetgroups[group].descimageinfos[binding] = *info;
}

void VulkanBasePipeline::UpdateDescriptorSet(uint32_t group, uint32_t set)
{
	DescriptorSetGroup& dsgroup = descsetgroups[group];
	VkWriteDescriptorSet* descsetwrites = new VkWriteDescriptorSet[dsgroup.bindings.size()];

	for( size_t i = 0; i < dsgroup.bindings.size(); ++i ) {
		VK_ASSERT(i == dsgroup.bindings[i].binding);

		descsetwrites[i].sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descsetwrites[i].pNext				= NULL;
		descsetwrites[i].dstSet				= dsgroup.descriptorsets[set];
		descsetwrites[i].descriptorCount	= dsgroup.bindings[i].descriptorCount;
		descsetwrites[i].descriptorType		= dsgroup.bindings[i].descriptorType;
		descsetwrites[i].dstArrayElement	= 0;
		descsetwrites[i].dstBinding			= dsgroup.bindings[i].binding;

		descsetwrites[i].pBufferInfo		= 0;
		descsetwrites[i].pImageInfo			= 0;
		descsetwrites[i].pTexelBufferView	= 0;

		if( descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
			descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC ||
			descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
			descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC)
		{
			VK_ASSERT(dsgroup.descbufferinfos[i].buffer != NULL);
			descsetwrites[i].pBufferInfo = &dsgroup.descbufferinfos[i];
		}

		if( descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLER ||
			descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
			descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE ||
			descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE )
		{
			VK_ASSERT(dsgroup.descimageinfos[i].imageView != NULL);
			VK_ASSERT(dsgroup.descimageinfos[i].imageLayout != VK_IMAGE_LAYOUT_UNDEFINED);

			descsetwrites[i].pImageInfo = &dsgroup.descimageinfos[i];
		}

		if( descsetwrites[i].descriptorType == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT )
		{
			VK_ASSERT(dsgroup.descimageinfos[i].imageView != NULL);
			descsetwrites[i].pImageInfo = &dsgroup.descimageinfos[i];
		}

		// TODO: other types
	}

	vkUpdateDescriptorSets(driverinfo.device, (uint32_t)dsgroup.bindings.size(), descsetwrites, 0, NULL);
	delete[] descsetwrites;
}

void VulkanBasePipeline::AddPushConstantRange(VkShaderStageFlags stages, uint32_t offset, uint32_t size)
{
	VK_ASSERT(offset % 4 == 0);
	VK_ASSERT(size % 4 == 0);

	VkPushConstantRange range;

	range.offset = offset;
	range.size = size;
	range.stageFlags = stages;

	basetempdata->pushconstantranges.push_back(range);
}

bool VulkanBasePipeline::Assemble()
{
	VkPipelineLayoutCreateInfo	layoutinfo = {};
	VkResult					res;

	// collect layouts
	VkDescriptorSetLayout* descsetlayouts = new VkDescriptorSetLayout[descsetgroups.size()];

	for( size_t i = 0; i < descsetgroups.size(); ++i )
		descsetlayouts[i] = descsetgroups[i].descsetlayout;

	// pipeline layout
	layoutinfo.sType					= VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutinfo.pNext					= NULL;
	layoutinfo.pushConstantRangeCount	= (uint32_t)basetempdata->pushconstantranges.size();
	layoutinfo.pPushConstantRanges		= basetempdata->pushconstantranges.data();
	layoutinfo.setLayoutCount			= (uint32_t)descsetgroups.size();
	layoutinfo.pSetLayouts				= descsetlayouts;

	res = vkCreatePipelineLayout(driverinfo.device, &layoutinfo, NULL, &pipelinelayout);
	delete[] descsetlayouts;

	return (res == VK_SUCCESS);
}

//*************************************************************************************************************
//
// VulkanGraphicsPipeline impl
//
//*************************************************************************************************************

VulkanGraphicsPipeline::GraphicsTemporaryData::GraphicsTemporaryData()
{
	attributes.reserve(8);

	// vertex input
	vertexinputstate.sType						= VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexinputstate.pNext						= NULL;
	vertexinputstate.flags						= 0;

	// input assembler
	inputassemblystate.sType					= VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputassemblystate.pNext					= NULL;
	inputassemblystate.flags					= 0;
	inputassemblystate.primitiveRestartEnable	= VK_FALSE;
	inputassemblystate.topology					= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// rasterizer state
	rasterizationstate.sType					= VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationstate.pNext					= NULL;
	rasterizationstate.flags					= 0;
	rasterizationstate.polygonMode				= VK_POLYGON_MODE_FILL;
	rasterizationstate.cullMode					= VK_CULL_MODE_BACK_BIT;
	rasterizationstate.frontFace				= VK_FRONT_FACE_CLOCKWISE;
	rasterizationstate.depthClampEnable			= VK_FALSE;
	rasterizationstate.rasterizerDiscardEnable	= VK_FALSE;
	rasterizationstate.depthBiasEnable			= VK_FALSE;
	rasterizationstate.depthBiasConstantFactor	= 0;
	rasterizationstate.depthBiasClamp			= 0;
	rasterizationstate.depthBiasSlopeFactor		= 0;
	rasterizationstate.lineWidth				= 1;

	// blend state
	blendstates.resize(1);

	blendstates[0].colorWriteMask				= 0xf;
	blendstates[0].blendEnable					= VK_FALSE;
	blendstates[0].alphaBlendOp					= VK_BLEND_OP_ADD;
	blendstates[0].colorBlendOp					= VK_BLEND_OP_ADD;
	blendstates[0].srcColorBlendFactor			= VK_BLEND_FACTOR_ZERO;
	blendstates[0].dstColorBlendFactor			= VK_BLEND_FACTOR_ZERO;
	blendstates[0].srcAlphaBlendFactor			= VK_BLEND_FACTOR_ZERO;
	blendstates[0].dstAlphaBlendFactor			= VK_BLEND_FACTOR_ZERO;

	colorblendstate.sType						= VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorblendstate.flags						= 0;
	colorblendstate.pNext						= NULL;
	colorblendstate.attachmentCount				= 1;
	colorblendstate.pAttachments				= blendstates.data();
	colorblendstate.logicOpEnable				= VK_FALSE;
	colorblendstate.logicOp						= VK_LOGIC_OP_NO_OP;
	colorblendstate.blendConstants[0]			= 1.0f;
	colorblendstate.blendConstants[1]			= 1.0f;
	colorblendstate.blendConstants[2]			= 1.0f;
	colorblendstate.blendConstants[3]			= 1.0f;

	// viewport state
	viewportstate.sType							= VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportstate.pNext							= NULL;
	viewportstate.flags							= 0;
	viewportstate.viewportCount					= 1;
	viewportstate.scissorCount					= 1;
	viewportstate.pScissors						= NULL;
	viewportstate.pViewports					= NULL;

	// depth-stencil state
	depthstencilstate.sType						= VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthstencilstate.pNext						= NULL;
	depthstencilstate.flags						= 0;
	depthstencilstate.depthTestEnable			= VK_FALSE;
	depthstencilstate.depthWriteEnable			= VK_FALSE;
	depthstencilstate.depthCompareOp			= VK_COMPARE_OP_LESS_OR_EQUAL;
	depthstencilstate.depthBoundsTestEnable		= VK_FALSE;
	depthstencilstate.stencilTestEnable			= VK_FALSE;
	depthstencilstate.back.failOp				= VK_STENCIL_OP_KEEP;
	depthstencilstate.back.passOp				= VK_STENCIL_OP_KEEP;
	depthstencilstate.back.depthFailOp			= VK_STENCIL_OP_KEEP;
	depthstencilstate.back.compareOp			= VK_COMPARE_OP_ALWAYS;
	depthstencilstate.back.compareMask			= 0;
	depthstencilstate.back.reference			= 0;
	depthstencilstate.back.writeMask			= 0;
	depthstencilstate.minDepthBounds			= 0;
	depthstencilstate.maxDepthBounds			= 0;
	depthstencilstate.front						= depthstencilstate.back;

	// multisample state
	multisamplestate.sType						= VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisamplestate.pNext						= NULL;
	multisamplestate.flags						= 0;
	multisamplestate.pSampleMask				= NULL;
	multisamplestate.rasterizationSamples		= VK_SAMPLE_COUNT_1_BIT;
	multisamplestate.sampleShadingEnable		= VK_FALSE;
	multisamplestate.alphaToCoverageEnable		= VK_FALSE;
	multisamplestate.alphaToOneEnable			= VK_FALSE;
	multisamplestate.minSampleShading			= 0.0f;
}

VulkanGraphicsPipeline::VulkanGraphicsPipeline()
{
	tempdata				= new GraphicsTemporaryData();
	basetempdata			= tempdata;

	viewport.x				= 0;
	viewport.y				= 0;
	viewport.width			= 800;
	viewport.height			= 600;
	viewport.minDepth		= 0;
	viewport.maxDepth		= 1;

	scissor.extent.width	= 800;
	scissor.extent.height	= 600;
	scissor.offset.x		= 0;
	scissor.offset.y		= 0;

	tempdata->viewportstate.pScissors = &scissor;
	tempdata->viewportstate.pViewports = &viewport;
}

VulkanGraphicsPipeline::~VulkanGraphicsPipeline()
{
}

bool VulkanGraphicsPipeline::Assemble(VkRenderPass renderpass)
{
	VK_ASSERT(tempdata != NULL);
	VK_ASSERT(VulkanBasePipeline::Assemble());

	VkGraphicsPipelineCreateInfo	pipelineinfo		= {};
	VkDynamicState					dynstateenables[VK_DYNAMIC_STATE_RANGE_SIZE];
	VkResult						res;

	memset(dynstateenables, 0, sizeof(dynstateenables));
	tempdata->dynamicstate.dynamicStateCount	= 0;

	dynstateenables[0] = VK_DYNAMIC_STATE_VIEWPORT;
	++tempdata->dynamicstate.dynamicStateCount;

	dynstateenables[1] = VK_DYNAMIC_STATE_SCISSOR;
	++tempdata->dynamicstate.dynamicStateCount;

	tempdata->dynamicstate.sType			= VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	tempdata->dynamicstate.pNext			= NULL;
	tempdata->dynamicstate.pDynamicStates	= dynstateenables;
	tempdata->dynamicstate.flags			= 0;

	// input state
	tempdata->vertexinputstate.vertexBindingDescriptionCount	= (uint32_t)tempdata->vertexbindings.size();
	tempdata->vertexinputstate.pVertexBindingDescriptions		= tempdata->vertexbindings.data();
	tempdata->vertexinputstate.vertexAttributeDescriptionCount	= (uint32_t)tempdata->attributes.size();
	tempdata->vertexinputstate.pVertexAttributeDescriptions		= tempdata->attributes.data();

	for( size_t i = 0; i < tempdata->shaderstages.size(); ++i ) {
		if( tempdata->specinfos[i].GetSpecializationInfo()->dataSize > 0 )
			tempdata->shaderstages[i].pSpecializationInfo = tempdata->specinfos[i].GetSpecializationInfo();
	}

	// pipeline
	pipelineinfo.sType					= VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineinfo.pNext					= NULL;
	pipelineinfo.layout					= pipelinelayout;
	pipelineinfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineinfo.basePipelineIndex		= 0;
	pipelineinfo.flags					= 0;
	pipelineinfo.pVertexInputState		= &tempdata->vertexinputstate;
	pipelineinfo.pInputAssemblyState	= &tempdata->inputassemblystate;
	pipelineinfo.pRasterizationState	= &tempdata->rasterizationstate;
	pipelineinfo.pColorBlendState		= &tempdata->colorblendstate;
	pipelineinfo.pTessellationState		= NULL;
	pipelineinfo.pMultisampleState		= &tempdata->multisamplestate;
	pipelineinfo.pDynamicState			= &tempdata->dynamicstate;
	pipelineinfo.pViewportState			= &tempdata->viewportstate;
	pipelineinfo.pDepthStencilState		= &tempdata->depthstencilstate;
	pipelineinfo.pStages				= tempdata->shaderstages.data();
	pipelineinfo.stageCount				= (uint32_t)tempdata->shaderstages.size();
	pipelineinfo.renderPass				= renderpass;
	pipelineinfo.subpass				= 0;

	res = vkCreateGraphicsPipelines(driverinfo.device, VK_NULL_HANDLE, 1, &pipelineinfo, NULL, &pipeline);
	delete tempdata;

	tempdata = 0;
	basetempdata = 0;

	return (res == VK_SUCCESS);
}

void VulkanGraphicsPipeline::SetInputAttribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset)
{
	if( tempdata->attributes.size() <= location )
		tempdata->attributes.resize(location + 1);

	VkVertexInputAttributeDescription& attribdesc = tempdata->attributes[location];

	attribdesc.location	= location;
	attribdesc.binding	= binding;
	attribdesc.format	= format;
	attribdesc.offset	= offset;
}

void VulkanGraphicsPipeline::SetVertexInputBinding(uint32_t location, const VkVertexInputBindingDescription& desc)
{
	VkVertexInputBindingDescription defvalue;

	defvalue.binding	= 0;
	defvalue.inputRate	= VK_VERTEX_INPUT_RATE_VERTEX;
	defvalue.stride		= 0;

	if( tempdata->vertexbindings.size() <= location )
		tempdata->vertexbindings.resize(location + 1, defvalue);

	tempdata->vertexbindings[location] = desc;
}

void VulkanGraphicsPipeline::SetInputAssembler(VkPrimitiveTopology topology, VkBool32 primitiverestart)
{
	tempdata->inputassemblystate.topology = topology;
	tempdata->inputassemblystate.primitiveRestartEnable = primitiverestart;
}

void VulkanGraphicsPipeline::SetRasterizer(VkPolygonMode fillmode, VkCullModeFlags cullmode)
{
	tempdata->rasterizationstate.polygonMode = fillmode;
	tempdata->rasterizationstate.cullMode = cullmode;
}

void VulkanGraphicsPipeline::SetDepth(VkBool32 depthenable, VkBool32 depthwriteenable, VkCompareOp compareop)
{
	tempdata->depthstencilstate.depthTestEnable = depthenable;
	tempdata->depthstencilstate.depthWriteEnable = depthwriteenable;
	tempdata->depthstencilstate.depthCompareOp = compareop;
}

void VulkanGraphicsPipeline::SetBlendState(uint32_t attachment, VkBool32 enable, VkBlendOp colorop, VkBlendFactor srcblend, VkBlendFactor destblend)
{
	if( attachment >= tempdata->blendstates.size() )
		tempdata->blendstates.resize(attachment + 1);

	VkPipelineColorBlendAttachmentState& blendstate = tempdata->blendstates[attachment];

	blendstate.colorWriteMask		= 0xf;
	blendstate.blendEnable			= enable;
	blendstate.alphaBlendOp			= VK_BLEND_OP_ADD;
	blendstate.colorBlendOp			= colorop;
	blendstate.srcColorBlendFactor	= srcblend;
	blendstate.dstColorBlendFactor	= destblend;
	blendstate.srcAlphaBlendFactor	= VK_BLEND_FACTOR_ZERO;
	blendstate.dstAlphaBlendFactor	= VK_BLEND_FACTOR_ZERO;

	tempdata->colorblendstate.attachmentCount = (uint32_t)tempdata->blendstates.size();
	tempdata->colorblendstate.pAttachments = tempdata->blendstates.data();
}

void VulkanGraphicsPipeline::SetViewport(float x, float y, float width, float height, float minz, float maxz)
{
	viewport.x = x;
	viewport.y = y;
	viewport.width = width;
	viewport.height = height;
	viewport.minDepth = minz;
	viewport.maxDepth = maxz;
}

void VulkanGraphicsPipeline::SetScissor(int32_t x, int32_t y, uint32_t width, uint32_t height)
{
	scissor.extent.width = width;
	scissor.extent.height = height;
	scissor.offset.x = x;
	scissor.offset.y = y;
}

//*************************************************************************************************************
//
// VulkanComputePipeline impl
//
//*************************************************************************************************************

VulkanComputePipeline::VulkanComputePipeline()
{
	basetempdata = new BaseTemporaryData();
}

VulkanComputePipeline::~VulkanComputePipeline()
{
}

bool VulkanComputePipeline::Assemble()
{
	BaseTemporaryData* tempdata = basetempdata;

	VK_ASSERT(tempdata != NULL);
	VK_ASSERT(tempdata->shaderstages.size() == 1);
	VK_ASSERT(tempdata->shaderstages[0].stage == VK_SHADER_STAGE_COMPUTE_BIT);

	VK_ASSERT(VulkanBasePipeline::Assemble());

	VkComputePipelineCreateInfo	pipelineinfo = {};
	VkResult					res;

	if( tempdata->specinfos[0].GetSpecializationInfo()->dataSize > 0 )
		tempdata->shaderstages[0].pSpecializationInfo = tempdata->specinfos[0].GetSpecializationInfo();

	// pipeline
	pipelineinfo.sType					= VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	pipelineinfo.pNext					= NULL;
	pipelineinfo.layout					= pipelinelayout;
	pipelineinfo.basePipelineHandle		= VK_NULL_HANDLE;
	pipelineinfo.basePipelineIndex		= 0;
	pipelineinfo.flags					= 0;
	pipelineinfo.stage					= tempdata->shaderstages[0];

	res = vkCreateComputePipelines(driverinfo.device, VK_NULL_HANDLE, 1, &pipelineinfo, NULL, &pipeline);
	delete tempdata;

	tempdata = 0;
	basetempdata = 0;

	return (res == VK_SUCCESS);
}

//*************************************************************************************************************
//
// VulkanBasicMesh impl
//
//*************************************************************************************************************

static void AccumulateTangentFrame(VulkanTBNVertex* vdata, uint32_t i1, uint32_t i2, uint32_t i3)
{
	VulkanTBNVertex* v1 = (vdata + i1);
	VulkanTBNVertex* v2 = (vdata + i2);
	VulkanTBNVertex* v3 = (vdata + i3);

	float ax = v2->x - v1->x;
	float ay = v2->y - v1->y;
	float az = v2->z - v1->z;

	float cx = v3->x - v1->x;
	float cy = v3->y - v1->y;
	float cz = v3->z - v1->z;

	float s1 = v2->u - v1->u;
	float s2 = v3->u - v1->u;
	float t1 = v2->v - v1->v;
	float t2 = v3->v - v1->v;

	float invdet = 1.0f / ((s1 * t2 - s2 * t1) + 0.0001f);

	float tx = (t2 * ax - t1 * cx) * invdet;
	float ty = (t2 * ay - t1 * cy) * invdet;
	float tz = (t2 * az - t1 * cz) * invdet;

	float bx = (s1 * cx - s2 * ax) * invdet;
	float by = (s1 * cy - s2 * ay) * invdet;
	float bz = (s1 * cz - s2 * az) * invdet;

	v1->tx += tx;	v2->tx += tx;	v3->tx += tx;
	v1->ty += ty;	v2->ty += ty;	v3->ty += ty;
	v1->tz += tz;	v2->tz += tz;	v3->tz += tz;

	v1->bx += bx;	v2->bx += bx;	v3->bx += bx;
	v1->by += by;	v2->by += by;	v3->by += by;
	v1->bz += bz;	v2->bz += bz;	v3->bz += bz;
}

static void OrthogonalizeTangentFrame(VulkanTBNVertex& vert)
{
	float t[3], b[3], q[3];

	//VKVec3Normalize(&vert.nx, &vert.nx);

	VKVec3Scale(t, &vert.nx, VKVec3Dot(&vert.nx, &vert.tx));
	VKVec3Subtract(t, &vert.tx, t);

	VKVec3Scale(q, t, (VKVec3Dot(t, &vert.bx) / VKVec3Dot(&vert.tx, &vert.tx)));
	VKVec3Scale(b, &vert.nx, VKVec3Dot(&vert.nx, &vert.bx));
	VKVec3Subtract(b, &vert.bx, b);
	VKVec3Subtract(b, b, q);

	VKVec3Normalize(&vert.tx, t);
	VKVec3Normalize(&vert.bx, b);
}

VulkanBasicMesh::VulkanBasicMesh(uint32_t numvertices, uint32_t numindices, uint32_t vertexstride, VulkanBuffer* buff, VkDeviceSize off)
{
	totalsize	= numvertices * vertexstride;
	descriptor	= 0;
	vertexcount	= numvertices;
	indexcount	= numindices;
	vstride		= vertexstride;
	baseoffset	= (buff ? off : 0);
	inherited	= (buff != 0);
	numsubsets	= 1;

	// TODO: if primitive reset disabled
	if( numvertices > 0xffff )
		indexformat = VK_INDEX_TYPE_UINT32;
	else
		indexformat = VK_INDEX_TYPE_UINT16;

	if( buff )
	{
		if( totalsize % 4 > 0 )
			totalsize += (4 - (totalsize % 4));

		indexoffset = baseoffset + totalsize;
		totalsize += numindices * (indexformat == VK_INDEX_TYPE_UINT16 ? 2 : 4);

		if( totalsize % 256 > 0 )
			totalsize += (256 - (totalsize % 256));

		uniformoffset = baseoffset + totalsize;
		totalsize += 512;	// 32 registers

		vertexbuffer = indexbuffer = unibuffer = buff;
		VK_ASSERT(baseoffset + totalsize <= buff->GetSize());

		mappedvdata = mappedidata = mappedudata = (uint8_t*)buff->MapContents(0, 0);
	}
	else
	{
		totalsize += numindices * (indexformat == VK_INDEX_TYPE_UINT16 ? 2 : 4);

		vertexbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, numvertices * vertexstride, VK_MEMORY_PROPERTY_SHARED_BIT);
		indexbuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, numindices * (indexformat == VK_INDEX_TYPE_UINT16 ? 2 : 4), VK_MEMORY_PROPERTY_SHARED_BIT);
		unibuffer = VulkanBuffer::Create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 512, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		indexoffset = 0;
		uniformoffset = 0;

		mappedvdata = (uint8_t*)vertexbuffer->MapContents(0, 0);
		mappedidata = (uint8_t*)indexbuffer->MapContents(0, 0);
		mappedudata = (uint8_t*)unibuffer->MapContents(0, 0);
	}
	
	VK_ASSERT(vertexbuffer);
	VK_ASSERT(indexbuffer);
	VK_ASSERT(unibuffer);

	unibufferinfo.buffer	= unibuffer->GetBuffer();
	unibufferinfo.offset	= uniformoffset;
	unibufferinfo.range		= 512;

	subsettable = new VulkanAttributeRange[1];
	materials = new VulkanMaterial[1];

	subsettable[0].AttribId			= 0;
	subsettable[0].IndexCount		= numindices;
	subsettable[0].IndexStart		= 0;
	subsettable[0].PrimitiveType	= VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	subsettable[0].VertexCount		= numvertices;
	subsettable[0].VertexStart		= 0;

	materials[0].Ambient			= VulkanColor(0, 0, 0, 1);
	materials[0].Diffuse			= VulkanColor(1, 1, 1, 1);
	materials[0].Specular			= VulkanColor(1, 1, 1, 1);
	materials[0].Emissive			= VulkanColor(0, 0, 0, 1);
	materials[0].Power				= 80;
	materials[0].Texture			= 0;
}

VulkanBasicMesh::~VulkanBasicMesh()
{
	if( subsettable )
		delete[] subsettable;

	if( materials )
		delete[] materials;

	if( !inherited )
	{
		if( vertexbuffer )
			delete vertexbuffer;

		if( indexbuffer )
			delete indexbuffer;

		if( unibuffer )
			delete unibuffer;
	}
}

void VulkanBasicMesh::Draw(VkCommandBuffer commandbuffer, VulkanGraphicsPipeline* pipeline, bool rebind)
{
	if( rebind )
	{
		VkBuffer vbuff = vertexbuffer->GetBuffer();

		vkCmdBindVertexBuffers(commandbuffer, 0, 1, &vbuff, &baseoffset);
		vkCmdBindIndexBuffer(commandbuffer, indexbuffer->GetBuffer(), indexoffset, indexformat);
	}

	for( uint32_t i = 0; i < numsubsets; ++i )
		DrawSubset(commandbuffer, i, pipeline, false);
}

void VulkanBasicMesh::DrawSubset(VkCommandBuffer commandbuffer, uint32_t index, VulkanGraphicsPipeline* pipeline, bool rebind)
{
	VK_ASSERT(index < numsubsets);

	const VulkanAttributeRange& subset = subsettable[index];

	if( rebind )
	{
		VkBuffer vbuff = vertexbuffer->GetBuffer();

		vkCmdBindVertexBuffers(commandbuffer, 0, 1, &vbuff, &baseoffset);
		vkCmdBindIndexBuffer(commandbuffer, indexbuffer->GetBuffer(), indexoffset, indexformat);
	}

	if( pipeline )
		vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, pipeline->GetDescriptorSets(0) + index, 0, NULL);

	vkCmdDrawIndexed(commandbuffer, subset.IndexCount, 1, subset.IndexStart, 0, 0);
}

void VulkanBasicMesh::DrawSubsetInstanced(VkCommandBuffer commandbuffer, uint32_t index, VulkanGraphicsPipeline* pipeline, uint32_t numinstances, bool rebind)
{
	VK_ASSERT(index < numsubsets);

	const VulkanAttributeRange& subset = subsettable[index];

	if( rebind )
	{
		VkBuffer vbuff = vertexbuffer->GetBuffer();

		vkCmdBindVertexBuffers(commandbuffer, 0, 1, &vbuff, &baseoffset);
		vkCmdBindIndexBuffer(commandbuffer, indexbuffer->GetBuffer(), indexoffset, indexformat);
	}

	if( pipeline )
		vkCmdBindDescriptorSets(commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->GetPipelineLayout(), 0, 1, pipeline->GetDescriptorSets(0) + index, 0, NULL);

	vkCmdDrawIndexed(commandbuffer, subset.IndexCount, numinstances, subset.IndexStart, 0, 0);
}

void VulkanBasicMesh::UploadToVRAM(VkCommandBuffer commandbuffer)
{
	if( !inherited && (mappedvdata || mappedidata) ) {
		// unmap before use
		vertexbuffer->UnmapContents();

		if( indexbuffer != vertexbuffer )
			indexbuffer->UnmapContents();

		mappedvdata = mappedidata = 0;
	}

	if( inherited ) {
		vertexbuffer->UploadToVRAM(commandbuffer);
	} else {
		vertexbuffer->UploadToVRAM(commandbuffer);
		indexbuffer->UploadToVRAM(commandbuffer);
		//unibuffer->UploadToVRAM(commandbuffer);
	}

	for( uint32_t i = 0; i < numsubsets; ++i ) {
		if( materials[i].Texture )
			materials[i].Texture->UploadToVRAM(commandbuffer);

		if( materials[i].NormalMap )
			materials[i].NormalMap->UploadToVRAM(commandbuffer);
	}

	VulkanPipelineBarrierBatch	barrier(VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT);

	for( uint32_t i = 0; i < numsubsets; ++i ) {
		if( materials[i].Texture )
			barrier.ImageLayoutTransfer(materials[i].Texture->GetImage(), 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		if( materials[i].NormalMap )
			barrier.ImageLayoutTransfer(materials[i].NormalMap->GetImage(), 0, VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	}

	barrier.Enlist(commandbuffer);
}

void VulkanBasicMesh::DeleteStagingBuffers()
{
	if( inherited ) {
		vertexbuffer->DeleteStagingBuffer();
	} else {
		vertexbuffer->DeleteStagingBuffer();
		indexbuffer->DeleteStagingBuffer();
		//unibuffer->DeleteStagingBuffer();
	}

	for( uint32_t i = 0; i < numsubsets; ++i ) {
		if( materials[i].Texture ) {
			materials[i].Texture->DeleteStagingBuffer();
			materials[i].Texture->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}

		if( materials[i].NormalMap ) {
			materials[i].NormalMap->DeleteStagingBuffer();
			materials[i].NormalMap->StoreLayout(VK_ACCESS_SHADER_READ_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
	}
}

void VulkanBasicMesh::GenerateTangentFrame()
{
	VK_ASSERT(vstride == sizeof(VulkanCommonVertex));	// not generated yet
	VK_ASSERT(vertexbuffer != NULL);
	VK_ASSERT(!inherited);
	VK_ASSERT(indexformat == VK_INDEX_TYPE_UINT32);	// TODO:

	VulkanBuffer*		newbuffer	= VulkanBuffer::Create(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexcount * sizeof(VulkanTBNVertex), VK_MEMORY_PROPERTY_SHARED_BIT);
	VulkanCommonVertex*	oldvdata	= (VulkanCommonVertex*)vertexbuffer->MapContents(0, 0);
	VulkanTBNVertex*	newvdata	= (VulkanTBNVertex*)newbuffer->MapContents(0, 0);
	uint32_t*			idata		= (uint32_t*)indexbuffer->MapContents(0, 0);
	uint32_t			i1, i2, i3;

	for( uint32_t i = 0; i < numsubsets; ++i ) {
		const VulkanAttributeRange& subset = subsettable[i];
		VK_ASSERT(subset.IndexCount > 0);

		VulkanCommonVertex*	oldsubsetdata	= (oldvdata + subset.VertexStart);
		VulkanTBNVertex*	newsubsetdata	= (newvdata + subset.VertexStart);
		uint32_t*			subsetidata		= (idata + subset.IndexStart);

		// initialize new data
		for( uint32_t j = 0; j < subset.VertexCount; ++j ) {
			VulkanCommonVertex& oldvert = oldsubsetdata[j];
			VulkanTBNVertex& newvert = newsubsetdata[j];

			VKVec3Assign(&newvert.x, &oldvert.x);
			VKVec3Assign(&newvert.nx, &oldvert.nx);
			
			newvert.u = oldvert.u;
			newvert.v = oldvert.v;

			VKVec3Set(&newvert.tx, 0, 0, 0);
			VKVec3Set(&newvert.bx, 0, 0, 0);
		}

		for( uint32_t j = 0; j < subset.IndexCount; j += 3 ) {
			i1 = *(subsetidata + j + 0) - subset.VertexStart;
			i2 = *(subsetidata + j + 1) - subset.VertexStart;
			i3 = *(subsetidata + j + 2) - subset.VertexStart;

			AccumulateTangentFrame(newsubsetdata, i1, i2, i3);
		}

		for( uint32_t j = 0; j < subset.VertexCount; ++j ) {
			OrthogonalizeTangentFrame(newsubsetdata[j]);
		}
	}

	indexbuffer->UnmapContents();
	newbuffer->UnmapContents();
	vertexbuffer->UnmapContents();

	delete vertexbuffer;

	vertexbuffer = newbuffer;
	vstride = sizeof(VulkanTBNVertex);
}

void* VulkanBasicMesh::GetVertexBufferPointer()
{
	return mappedvdata + baseoffset;
}

void* VulkanBasicMesh::GetIndexBufferPointer()
{
	return mappedidata + indexoffset;
}

void* VulkanBasicMesh::GetUniformBufferPointer()
{
	return mappedudata + uniformoffset;
}

VulkanBasicMesh* VulkanBasicMesh::LoadFromQM(const char* file, VulkanBuffer* buffer, VkDeviceSize offset)
{
	static const unsigned short elemsizes[6] = {
		1,	// float
		2,	// float2
		3,	// float3
		4,	// float4
		4,	// color
		4	// ubyte4
	};

	static const unsigned short elemstrides[6] = {
		4,	// float
		4,	// float2
		4,	// float3
		4,	// float4
		1,	// color
		1	// ubyte4
	};

	static VkFormat elemformats[6] = {
		VK_FORMAT_R32_SFLOAT,
		VK_FORMAT_R32G32_SFLOAT,
		VK_FORMAT_R32G32B32_SFLOAT,
		VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_FORMAT_R8G8B8A8_UINT
	};

	VkVertexInputAttributeDescription*	vertexlayout	= 0;
	VulkanBasicMesh*					mesh			= 0;
	FILE*								infile			= 0;
	void*								data			= 0;
	
	std::string							basedir(file), str;
	float								bbmin[3];
	float								bbmax[3];
	char								buff[256];

	uint32_t							unused;
	uint32_t							version;
	uint32_t							numindices;
	uint32_t							numvertices;
	uint32_t							vstride;
	uint32_t							istride;
	uint32_t							numsubsets;
	uint32_t							numelems;
	uint16_t							tmp16;
	uint8_t								tmp8;
	uint8_t								elemtype;

#ifdef _MSC_VER
	fopen_s(&infile, file, "rb");
#else
	infile = fopen(file, "rb");
#endif

	if( !infile )
		return false;

	basedir = basedir.substr(0, basedir.find_last_of('/') + 1);

	fread(&unused, 4, 1, infile);
	fread(&numindices, 4, 1, infile);
	fread(&istride, 4, 1, infile);
	fread(&numsubsets, 4, 1, infile);

	version = unused >> 16;

	fread(&numvertices, 4, 1, infile);
	fread(&unused, 4, 1, infile);
	fread(&unused, 4, 1, infile);
	fread(&unused, 4, 1, infile);

	// vertex declaration
	fread(&numelems, 4, 1, infile);

	vstride = 0;
	vertexlayout = new VkVertexInputAttributeDescription[numelems];

	for( uint32_t i = 0; i < numelems; ++i )
	{
		vertexlayout[i].location = i;

		fread(&tmp16, 2, 1, infile);	// binding
		vertexlayout[i].binding = tmp16;

		fread(&tmp8, 1, 1, infile);		// usage
		fread(&elemtype, 1, 1, infile);	// type
		fread(&tmp8, 1, 1, infile);		// usageindex

		vertexlayout[i].offset = vstride;
		vertexlayout[i].format = elemformats[elemtype];

		vstride += elemsizes[elemtype] * elemstrides[elemtype];
	}

	mesh = new VulkanBasicMesh(numvertices, numindices, vstride, buffer, offset);
	
	delete[] mesh->materials;
	delete[] mesh->subsettable;

	mesh->numsubsets = numsubsets;
	mesh->subsettable = new VulkanAttributeRange[numsubsets];
	mesh->materials = new VulkanMaterial[numsubsets];

	// data
	data = mesh->GetVertexBufferPointer();
	fread(data, vstride, numvertices, infile);

	data = mesh->GetIndexBufferPointer();
	fread(data, istride, numindices, infile);

	if( version >= 1 ) {
		fread(&unused, 4, 1, infile);

		if( unused > 0 )
			fseek(infile, 8 * unused, SEEK_CUR);
	}

	printf("Loaded %s (%u verts, %u tris)\n", file, numvertices, numindices / 3);

	for( uint32_t i = 0; i < numsubsets; ++i )
	{
		VulkanAttributeRange& subset = mesh->subsettable[i];
		VulkanMaterial& material = mesh->materials[i];

		subset.AttribId = i;
		subset.PrimitiveType = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		fread(&subset.IndexStart, 4, 1, infile);
		fread(&subset.VertexStart, 4, 1, infile);
		fread(&subset.VertexCount, 4, 1, infile);
		fread(&subset.IndexCount, 4, 1, infile);

		fread(bbmin, sizeof(float), 3, infile);
		fread(bbmax, sizeof(float), 3, infile);

		mesh->boundingbox.Add(bbmin);
		mesh->boundingbox.Add(bbmax);

		// subset & material info
		ReadString(infile, buff);
		ReadString(infile, buff);

		if( buff[1] != ',' )
		{
			fread(&material.Ambient, sizeof(VulkanColor), 1, infile);
			fread(&material.Diffuse, sizeof(VulkanColor), 1, infile);
			fread(&material.Specular, sizeof(VulkanColor), 1, infile);
			fread(&material.Emissive, sizeof(VulkanColor), 1, infile);

			if( version >= 2 )
				fseek(infile, 16, SEEK_CUR);	// uvscale

			fread(&material.Power, sizeof(float), 1, infile);
			fread(&material.Diffuse.a, sizeof(float), 1, infile);
			fread(&unused, 4, 1, infile);	// blendmode

			ReadString(infile, buff);

			if( buff[1] != ',' )
			{
				str = basedir + buff;
				material.Texture = VulkanImage::CreateFromFile(str.c_str(), true);
			}

			ReadString(infile, buff);

			if( buff[1] != ',' )
			{
				str = basedir + buff;
				material.NormalMap = VulkanImage::CreateFromFile(str.c_str(), true);
			}

			ReadString(infile, buff);
			ReadString(infile, buff);
			ReadString(infile, buff);
			ReadString(infile, buff);
			ReadString(infile, buff);
			ReadString(infile, buff);
		}
		else
		{
			material.Ambient	= VulkanColor(0, 0, 0, 1);
			material.Diffuse	= VulkanColor(1, 1, 1, 1);
			material.Specular	= VulkanColor(1, 1, 1, 1);
			material.Emissive	= VulkanColor(0, 0, 0, 1);
			material.Power		= 80;
			material.Texture	= 0;
		}

		// texture info
		ReadString(infile, buff);

		if( buff[1] != ',' && material.Texture == 0 )
		{
			str = basedir + buff;
			material.Texture = VulkanImage::CreateFromFile(str.c_str(), true);
		}

		ReadString(infile, buff);
		ReadString(infile, buff);
		ReadString(infile, buff);
		ReadString(infile, buff);
		ReadString(infile, buff);
		ReadString(infile, buff);
		ReadString(infile, buff);
	}

	fclose(infile);
	return mesh;
}

//*************************************************************************************************************
//
// VulkanPipelineBarrierBatch impl
//
//*************************************************************************************************************

VulkanPipelineBarrierBatch::VulkanPipelineBarrierBatch(VkPipelineStageFlags srcstage, VkPipelineStageFlags dststage)
{
	src = srcstage;
	dst = dststage;

	buffbarriers.reserve(4);
	imgbarriers.reserve(4);
}

void VulkanPipelineBarrierBatch::BufferAccessBarrier(VkBuffer buffer, VkAccessFlags srcaccess, VkAccessFlags dstaccess, VkDeviceSize offset, VkDeviceSize size)
{
	VkBufferMemoryBarrier barrier = {};

	barrier.sType				= VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.pNext				= NULL;
	barrier.srcAccessMask		= srcaccess;
	barrier.dstAccessMask		= dstaccess;
	barrier.srcQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex	= VK_QUEUE_FAMILY_IGNORED;
	barrier.buffer				= buffer;
	barrier.offset				= offset;
	barrier.size				= size;

	buffbarriers.push_back(barrier);
}

void VulkanPipelineBarrierBatch::ImageLayoutTransfer(VkImage image, VkAccessFlags srcaccess, VkAccessFlags dstaccess, VkImageAspectFlags aspectmask, VkImageLayout oldlayout, VkImageLayout newlayout)
{
	VkImageMemoryBarrier barrier = {};

	barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext							= NULL;
	barrier.srcAccessMask					= srcaccess;
	barrier.dstAccessMask					= dstaccess;
	barrier.oldLayout						= oldlayout;
	barrier.newLayout						= newlayout;
	barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
	barrier.image							= image;
	barrier.subresourceRange.aspectMask		= aspectmask;
	barrier.subresourceRange.baseMipLevel	= 0;
	barrier.subresourceRange.levelCount		= 1;	// TODO: ezt nagyon atalakitani
	barrier.subresourceRange.baseArrayLayer	= 0;
	barrier.subresourceRange.layerCount		= 1;

	imgbarriers.push_back(barrier);
}

void VulkanPipelineBarrierBatch::ImageLayoutTransfer(VulkanImage* image, VkAccessFlags dstaccess, VkImageAspectFlags aspectmask, VkImageLayout newlayout)
{
	VkImageMemoryBarrier barrier = {};

	barrier.sType							= VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.pNext							= NULL;
	barrier.srcAccessMask					= image->GetAccess();
	barrier.dstAccessMask					= dstaccess;
	barrier.oldLayout						= image->GetLayout();
	barrier.newLayout						= newlayout;
	barrier.srcQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex				= VK_QUEUE_FAMILY_IGNORED;
	barrier.image							= image->GetImage();
	barrier.subresourceRange.aspectMask		= aspectmask;
	barrier.subresourceRange.baseMipLevel	= 0;
	barrier.subresourceRange.levelCount		= image->GetMipMapCount();
	barrier.subresourceRange.baseArrayLayer	= 0;
	barrier.subresourceRange.layerCount		= image->GetArraySize();

	imgbarriers.push_back(barrier);
}

void VulkanPipelineBarrierBatch::Enlist(VkCommandBuffer commandbuffer)
{
	VkImageMemoryBarrier*	ibarriers = NULL;
	VkBufferMemoryBarrier*	bbarriers = NULL;

	if( buffbarriers.size() > 0 )
		bbarriers = buffbarriers.data();

	if( imgbarriers.size() > 0 )
		ibarriers = imgbarriers.data();

	if( bbarriers || ibarriers )
		vkCmdPipelineBarrier(commandbuffer, src, dst, 0, 0, NULL, (uint32_t)buffbarriers.size(), bbarriers, (uint32_t)imgbarriers.size(), ibarriers);
}

void VulkanPipelineBarrierBatch::Reset()
{
	buffbarriers.clear();
	imgbarriers.clear();
}

void VulkanPipelineBarrierBatch::Reset(VkPipelineStageFlags srcstage, VkPipelineStageFlags dststage)
{
	src = srcstage;
	dst = dststage;

	buffbarriers.clear();
	imgbarriers.clear();
}

//*************************************************************************************************************
//
// VulkanFramePump impl
//
//*************************************************************************************************************

VulkanFramePump::VulkanFramePump()
{
	FrameFinished = 0;

	buffersinflight = 0;
	currentframe = 0;
	currentdrawable = 0;

	VkCommandBufferAllocateInfo	cmdbuffinfo	= {};
	VkSemaphoreCreateInfo		semainfo	= {};
	VkFenceCreateInfo			fenceinfo	= {};
	VkResult					res;

	semainfo.sType					= VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semainfo.pNext					= NULL;
	semainfo.flags					= 0;

	fenceinfo.sType					= VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceinfo.pNext					= NULL;
	fenceinfo.flags					= 0;

	cmdbuffinfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdbuffinfo.pNext				= NULL;
	cmdbuffinfo.commandPool			= driverinfo.commandpool;
	cmdbuffinfo.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdbuffinfo.commandBufferCount	= VK_NUM_QUEUED_FRAMES;

	res = vkAllocateCommandBuffers(driverinfo.device, &cmdbuffinfo, commandbuffers);
	VK_ASSERT(res == VK_SUCCESS);

	for( int i = 0; i < VK_NUM_QUEUED_FRAMES; ++i ) {
		res = vkCreateSemaphore(driverinfo.device, &semainfo, NULL, &acquiresemas[i]);
		VK_ASSERT(res == VK_SUCCESS);

		res = vkCreateSemaphore(driverinfo.device, &semainfo, NULL, &presentsemas[i]);
		VK_ASSERT(res == VK_SUCCESS);

		res = vkCreateFence(driverinfo.device, &fenceinfo, NULL, &fences[i]);
		VK_ASSERT(res == VK_SUCCESS);
	}
}

VulkanFramePump::~VulkanFramePump()
{
	vkDeviceWaitIdle(driverinfo.device);

	for( int i = 0; i < VK_NUM_QUEUED_FRAMES; ++i ) {
		if( acquiresemas[i] )
			vkDestroySemaphore(driverinfo.device, acquiresemas[i], NULL);

		if( presentsemas[i] )
			vkDestroySemaphore(driverinfo.device, presentsemas[i], NULL);

		if( fences[i] )
			vkDestroyFence(driverinfo.device, fences[i], 0);
	}

	vkFreeCommandBuffers(driverinfo.device, driverinfo.commandpool, VK_NUM_QUEUED_FRAMES, commandbuffers);
}

VkCommandBuffer VulkanFramePump::GetNextCommandBuffer()
{
	return commandbuffers[currentframe];
}

uint32_t VulkanFramePump::GetNextDrawable()
{
	VkResult res;

	res = vkAcquireNextImageKHR(driverinfo.device, driverinfo.swapchain, UINT64_MAX, acquiresemas[currentframe], NULL, &currentdrawable);
	VK_ASSERT(res == VK_SUCCESS);

	return currentdrawable;
}

void VulkanFramePump::Present()
{
	VkSubmitInfo			submitinfo		= {};
	VkPresentInfoKHR		presentinfo		= {};
	VkPipelineStageFlags	pipestageflags	= VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	VkResult				res;
	uint32_t				nextframe		= (currentframe + 1) % VK_NUM_QUEUED_FRAMES;

	submitinfo.pNext				= NULL;
	submitinfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.waitSemaphoreCount	= 1;
	submitinfo.pWaitSemaphores		= &acquiresemas[currentframe];
	submitinfo.pWaitDstStageMask	= &pipestageflags;
	submitinfo.commandBufferCount	= 1;
	submitinfo.pCommandBuffers		= &commandbuffers[currentframe];
	submitinfo.signalSemaphoreCount	= 1;
	submitinfo.pSignalSemaphores	= &presentsemas[currentframe];

	res = vkQueueSubmit(driverinfo.graphicsqueue, 1, &submitinfo, fences[currentframe]);
	VK_ASSERT(res == VK_SUCCESS);

	presentinfo.sType				= VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentinfo.pNext				= NULL;
	presentinfo.swapchainCount		= 1;
	presentinfo.pSwapchains			= &driverinfo.swapchain;
	presentinfo.pImageIndices		= &currentdrawable;
	presentinfo.waitSemaphoreCount	= 1;
	presentinfo.pWaitSemaphores		= &presentsemas[currentframe];
	presentinfo.pResults			= NULL;

	res = vkQueuePresentKHR(driverinfo.graphicsqueue, &presentinfo);
	VK_ASSERT(res == VK_SUCCESS);

	++buffersinflight;

	if( buffersinflight == VK_NUM_QUEUED_FRAMES )
	{
		do {
			res = vkWaitForFences(driverinfo.device, 1, &fences[nextframe], VK_TRUE, 100000000);
		} while( res == VK_TIMEOUT );

		vkResetFences(driverinfo.device, 1, &fences[nextframe]);
		vkResetCommandBuffer(commandbuffers[nextframe], 0);

		if( FrameFinished )
			FrameFinished(nextframe);

		--buffersinflight;
	}

	VK_ASSERT(buffersinflight < VK_NUM_QUEUED_FRAMES);
	currentframe = nextframe;
}

//*************************************************************************************************************
//
// Functions impl
//
//*************************************************************************************************************

VkVertexInputBindingDescription VulkanMakeBindingDescription(uint32_t binding, VkVertexInputRate inputrate, uint32_t stride)
{
	VkVertexInputBindingDescription bindingdesc;

	bindingdesc.binding		= binding;
	bindingdesc.inputRate	= inputrate;
	bindingdesc.stride		= stride;

	return std::move(bindingdesc);
}

VkCommandBuffer VulkanCreateTempCommandBuffer(bool begin)
{
	VkCommandBuffer				cmdbuff = 0;
	VkCommandBufferAllocateInfo	cmdbuffinfo		= {};
	VkCommandBufferBeginInfo	begininfo		= {};
	VkResult					res;

	cmdbuffinfo.sType				= VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdbuffinfo.pNext				= NULL;
	cmdbuffinfo.commandPool			= driverinfo.commandpool;
	cmdbuffinfo.level				= VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdbuffinfo.commandBufferCount	= 1;

	res = vkAllocateCommandBuffers(driverinfo.device, &cmdbuffinfo, &cmdbuff);
	VK_ASSERT(res == VK_SUCCESS);

	if( begin )
	{
		begininfo.sType					= VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begininfo.pNext					= NULL;
		begininfo.flags					= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		begininfo.pInheritanceInfo		= NULL;

		res = vkBeginCommandBuffer(cmdbuff, &begininfo);
		VK_ASSERT(res == VK_SUCCESS);
	}

	return cmdbuff;
}

void VulkanSubmitTempCommandBuffer(VkCommandBuffer commandbuffer, bool wait)
{
	VkSubmitInfo	submitinfo	= {};
	VkResult		res;

	submitinfo.sType				= VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitinfo.pNext				= NULL;
	submitinfo.waitSemaphoreCount	= 0;
	submitinfo.pWaitSemaphores		= NULL;
	submitinfo.pWaitDstStageMask	= 0;
	submitinfo.commandBufferCount	= 1;
	submitinfo.pCommandBuffers		= &commandbuffer;
	submitinfo.signalSemaphoreCount	= 0;
	submitinfo.pSignalSemaphores	= NULL;

	vkEndCommandBuffer(commandbuffer);

	res = vkQueueSubmit(driverinfo.graphicsqueue, 1, &submitinfo, VK_NULL_HANDLE);
	VK_ASSERT(res == VK_SUCCESS);

	if( wait ) {
		res = vkQueueWaitIdle(driverinfo.graphicsqueue);
		VK_ASSERT(res == VK_SUCCESS);

		vkFreeCommandBuffers(driverinfo.device, driverinfo.commandpool, 1, &commandbuffer);
	} else {
		// TODO: !!!
	}
}

bool VulkanQueryFormatSupport(VkFormat format, VkFormatFeatureFlags features)
{
	VkFormatProperties formatprops;
	vkGetPhysicalDeviceFormatProperties(driverinfo.gpus[0], format, &formatprops);

	return ((formatprops.optimalTilingFeatures & features) == features);
}
