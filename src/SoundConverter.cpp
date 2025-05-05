#include "SoundConverter.h"

#include <samplerate.h>

class StoneAgeSoundConverter : public ISoundConverter
{
public:

	virtual ~StoneAgeSoundConverter() override { Release(); }
	virtual bool Init(const WaveFormat& SrcFormat, const WaveFormat& DstFormat) override;
	virtual void Release() override;
	virtual bool Process(const TRawArray<uint8_t>& InBytes) override;
	virtual TRawArray<float>& GetOutputBuffer() override { return SamplesBuf; }

private:

	WaveFormat InputFormat {};
	WaveFormat OutputFormat {};
	SRC_STATE* ResamplerState = nullptr;

	TRawArray<float> SamplesBuf;
	TRawArray<float> TmpBuf;
};

static void PrintFormat(const char* Prefix, const WaveFormat& wf)
{
	const char* FormatName = "UNSUPPORTED";
	switch (wf.Format)
	{
		case EWaveFormat::PCM: FormatName = "PCM"; break; 
		case EWaveFormat::FLOAT: FormatName = "FLOAT"; break; 
	}

	logi("{}{} Chan={} Rate={} Bits={}", Prefix, FormatName, wf.NumChannels, wf.SampleRate, wf.BitsPerSample);
}

static bool ValidateFormat(const WaveFormat& wf)
{
	return 
		(wf.Format != EWaveFormat::UNSUPPORTED)
		&& (wf.NumChannels == 1 || wf.NumChannels == 2)
		&& (wf.SampleRate != 0)
		&& (wf.BitsPerSample == 16 || wf.BitsPerSample == 32);
}

bool StoneAgeSoundConverter::Init(const WaveFormat& SrcFormat, const WaveFormat& DstFormat)
{
	do
	{
		InputFormat = SrcFormat;
		OutputFormat = DstFormat;

		PrintFormat("InputFormat: ", InputFormat);
		PrintFormat("OutputFormat: ", OutputFormat);

		if (!ValidateFormat(InputFormat))
		{
			loge("Unsupported InputFormat");
			break;
		}

		if (!ValidateFormat(OutputFormat))
		{
			loge("Unsupported OutputFormat");
			break;
		}

		if (OutputFormat.Format != EWaveFormat::FLOAT || OutputFormat.BitsPerSample != 32)
		{
			loge("Unsupported OutputFormat");
			break;
		}

		int Error = 0;
		ResamplerState = src_new(SRC_LINEAR, DstFormat.NumChannels, &Error);
		if (Error != 0)
		{
			loge("src_new Error={}", Error);
			break;
		}

		return true;
	}
	while (0);

	Release();
	return false;
}

void StoneAgeSoundConverter::Release()
{
	if (ResamplerState)
	{
		src_delete(ResamplerState);
		ResamplerState = nullptr;
	}
}

bool StoneAgeSoundConverter::Process(const TRawArray<uint8_t>& InBytes)
{
	if (!InBytes.size())
		return false;

	//const size_t NumChunkFrames = InBytes.size() / InputFormat.BlockAlign;
	//const size_t NumChunkSamples = InBytes.size() / (InputFormat.BitsPerSample / 8);

	if (InputFormat.Format == EWaveFormat::FLOAT && InputFormat.BitsPerSample == 32)
	{
		const TRawArrayView<float> ChunkF32(InBytes);
		SamplesBuf.copy(ChunkF32.data(), ChunkF32.size());
	}
	else if (InputFormat.Format == EWaveFormat::PCM && InputFormat.BitsPerSample == 16) // not tested
	{
		const TRawArrayView<int16_t> ChunkS16(InBytes);
		SamplesBuf.resize(ChunkS16.size());
		S16toF32(ChunkS16.data(), ChunkS16.size(), SamplesBuf.data(), SamplesBuf.size());
	}
	else
	{
		loge("Unsupported InputFormat");
		return false;
	}

	#if 1
	if (InputFormat.NumChannels != OutputFormat.NumChannels)
	{
		if (InputFormat.NumChannels == 2 && OutputFormat.NumChannels == 1)
		{
			Stereo2Mono(SamplesBuf.data(), SamplesBuf.size(), TmpBuf);
			TmpBuf.swap(SamplesBuf);
		}
		else
		{
			loge("Unsupported NumChannels");
			return false;
		}
	}
	#endif

	#if 1
	if (InputFormat.SampleRate != OutputFormat.SampleRate)
	{
		if (!ResamplerState)
		{
			loge("resampler not initialized");
			return false;
		}

		int Error = src_reset(ResamplerState);
		if (Error != 0)
		{
			loge("src_reset Error={}", Error);
			return false;
		}

		const double Ratio = (double)OutputFormat.SampleRate / (double)InputFormat.SampleRate;
		const size_t NumSamples = SamplesBuf.size();
		const size_t NumConvertedSamples = (size_t)(NumSamples * Ratio) + 16;
		TmpBuf.resize(NumConvertedSamples);

		SRC_DATA Data {};
		Data.data_in = SamplesBuf.data();
		Data.input_frames = (long)NumSamples;
		Data.data_out = TmpBuf.data();
		Data.output_frames = (long)NumConvertedSamples;
		Data.src_ratio = Ratio;
		Data.end_of_input = 0;

		src_set_ratio(ResamplerState, Ratio);

		//logi("Resample {} -> {} Ratio={}", NumSamples, NumConvertedSamples, Ratio);
		Error = src_process(ResamplerState, &Data);
		if (Error != 0)
		{
			loge("src_process Error={}", Error);
			return false;
		}

		TmpBuf.resize(Data.output_frames_gen);
		TmpBuf.swap(SamplesBuf);
	}
	#endif

	return true;
}

ISoundConverter* ISoundConverter::CreateInstance()
{
	return new StoneAgeSoundConverter();
}
