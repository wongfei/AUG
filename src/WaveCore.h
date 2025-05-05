#pragma once

// NEXT GEN SOUND PROCESSING LIBRARY OPTIMIZED FOR MAKING THIS PLANET A WARMER PLACE

#include "RawArray.h"

enum class EWaveFormat
{
	UNSUPPORTED,
	PCM,
	FLOAT,
};

struct WaveFormat
{
	EWaveFormat Format;
	uint32_t NumChannels;
	uint32_t SampleRate;
	uint32_t BitsPerSample;
	uint32_t BlockAlign;

	inline void ComputeBlockAlign() { BlockAlign = NumChannels * (BitsPerSample / 8); }
};

#pragma pack(push, 1)
struct WaveHeader // http://soundfile.sapp.org/doc/WaveFormat/
{
	char ChunkID[4];
	uint32_t ChunkSize;
	char Format[4];
	char Subchunk1ID[4];
	uint32_t Subchunk1Size;
	uint16_t AudioFormat;
	uint16_t NumChannels;
	uint32_t SampleRate;
	uint32_t ByteRate;
	uint16_t BlockAlign;
	uint16_t BitsPerSample;
	char Subchunk2ID[4];
	uint32_t Subchunk2Size;
};
#pragma pack(pop)

void SaveWave(const char* Filename, const WaveFormat& Format, const TRawArray<float>& Samples);

void S16toF32(const int16_t* Src, size_t SrcCount, float* Dst, size_t DstCount);
void F32toS16(const float* Src, size_t SrcCount, int16_t* Dst, size_t DstCount);
void Stereo2Mono(const float* Src, size_t SrcCount, TRawArray<float>& Dst);
