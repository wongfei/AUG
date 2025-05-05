#pragma once

#include "WaveCore.h"

class ISoundConverter
{
public:

	static ISoundConverter* CreateInstance();

	virtual ~ISoundConverter() {}
	virtual bool Init(const WaveFormat& SrcFormat, const WaveFormat& DstFormat) = 0;
	virtual void Release() = 0;
	virtual bool Process(const TRawArray<uint8_t>& InBytes) = 0;
	virtual TRawArray<float>& GetOutputBuffer() = 0;
};
