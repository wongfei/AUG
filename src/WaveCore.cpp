#include "WaveCore.h"

#include <iostream>
#include <fstream>

void SaveWave(const char* Filename, const WaveFormat& Format, const TRawArray<float>& Samples)
{
	WaveHeader header {};

	const char RIFF[4] = {'R', 'I', 'F', 'F'};
	const char WAVE[4] = {'W', 'A', 'V', 'E'};
	const char FMT[4] = {'f', 'm', 't', ' '};
	const char DATA[4] = {'d', 'a', 't', 'a'};

	memcpy(header.ChunkID, RIFF, sizeof(RIFF));
	memcpy(header.Format, WAVE, sizeof(WAVE));
	memcpy(header.Subchunk1ID, FMT, sizeof(FMT));
	memcpy(header.Subchunk2ID, DATA, sizeof(DATA));

	header.Subchunk1Size = 16; // THIS IS SPARTA!!!

	switch (Format.Format)
	{
		case EWaveFormat::PCM: header.AudioFormat = 1; break;
		case EWaveFormat::FLOAT: header.AudioFormat = 3; break;
		default: header.AudioFormat = 0; break;
	}

	header.NumChannels = (uint16_t)Format.NumChannels;
	header.SampleRate = Format.SampleRate;
	header.ByteRate = Format.SampleRate * Format.NumChannels * (Format.BitsPerSample / 8);
	header.BlockAlign = (uint16_t)(Format.NumChannels * (Format.BitsPerSample / 8));
	header.BitsPerSample = (uint16_t)Format.BitsPerSample;

	header.Subchunk2Size = (uint32_t)(Samples.size() * (Format.BitsPerSample / 8));
	header.ChunkSize = 36 + header.Subchunk2Size;
	
	static_assert(sizeof(header) - 8 == 36);

	std::ofstream outFile(Filename, std::ios::binary);
	if (outFile)
	{
		outFile.write((const char*)&header, sizeof(header));
		outFile.write((const char*)Samples.data(), header.Subchunk2Size);
		outFile.close();
	}
}

// NICE TO HAVE 9000 GHZ CPU

void S16toF32(const int16_t* Src, size_t SrcCount, float* Dst, size_t DstCount)
{
	const size_t Num = std::min(SrcCount, DstCount);

	for (size_t i = 0; i < Num; ++i)
	{
		Dst[i] = float(Src[i]) / 32768.0f;
	}

	for (size_t i = Num; i < DstCount; ++i)
	{
		Dst[i] = 0.0f;
	}
}

void F32toS16(const float* Src, size_t SrcCount, int16_t* Dst, size_t DstCount)
{
	const size_t Num = std::min(SrcCount, DstCount);

	for (size_t i = 0; i < Num; ++i)
	{
		Dst[i] = (int16_t)(Src[i] * 32768.0f);
	}

	for (size_t i = Num; i < DstCount; ++i)
	{
		Dst[i] = 0;
	}
}

void Stereo2Mono(const float* Src, size_t SrcCount, TRawArray<float>& Dst)
{
	Dst.resize(SrcCount / 2);

	for (size_t i = 0, j = 0; i < SrcCount; i += 2, j++)
	{
		Dst[j] = (Src[i] + Src[i + 1]) * 0.5f;
	}
}
