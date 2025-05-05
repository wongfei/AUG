#include "SpeechToText.h"
#include "SoundRecorder.h"
#include "SoundConverter.h"

#include <BS_thread_pool.hpp>
#include <samplerate.h>
#include <whisper.h>
#include <imgui.h>

#include <iostream>
#include <fstream>
#include <filesystem>

class WhisperSpeechToText : public ISpeechToText
{
public:

	virtual ~WhisperSpeechToText() override { Release(); }
	virtual void Serialize(IniFile& Config, bool Save) override;
	virtual bool Init(ResultCallback Callback) override;
	virtual void Release() override;
	virtual void RenderUI() override;

private:

	void PollRecorder();
	void WhisperProcess(const TRawArray<float>& InSamples);

	ResultCallback ResCallback;

	float SampleLevelThreshold = 0.005f;
	float SplitSilenceDuration = 0.05f;
	float FlushSilenceDuration = 10.0f;
	float MinSegmentDuration = 2.0f;
	float MaxSegmentDuration = 10.0f;

	std::string WhisperModel = "ggml-small.bin";
	int NumProcessors = 1;
	int NumThreads = 8;
	bool UseGpu = false;

	WaveFormat DeviceFormat {};
	WaveFormat WhisperFormat {};

	std::unique_ptr<ISoundRecorder> Recorder;
	std::unique_ptr<ISoundConverter> Converter;

	whisper_context_params WhisperContextParams {};
	whisper_full_params WhisperFullParams {};
	whisper_context* WhisperContext {};

	std::chrono::high_resolution_clock::time_point LastSampleTimestamp {};
	std::chrono::high_resolution_clock::time_point LastProcessTimestamp {};

	TRawArray<uint8_t> ChunkBytes;
	TRawArray<float> SampleAccumulator;
	TRawArray<float> DebugWavBuffer;

	float SegmentDuration = 0;
	float SilenceDuration = 0;
	bool Silent = false;
	bool Paused = false;

	std::atomic<int> ExitFlag;
	std::unique_ptr<BS::thread_pool<BS::tp::none>> WhisperThreadPool;
	std::unique_ptr<std::thread> RecorderThread;
};

bool WhisperSpeechToText::Init(ResultCallback Callback)
{
	do
	{
		ResCallback = std::move(Callback);

		DeviceFormat = {};
		Recorder.reset(ISoundRecorder::CreateInstance());
		GUARD_BREAK(Recorder->Init(DeviceFormat), "Failed to init sound recorder");
		WhisperFormat = DeviceFormat;

		WhisperFormat.Format = EWaveFormat::FLOAT;
		WhisperFormat.NumChannels = 1;
		WhisperFormat.SampleRate = WHISPER_SAMPLE_RATE;
		WhisperFormat.ComputeBlockAlign();

		Converter.reset(ISoundConverter::CreateInstance());
		GUARD_BREAK(Converter->Init(DeviceFormat, WhisperFormat), "Failed to init sound converter");

		LastSampleTimestamp = std::chrono::high_resolution_clock::now();
		LastProcessTimestamp = std::chrono::high_resolution_clock::now();

		std::filesystem::path FullModelPath(WhisperModel);
		if (!std::filesystem::exists(FullModelPath))
		{
			char ExePath[MAX_PATH] = {0};
			GetModuleFileNameA(NULL, ExePath, MAX_PATH); // std::filesystem IS SO GOOD
			auto ExeDir(std::filesystem::path(ExePath).parent_path());

			FullModelPath = ExeDir / WhisperModel;
			if (!std::filesystem::exists(FullModelPath))
			{
				FullModelPath = (ExeDir / "models" / WhisperModel);
				if (!std::filesystem::exists(FullModelPath))
				{
					//ModelPath = getenv("WHISPER_MODEL");
					loge("File not found: {}", FullModelPath.string().c_str());
					break;
				}
			}
		}

		//whisper_log_set(&WhisperLogCallback, nullptr);

		WhisperContextParams = whisper_context_default_params();
		WhisperContextParams.use_gpu = UseGpu;

		WhisperFullParams = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
		WhisperFullParams.n_threads = NumThreads;

		WhisperContext = whisper_init_from_file_with_params(FullModelPath.string().c_str(), WhisperContextParams);
		GUARD_BREAK(WhisperContext, "whisper_init");

		ExitFlag = 0;
		WhisperThreadPool.reset(new BS::thread_pool(1)); // don't change

		RecorderThread.reset(new std::thread([this]()
		{
			while (!ExitFlag) { PollRecorder(); Sleep(1); } // we have cores to spin, samurai
		}));

		return true;
	}
	while (0);

	Release();
	return false;
}

void WhisperSpeechToText::Release()
{
	if (RecorderThread) { logi("WhisperSpeechToText::Release"); }

	ExitFlag = 1;
	ResCallback = {};

	if (RecorderThread)
	{
		RecorderThread->join();
		RecorderThread.reset();
	}

	if (WhisperThreadPool)
	{
		WhisperThreadPool->purge();
		WhisperThreadPool.reset();
	}

	if (WhisperContext)
	{
		whisper_free(WhisperContext);
		WhisperContext = nullptr;
	}
	whisper_log_set(nullptr, nullptr);

	Recorder.reset();
	Converter.reset();

	if (DebugWavBuffer.size())
	{
		SaveWave("debug.wav", WhisperFormat, DebugWavBuffer);
		DebugWavBuffer.clear();
	}
}

void WhisperSpeechToText::PollRecorder()
{
	size_t RecordedBytes = 0;
	for (;;)
	{
		const auto Result = Recorder->Poll(ChunkBytes);
		if (!Result.DataSize)
		{
			break;
		}

		if (Converter->Process(ChunkBytes))
		{
			const auto& ConvertedBuffer = Converter->GetOutputBuffer();
			SampleAccumulator.append(ConvertedBuffer);

			#if 0
			DebugWavBuffer.append(ConvertedBuffer);
			#endif

			Silent = true;
			for (size_t i = 0; i < ConvertedBuffer.size(); ++i)
			{
				if (fabsf(ConvertedBuffer[i]) > SampleLevelThreshold)
				{
					Silent = false;
					break;
				}
			}

			if (!Silent)
			{
				LastSampleTimestamp = std::chrono::high_resolution_clock::now();
			}
		}

		RecordedBytes += ChunkBytes.size();
		ChunkBytes.resize(0);
	}

	const auto Now = std::chrono::high_resolution_clock::now();
	SilenceDuration = (float)(std::chrono::duration_cast<std::chrono::milliseconds>(Now - LastSampleTimestamp).count() * 0.001);

	const size_t NumChunkSamples = SampleAccumulator.size();
	SegmentDuration = NumChunkSamples / (float)WhisperFormat.SampleRate;

	if ((SegmentDuration > MinSegmentDuration && SilenceDuration > SplitSilenceDuration)
		|| SegmentDuration > MaxSegmentDuration)
	{
		if (LastSampleTimestamp > LastProcessTimestamp)
		{
			LastProcessTimestamp = std::chrono::high_resolution_clock::now();

			if (WhisperContext && WhisperThreadPool && !Paused && !ExitFlag)
			{
				auto Fut = WhisperThreadPool->submit_task([this, TaskBuffer = std::move(SampleAccumulator)]() mutable
				{
					WhisperProcess(TaskBuffer);
				});
			}
			else
			{
				SampleAccumulator.resize(0);
			}
		}
		else
		{
			SampleAccumulator.resize(0);
		}
	}

	if (SilenceDuration > FlushSilenceDuration && SegmentDuration > 0.0f)
	{
		SampleAccumulator.resize(0);
	}
}

void WhisperSpeechToText::WhisperProcess(const TRawArray<float>& InSamples)
{
	Result Res {};

	// minimal buffer duration is 1.0s -> https://github.com/ggerganov/whisper.cpp/issues/39
	const int Error = whisper_full_parallel(WhisperContext, WhisperFullParams, InSamples.data(), (int)InSamples.size(), NumProcessors);
	if (Error != 0)
	{
		loge("whisper_full_parallel Error={}", Error);
	}
	else
	{
		const int n_segments = whisper_full_n_segments(WhisperContext);
		for (int i = 0; i < n_segments; ++i)
		{
			#if 0
			for (int j = 0; j < whisper_full_n_tokens(WhisperContext, i); ++j)
			{
				const char* TokenText = whisper_full_get_token_text(WhisperContext, i, j);
				const float TokenP = whisper_full_get_token_p(WhisperContext, i, j);

				logi("TOK # {} # P={}", TokenText, TokenP);
			}
			#endif

			const char* SegmentText = whisper_full_get_segment_text(WhisperContext, i);
			if (SegmentText)
			{
				Res.Segments.push_back(SegmentText);
				Res.Text.append(SegmentText);
			}
		}
	}

	if (ResCallback)
		ResCallback(Res);
}

void WhisperSpeechToText::Serialize(IniFile& Config, bool Save)
{
	INI_SERIALIZE_PROP("Speech", WhisperModel);
	INI_SERIALIZE_PROP("Speech", NumProcessors);
	INI_SERIALIZE_PROP("Speech", NumThreads);
	INI_SERIALIZE_PROP("Speech", UseGpu);

	INI_SERIALIZE_PROP("Speech", SampleLevelThreshold);
	INI_SERIALIZE_PROP("Speech", SplitSilenceDuration);
	INI_SERIALIZE_PROP("Speech", FlushSilenceDuration);
	INI_SERIALIZE_PROP("Speech", MinSegmentDuration);
	INI_SERIALIZE_PROP("Speech", MaxSegmentDuration);
}

void WhisperSpeechToText::RenderUI()
{
	ImGui::Checkbox("Paused", &Paused);
	ImGui::SliderFloat("SampleLevelThreshold", &SampleLevelThreshold, 0.0f, 0.1f);
	ImGui::SliderFloat("SplitSilenceDuration", &SplitSilenceDuration, 0.0f, 0.5f);
	ImGui::SliderFloat("FlushSilenceDuration", &FlushSilenceDuration, SplitSilenceDuration + 0.1f, 30.0f);
	ImGui::SliderFloat("MinSegmentDuration", &MinSegmentDuration, 1.0f, 10.0f);
	ImGui::SliderFloat("MaxSegmentDuration", &MaxSegmentDuration, MinSegmentDuration + 0.1f, 30.0f);

	ImGui::Text("SegmentDuration %f", SegmentDuration);
	ImGui::Text("SilenceDuration %f", SilenceDuration);
	ImGui::Text("%s", (Silent ? "Silent" : "Active"));
}

ISpeechToText* ISpeechToText::CreateInstance()
{
	return new WhisperSpeechToText();
}
