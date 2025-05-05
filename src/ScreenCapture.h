#pragma once

#include "AUGCore.h"
#include <opencv2/core/mat.hpp>

class IScreenCapture
{
public:

	static IScreenCapture* CreateInstance();

	virtual ~IScreenCapture() {}
	virtual bool Init() = 0;
	virtual void Release() = 0;
	virtual bool Capture() = 0;
	virtual cv::Mat& GetImage() = 0;
};
