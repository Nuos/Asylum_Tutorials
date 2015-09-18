
#ifndef _DDS_H_
#define _DDS_H_

enum DDS_Format
{
	RGBA8 = 0,
	DXT1 = 1,
	DXT5 = 2,
	RGBA16F = 3
};

struct DDS_Image_Info
{
	unsigned int	Width;
	unsigned int	Height;
	unsigned int	Format;
	unsigned int	MipLevels;
	unsigned int	DataSize;
	void*			Data;
};

bool LoadFromDDS(const char* file, DDS_Image_Info* outinfo);
bool SaveToDDS(const char* file, const DDS_Image_Info* info);

#endif
