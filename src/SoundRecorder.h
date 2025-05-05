#pragma once

#include "WaveCore.h"

class ISoundRecorder
{
public:

	static ISoundRecorder* CreateInstance();

	virtual ~ISoundRecorder() {}
	virtual bool Init(WaveFormat& OutDeviceFormat) = 0;
	virtual void Release() = 0;
	virtual void Flush() = 0;

	struct PollResult { size_t ErrorCode; size_t DataSize; bool Silent; };
	virtual PollResult Poll(TRawArray<uint8_t>& DstBuffer) = 0;
};
