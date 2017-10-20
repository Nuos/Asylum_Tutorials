
#ifndef _DDS_H_
#define _DDS_H_

struct DDS_Image_Info
{
	unsigned int	Width;
	unsigned int	Height;
	unsigned int	Depth;
	unsigned int	Format;
	unsigned int	MipLevels;
	unsigned int	DataSize;
	void*			Data;
};

bool LoadFromDDS(const char* file, DDS_Image_Info* outinfo);
bool SaveToDDS(const char* file, const DDS_Image_Info* info);

unsigned int GetImageSize(unsigned int width, unsigned int height, unsigned int bytes, unsigned int miplevels);
unsigned int GetCompressedImageSize(unsigned int width, unsigned int height, unsigned int miplevels, unsigned int format);
unsigned int GetCompressedImageSize(unsigned int width, unsigned int height, unsigned int depth, unsigned int miplevels, unsigned int format);
unsigned int GetCompressedLevelSize(unsigned int width, unsigned int height, unsigned int level, unsigned int format);
unsigned int GetCompressedLevelSize(unsigned int width, unsigned int height, unsigned int depth, unsigned int level, unsigned int format);

#endif
