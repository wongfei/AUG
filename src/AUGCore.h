#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstdarg>

#include <memory>
#include <string>
#include <vector>

#include <algorithm>
#include <functional>

#include "log.h"

#define GUARD_BREAK(expr, msg) if (!(expr)) { loge(msg); break; }
#define GUARD_HR_BREAK(hr, msg) if (FAILED((hr))) { loge(msg); break; }
#define GUARD_THROW(expr, msg) if (!(expr)) { throw std::exception(msg); }

#define AUG_CONCAT(a, b) AUG_CONCAT_INNER(a, b)
#define AUG_CONCAT_INNER(a, b) a ## b
#define AUG_UNIQUE_NAME(base) AUG_CONCAT(base, __LINE__)
#define AUG_PERF(name) LameProfiler AUG_UNIQUE_NAME(_perf_) (name)

#define AUG_DEF_COPY(ClassName)\
	ClassName(const ClassName&) = default;\
	ClassName& operator=(const ClassName&) = default

#define AUG_DEF_MOVE(ClassName)\
	ClassName(ClassName&&) noexcept = default;\
	ClassName& operator=(ClassName&&) noexcept = default

#define AUG_NO_COPY(ClassName)\
	ClassName(const ClassName&) = delete;\
	ClassName& operator=(const ClassName&) = delete

#define AUG_NO_MOVE(ClassName)\
	ClassName(ClassName&&) = delete;\
	ClassName& operator=(ClassName&&) = delete

#define AUG_MOVABLE_NONCOPYABLE(ClassName)\
	ClassName() {};\
	AUG_DEF_MOVE(ClassName);\
	AUG_NO_COPY(ClassName)

struct float2
{
	float x, y;
};

struct float3
{
	float x, y, z;
};

struct float4
{
	float x, y, z, w;
};

struct IntRect
{
	int Left, Top, Right, Bottom;

	inline int Width() const { return Right - Left; }
	inline int Height() const { return Bottom - Top; }
};

inline std::string strf_va(const char* format, va_list args)
{
	const size_t BufCount = 1024;
	char buf[BufCount];

	#ifdef WIN32
		int n = _vsnprintf_s(buf, BufCount, _TRUNCATE, format, args);
	#else
		int n = vsnprintf(buf, BufCount, format, args);
	#endif

	return std::string(buf, n);
}

inline std::string strf(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	auto str = strf_va(format, args);
	va_end(args);
	return str;
}

inline std::vector<std::string> split(const std::string& s, const std::string& delim)
{
	std::vector<std::string> res;
	size_t start = 0;
	for (;;)
	{
		size_t end = s.find(delim, start);
		if (end != std::string::npos)
		{
			res.emplace_back(s.substr(start, end - start));
			start = end + delim.length();
		}
		else
		{
			res.emplace_back(s.substr(start));
			break;
		}
	}
	return res;
}

struct LameProfiler
{
	const char* name;
	double threshold;
	std::chrono::high_resolution_clock::time_point clock0;

	inline LameProfiler(const char* in_name, double in_threshold = 0.001) { 
		name = in_name;
		threshold = in_threshold;
		clock0 = std::chrono::high_resolution_clock::now(); 
	}

	inline ~LameProfiler() { 
		const auto clock1 = std::chrono::high_resolution_clock::now(); 
		const auto clock_dur = std::chrono::duration_cast<std::chrono::microseconds>(clock1 - clock0);
		const double elapsed = clock_dur.count() * 0.001; // to milliseconds
		if (elapsed > threshold)
		{
			logi("PERF # {} {} ms", name, elapsed);
		}
	}
};
