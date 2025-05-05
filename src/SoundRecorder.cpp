#include "SoundRecorder.h"

#if defined(_WIN32)

#include <windows.h>
#include <atlbase.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

namespace Win32 {

class WASAPISoundRecorder : public ISoundRecorder
{
public:

	virtual ~WASAPISoundRecorder() override { Release(); }
	virtual bool Init(WaveFormat& OutDeviceFormat) override;
	virtual void Release() override;
	virtual void Flush() override;
	virtual PollResult Poll(TRawArray<uint8_t>& DstBuffer) override;

private:

	CComPtr<IMMDevice> pDevice;
	CComPtr<IAudioClient> pAudioClient;
	CComPtr<IAudioCaptureClient> pCaptureClient;
	WaveFormat DeviceFormat {};
};

static WaveFormat ToWaveFormat(const WAVEFORMATEX* wf)
{
	EWaveFormat Format = EWaveFormat::UNSUPPORTED;

	switch (wf->wFormatTag)
	{
		case WAVE_FORMAT_PCM: Format = EWaveFormat::PCM; break;
		case WAVE_FORMAT_IEEE_FLOAT: Format = EWaveFormat::FLOAT; break;
	}

	if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		auto ex = (const WAVEFORMATEXTENSIBLE*)wf;
		if (IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) { Format = EWaveFormat::PCM; }
		else if (IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) { Format = EWaveFormat::FLOAT; }
	}

	WaveFormat res {};
	res.Format = Format;
	res.NumChannels = wf->nChannels;
	res.SampleRate = wf->nSamplesPerSec;
	res.BitsPerSample = wf->wBitsPerSample;
	res.BlockAlign = wf->nBlockAlign;
	return res;
}

static void PrintFormat(const char* prefix, const WAVEFORMATEX* wf)
{
	const char* FormatName = "?";

	switch (wf->wFormatTag)
	{
		case WAVE_FORMAT_PCM: FormatName = "PCM"; break;
		case WAVE_FORMAT_IEEE_FLOAT: FormatName = "FLOAT"; break;
		case WAVE_FORMAT_DRM: FormatName = "DRM"; break;
		case WAVE_FORMAT_ALAW: FormatName = "ALAW"; break;
		case WAVE_FORMAT_MULAW: FormatName = "MULAW"; break;
		case WAVE_FORMAT_ADPCM: FormatName = "ADPCM"; break;
		case WAVE_FORMAT_MPEG: FormatName = "MPEG"; break;
		case WAVE_FORMAT_DOLBY_AC3_SPDIF: FormatName = "DOLBY_AC3_SPDIF"; break;
		case WAVE_FORMAT_WMASPDIF: FormatName = "WMASPDIF"; break;
	}

	if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
	{
		auto ex = (const WAVEFORMATEXTENSIBLE*)wf;
		if (IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_PCM)) { FormatName = "SUBTYPE_PCM"; }
		else if (IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) { FormatName = "SUBTYPE_FLOAT"; }
		else if (IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_DRM)) { FormatName = "SUBTYPE_DRM"; }
		else if (IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_ALAW)) { FormatName = "SUBTYPE_ALAW"; }
		else if (IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_MULAW)) { FormatName = "SUBTYPE_MULAW"; }
		else if (IsEqualGUID(ex->SubFormat, KSDATAFORMAT_SUBTYPE_ADPCM)) { FormatName = "SUBTYPE_ADPCM"; }
	}

	logi("{}{} Chan={} Rate={} Bits={}", prefix, FormatName, wf->nChannels, wf->nSamplesPerSec, wf->wBitsPerSample);
}

bool WASAPISoundRecorder::Init(WaveFormat& OutDeviceFormat)
{
	HRESULT hr;

	do
	{
		// https://learn.microsoft.com/en-us/windows/win32/coreaudio/capturing-a-stream

		hr = CoInitialize(nullptr);
		GUARD_HR_BREAK(hr, "CoInitialize");

		CComPtr<IMMDeviceEnumerator> pEnumerator;
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
		GUARD_HR_BREAK(hr, "Failed to create MMDeviceEnumerator");

		hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
		GUARD_HR_BREAK(hr, "Failed to get default audio endpoint");

		hr = pDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&pAudioClient);
		GUARD_HR_BREAK(hr, "Failed to activate audio client");

		CComHeapPtr<WAVEFORMATEX> MixFormat; // CoTaskMemFree
		hr = pAudioClient->GetMixFormat(&MixFormat);
		GUARD_HR_BREAK(hr, "Failed to get mix format");

		WAVEFORMATEX* pSelectedFormat = MixFormat.m_pData;
		PrintFormat("MixFormat: ", pSelectedFormat);

		size_t NumFramesDesired = 1024;
		double BufferDurationSeconds = (double)NumFramesDesired / (double)MixFormat->nSamplesPerSec;
		double ReferenceTimeBase = 1e7;
		REFERENCE_TIME DesiredBufferDuration = static_cast<REFERENCE_TIME>(BufferDurationSeconds * ReferenceTimeBase);

		// hnsBufferDuration = 10000000 means 1 second of audio buffering
		hr = pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, DesiredBufferDuration, 0, pSelectedFormat, nullptr);
		GUARD_HR_BREAK(hr, "Failed to initialize audio client");

		DeviceFormat = ToWaveFormat(pSelectedFormat);
		OutDeviceFormat = DeviceFormat;

		//pAudioClient->GetBufferSize(&NumBufferFrames);
		//logi("NumBufferFrames={}", NumBufferFrames);

		hr = pAudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&pCaptureClient);
		GUARD_HR_BREAK(hr, "Failed to get audio capture client");

		hr = pAudioClient->Start();
		GUARD_HR_BREAK(hr, "Failed to start audio capture");

		Flush();

		return true;
	}
	while (0);

	Release();
	return false;
}

void WASAPISoundRecorder::Release()
{
	if (pAudioClient)
		pAudioClient->Stop();

	pCaptureClient.Release();
	pAudioClient.Release();
	pDevice.Release();
}

void WASAPISoundRecorder::Flush()
{
	HRESULT hr;
	UINT32 NumFramesInNextPacket = 0;

	hr = pCaptureClient->GetNextPacketSize(&NumFramesInNextPacket);
	if (hr != S_OK)
	{
		loge("GetNextPacketSize hr={}", (size_t)hr);
		return;
	}

	while (NumFramesInNextPacket)
	{
		BYTE* pData = nullptr;
		UINT32 NumFramesToRead = 0;
		DWORD Flags = 0;

		hr = pCaptureClient->GetBuffer(&pData, &NumFramesToRead, &Flags, nullptr, nullptr);
		if (hr != S_OK)
		{
			loge("GetBuffer hr={}", (size_t)hr);
			return;
		}

		pCaptureClient->ReleaseBuffer(NumFramesToRead);

		hr = pCaptureClient->GetNextPacketSize(&NumFramesInNextPacket);
		if (hr != S_OK)
		{
			loge("GetNextPacketSize hr={}", (size_t)hr);
			return;
		}
	}
}

ISoundRecorder::PollResult WASAPISoundRecorder::Poll(TRawArray<uint8_t>& DstBuffer)
{
	ISoundRecorder::PollResult Res {};

	if (!pCaptureClient) 
	{
		loge("CaptureClient not initialized");
		Res.ErrorCode = (size_t)E_FAIL;
		return Res;
	}

	HRESULT hr;
	UINT32 NumFramesInNextPacket = 0;
	hr = pCaptureClient->GetNextPacketSize(&NumFramesInNextPacket);
	if (hr != S_OK)
	{
		loge("GetNextPacketSize hr={}", (size_t)hr);
		Res.ErrorCode = (size_t)hr;
		return Res;
	}

	if (!NumFramesInNextPacket) // no data available
	{
		return Res;
	}

	BYTE* pData = nullptr;
	UINT32 NumFramesToRead = 0;
	DWORD Flags = 0;

	// The client should either read the entire data packet or none of it.
	hr = pCaptureClient->GetBuffer(&pData, &NumFramesToRead, &Flags, nullptr, nullptr);
	if (hr != S_OK)
	{
		loge("GetBuffer hr={}", (size_t)hr);
		Res.ErrorCode = (size_t)hr;
		return Res;
	}

	// The size of a frame in an audio stream is specified by the nBlockAlign member of the WAVEFORMATEX.
	// The size, in bytes, of an audio frame equals the number of channels in the stream multiplied by the sample size per channel.
	const size_t NumBytesToRead = NumFramesToRead * DeviceFormat.BlockAlign;

	DstBuffer.resize(NumBytesToRead);

	if (NumBytesToRead)
	{
		if (Flags & AUDCLNT_BUFFERFLAGS_SILENT)
		{
			//logi("silent {}", NumBytesToRead);
			memset(DstBuffer.data(), 0, NumBytesToRead);
			Res.Silent = true;
		}
		else
		{
			//logi("{}", NumBytesToRead);
			memcpy(DstBuffer.data(), pData, NumBytesToRead);
		}
	}

	// This parameter must be either equal to the number of frames in the previously acquired data packet or 0.
	pCaptureClient->ReleaseBuffer(NumFramesToRead);

	Res.DataSize = NumBytesToRead;
	return Res;
}

} // namespace

ISoundRecorder* ISoundRecorder::CreateInstance()
{
	return new Win32::WASAPISoundRecorder();
}

#endif // _WIN32
