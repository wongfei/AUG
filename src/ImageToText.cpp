#include "ImageToText.h"
#include "ScreenCapture.h"

#include <BS_thread_pool.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/text/ocr.hpp>
#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

class TesseractImageToText : public IImageToText
{
public:

	virtual ~TesseractImageToText() override { Release(); }
	virtual bool Init(ResultCallback Callback) override;
	virtual void Release() override;
	virtual void Process(IntRect Region) override;

private:

	void ProcessTask(IntRect Region, Result& Res);

	ResultCallback ResCallback;

	std::unique_ptr<IScreenCapture> ScreenCapture;
	std::unique_ptr<tesseract::TessBaseAPI> TessApi;

	std::unique_ptr<BS::thread_pool<BS::tp::none>> ThreadPool;
};

bool TesseractImageToText::Init(ResultCallback Callback)
{
	do
	{
		ResCallback = std::move(Callback);

		ScreenCapture.reset(IScreenCapture::CreateInstance()); 
		GUARD_BREAK(ScreenCapture->Init(), "Failed to init screen capture");

		setMsgSeverity(L_SEVERITY_ERROR); // STFU leptonica

		// https://tesseract-ocr.github.io/tessdoc/ImproveQuality

		TessApi.reset(new tesseract::TessBaseAPI());
		int TessError = TessApi->Init(NULL, "eng", tesseract::OEM_LSTM_ONLY);
		if (TessError)
		{
			loge("tess::Init Error={}", TessError);
			break;
		}

		TessApi->SetPageSegMode(tesseract::PSM_AUTO_OSD);
		//TessApi->SetVariable("tessedit_char_whitelist", "");
		//TessApi->SetVariable("save_best_choices", "T");
	
		ThreadPool.reset(new BS::thread_pool(1)); // don't change

		return true;
	}
	while (0);

	Release();
	return false;
}

void TesseractImageToText::Release()
{
	if (ThreadPool) { logi("TesseractImageToText::Release"); }

	ResCallback = {};
	ThreadPool.reset();
	ScreenCapture.reset();
	TessApi.reset();
}

void TesseractImageToText::Process(IntRect Region)
{
	if (!ThreadPool)
		return;

	auto Fut = ThreadPool->submit_task([this, Region = std::move(Region)] ()
	{
		Result Res {};

		ProcessTask(std::move(Region), Res);

		if (ResCallback)
			ResCallback(Res);
	});
}

void TesseractImageToText::ProcessTask(IntRect Region, Result& Res)
{
	// https://tesseract-ocr.github.io/tessdoc/Examples_C++.html
	// opencv_contrib\modules\text\src\ocr_tesseract.cpp

	if (!TessApi)
		return;

	{
		//AUG_PERF("CaptureScreen");
		if (!ScreenCapture->Capture())
		{
			loge("CaptureScreen");
			return;
		}
	}

	cv::Mat& InMat = ScreenCapture->GetImage();

	cv::Mat TessMat;
	{
		//AUG_PERF("cvtColor");
		cv::cvtColor(InMat, TessMat, cv::COLOR_BGRA2GRAY);
	}

	#if AUG_DEBUG_OCR
	cv::Mat DebugMat;
	cv::cvtColor(InMat, DebugMat, cv::COLOR_BGRA2BGR);
	#endif

	//AUG_PERF("tess::TOTAL");

	{
		//AUG_PERF("tess::SetImage");
		TessApi->SetImage((uchar*)TessMat.data, TessMat.size().width, TessMat.size().height, TessMat.channels(), (int)TessMat.step1());

		if (Region.Width() > 0 && Region.Height() > 0)
		{
			TessApi->SetRectangle(Region.Left, Region.Top, Region.Width(), Region.Height());
		}
	}

	{
		//AUG_PERF("tess::Recognize");
		int Error = TessApi->Recognize(nullptr);
		if (Error)
		{
			loge("tess::Recognize Error={}", Error);
			return;
		}
	}

	{
		//AUG_PERF("tess::GetUTF8Text");
		const char* Text = TessApi->GetUTF8Text();
		if (Text)
		{
			Res.Text.assign(Text);
			delete[] Text; // nice API clowns
		}
	}

	tesseract::ResultIterator* TessIter = TessApi->GetIterator();
	//tesseract::PageIteratorLevel TessLevel = tesseract::RIL_WORD;
	tesseract::PageIteratorLevel TessLevel = tesseract::RIL_TEXTLINE;

	if (TessIter)
	{
		//AUG_PERF("tess::ResultIterator");
		do
		{
			const char* TessWord = TessIter->GetUTF8Text(TessLevel);
			if (TessWord)
			{
				int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
				TessIter->BoundingBox(TessLevel, &x1, &y1, &x2, &y2);
				const float Confidence = TessIter->Confidence(TessLevel);

				Detection Det;
				Det.Rect = {x1, y1, x2, y2};
				Det.Confidence = Confidence;
				Det.Text = TessWord;
				Res.Detections.emplace_back(std::move(Det));

				#if AUG_DEBUG_OCR
					cv::rectangle(DebugMat, cv::Rect(x1, y1, x2 - x1, y2 - y1), cv::Scalar(0, 0, 255));
				#endif

				delete[] TessWord; // nice API clowns
			}
		}
		while (TessIter->Next(TessLevel));

		delete TessIter; // nice API clowns
	}

	TessApi->Clear();

	#if AUG_DEBUG_OCR
	cv::imshow("imshow", DebugMat);
	#endif
}

IImageToText* IImageToText::CreateInstance()
{
	return new TesseractImageToText();
}
