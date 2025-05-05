#pragma once

#include "AUGCore.h"

class IImageToText
{
public:

	static IImageToText* CreateInstance();

	struct Detection
	{
		IntRect Rect;
		float Confidence;
		std::string Text;
	};

	struct Result
	{
		std::vector<Detection> Detections;
		std::string Text;

		AUG_MOVABLE_NONCOPYABLE(Result);
	};

	using ResultCallback = std::function<void(Result&)>;

	virtual ~IImageToText() {}
	virtual bool Init(ResultCallback Callback) = 0;
	virtual void Release() = 0;
	virtual void Process(IntRect Region = {}) = 0;
};
