#pragma once

#include "IniFile.h"

class ISpeechToText
{
public:

	static ISpeechToText* CreateInstance();

	struct Result
	{
		std::vector<std::string> Segments;
		std::string Text;

		AUG_MOVABLE_NONCOPYABLE(Result);
	};

	using ResultCallback = std::function<void(Result&)>;

	virtual ~ISpeechToText() {}
	virtual bool Init(ResultCallback Callback) = 0;
	virtual void Release() = 0;
	virtual void Serialize(IniFile& Config, bool Save) = 0;
	virtual void RenderUI() = 0;
};
