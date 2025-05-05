#pragma once

#include "AUGCore.h"

class IAssistant
{
public:

	static IAssistant* CreateInstance();

	struct Message
	{
		std::string Role;
		std::string Content;
	};

	struct Result
	{
		std::string Content;
		bool Partial = false;

		AUG_MOVABLE_NONCOPYABLE(Result);
	};

	using ResultCallback = std::function<void(Result&)>;

	virtual ~IAssistant() {}
	virtual bool Init(std::string Url, std::string Token, ResultCallback Callback) = 0;
	virtual void Release() = 0;
	virtual void Process(std::vector<Message> Messages) = 0;
	virtual void CancelCurrentRequest() = 0;
	virtual void CancelAllRequests() = 0;
};
