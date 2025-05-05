#include "Assistant.h"

#include <BS_thread_pool.hpp>
#include <curl/curl.h>

#pragma warning(push, 1)
// GENERATES OVER 9000 WARNINGS, PROB UNIX STYLE :D
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#pragma warning(pop)

class OpenAIAssistant : public IAssistant
{
public:

	virtual ~OpenAIAssistant() override { Release(); }
	virtual bool Init(std::string Url, std::string Token, ResultCallback Callback) override;
	virtual void Release() override;
	virtual void Process(std::vector<Message> Messages) override;
	virtual void CancelCurrentRequest() override;
	virtual void CancelAllRequests() override;

private:

	std::string GenerateJson(const std::vector<Message>& Messages);
	void SendRequest(const std::string& RequestData, Result& Res);

	struct CurlContext { OpenAIAssistant* Solver; Result* Res; std::string Message; };
	size_t WriteCallback(char *ptr, size_t size, size_t nmemb, CurlContext* context);
	static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, CurlContext* userdata)
	{
		return userdata->Solver->WriteCallback(ptr, size, nmemb, userdata);
	}

	int ProgressCallback(CurlContext* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow);
	static int CurlProgressCallback(CurlContext* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
	{
		return clientp->Solver->ProgressCallback(clientp, dltotal, dlnow, ultotal, ulnow);
	}

private:

	std::string BackendUrl;
	std::string BackendToken;
	ResultCallback ResCallback;

	std::atomic<int> CancelRequestFlag;

	std::unique_ptr<CURL, decltype([] (CURL* ptr) { curl_easy_cleanup(ptr); })> Curl;
	std::unique_ptr<curl_slist, decltype([] (curl_slist* ptr) { curl_slist_free_all(ptr); })> Headers;

	std::unique_ptr<BS::thread_pool<BS::tp::none>> ThreadPool;
};

bool OpenAIAssistant::Init(std::string Url, std::string Token, ResultCallback Callback)
{
	do
	{
		BackendUrl = std::move(Url);
		BackendToken = std::move(Token);
		ResCallback = std::move(Callback);

		GUARD_BREAK(!BackendUrl.empty(), "Invalid backend url");

		curl_slist* TmpHeaders = nullptr;
		TmpHeaders = curl_slist_append(TmpHeaders, "Content-Type: application/json");
		TmpHeaders = curl_slist_append(TmpHeaders,  "Accept: text/event-stream");
		if (!BackendToken.empty())
		{
			std::string AuthString("Authorization: Bearer ");
			AuthString.append(BackendToken);
			TmpHeaders = curl_slist_append(TmpHeaders, AuthString.c_str());
		}

		GUARD_BREAK(TmpHeaders, "curl_slist_append");
		Headers.reset(TmpHeaders);

		Curl.reset(curl_easy_init());
		GUARD_BREAK(Curl.get(), "curl_easy_init");

		ThreadPool.reset(new BS::thread_pool(1)); // don't change

		return true;
	}
	while (0);

	Release();
	return false;
}

void OpenAIAssistant::Release()
{
	if (ThreadPool) { logi("OpenAIAssistant::Release"); }

	ResCallback = {};
	ThreadPool.reset();
	Headers.reset();
	Curl.reset();
}

void OpenAIAssistant::Process(std::vector<Message> Messages)
{
	if (Messages.empty())
		return;

	if (!ThreadPool || !ResCallback)
		return;

	CancelRequestFlag = 0;

	auto Fut = ThreadPool->submit_task([this, Messages = std::move(Messages)] ()
	{
		Result Res {};
		SendRequest(GenerateJson(Messages), Res);

		if (ResCallback)
			ResCallback(Res);
	});
}

void OpenAIAssistant::CancelCurrentRequest()
{
	CancelRequestFlag = 1;
}

void OpenAIAssistant::CancelAllRequests()
{
	CancelRequestFlag = 1;
	if (ThreadPool)
		ThreadPool->purge();
}

std::string OpenAIAssistant::GenerateJson(const std::vector<Message>& Messages)
{
	using namespace rapidjson;
	Document doc;
	doc.SetObject();
	Document::AllocatorType& allocator = doc.GetAllocator();

	//doc.AddMember("model", Value().SetString("gpt-3.5-turbo", allocator), allocator);
	//doc.AddMember("temperature", 0, allocator);
	//doc.AddMember("max_tokens", 1000, allocator);
	doc.AddMember("stream", true, allocator);

	Value jmessages(kArrayType);
	for (const auto& msg : Messages)
	{
		Value jmessage(kObjectType);
		jmessage.AddMember("role", Value().SetString(msg.Role.c_str(), allocator), allocator);
		jmessage.AddMember("content", Value().SetString(msg.Content.c_str(), allocator), allocator);
		jmessages.PushBack(jmessage, allocator);
	}
	doc.AddMember("messages", jmessages, allocator);

	StringBuffer buffer;
	Writer<StringBuffer> writer(buffer);
	doc.Accept(writer);

	return buffer.GetString();
}

void OpenAIAssistant::SendRequest(const std::string& RequestData, Result& Res)
{
	CurlContext context {};
	context.Solver = this;
	context.Res = &Res;
	context.Message.reserve(1024);

	curl_easy_reset(Curl.get());

	curl_easy_setopt(Curl.get(), CURLOPT_XFERINFODATA, &context);
	curl_easy_setopt(Curl.get(), CURLOPT_XFERINFOFUNCTION, CurlProgressCallback);
	curl_easy_setopt(Curl.get(), CURLOPT_NOPROGRESS, 0L);

	curl_easy_setopt(Curl.get(), CURLOPT_WRITEDATA, &context);
	curl_easy_setopt(Curl.get(), CURLOPT_WRITEFUNCTION, CurlWriteCallback);

	curl_easy_setopt(Curl.get(), CURLOPT_URL, BackendUrl.c_str());
	curl_easy_setopt(Curl.get(), CURLOPT_HTTPHEADER, Headers.get());
	curl_easy_setopt(Curl.get(), CURLOPT_POSTFIELDS, RequestData.c_str());
	curl_easy_setopt(Curl.get(), CURLOPT_POST, 1L);

	CURLcode Error = curl_easy_perform(Curl.get()); // blocking
	if (Error != CURLE_OK)
	{
		loge("curl_easy_perform Error={}", (int)Error);
	}
}

int OpenAIAssistant::ProgressCallback(CurlContext* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow)
{
	return CancelRequestFlag.load() ? 1 : 0; // non-zero return cancels the transfer
}

size_t OpenAIAssistant::WriteCallback(char *ptr, size_t size, size_t nmemb, CurlContext* context)
{
	assert(context);
	assert(context->Res);

	Result& Res = *context->Res;

	const size_t TotalSize = size * nmemb;
	std::string str(ptr, TotalSize / sizeof(char));
	
	if (str.find("data: [DONE]") != std::string::npos) // end of event-stream
	{
		Res.Content = std::move(context->Message);
		Res.Partial = false;
		return TotalSize;
	}

	// clear "data:" prefix because rapidjson dont like it
	const size_t pos = str.find_first_of('{');
	if (pos != std::string::npos)
	{
		for (size_t i = 0; i < pos; ++i)
			str[i] = ' ';
	}

	using namespace rapidjson;
	Document doc;
	ParseResult pr = doc.Parse(str.c_str());
	if (!pr)
	{
		loge("Document::Parse Error={} Offset={}", (int)pr.Code(), pr.Offset());
		return TotalSize;
	}

	// HERE WE GO BRAINFUCK
	if (doc.HasMember("choices") && doc["choices"].IsArray())
	{
		const Value& choices = doc["choices"];
		//for (SizeType i = 0; i < choices.Size(); ++i)
		if (choices.Size() > 0)
		{
			const Value& choice = choices[0];
			if (choice.HasMember("delta")) // event-stream
			{
				const Value& delta = choice["delta"];
				if (delta.HasMember("content") && delta["content"].IsString())
				{
					context->Message.append(delta["content"].GetString()); // accumulate full message
					Res.Content.assign(delta["content"].GetString()); // pass delta to callback
					Res.Partial = true;
					ResCallback(Res);
				}
			}
			#if 1
			else if (choice.HasMember("message") && choice["message"].IsObject())
			{
				const Value& message = choice["message"];
				if (message.HasMember("role") && message["role"].IsString()
					&& message.HasMember("content") && message["content"].IsString())
				{
					// message["role"].GetString()
					context->Message.assign(message["content"].GetString());
				}
			}
			#endif
		}
	}

	return TotalSize;
}

IAssistant* IAssistant::CreateInstance()
{
	return new OpenAIAssistant();
}
