#pragma once

#include "AUGCore.h"
#include <unordered_map>

#define INI_SERIALIZE_PROP(Sec, Key) if (Save) { Config.set(Sec, #Key, Key); } else { Config.get(Sec, #Key, Key); }

struct IniSection
{
	std::unordered_map<std::string, std::string> keys;
};

struct IniFile
{
	std::string dataFilename;
	std::string data;
	std::unordered_map<std::string, IniSection> sections;
	bool ready = false;

	IniFile();
	~IniFile();

	IniFile(const std::string& filename);
	bool load(const std::string& filename);
	bool save(const std::string& filename) const;

	bool hasSection(const std::string& section) const;
	bool hasKey(const std::string& section, const std::string& key) const;

	const IniSection& getSection(const std::string& section) const;
	const std::string& getString(const std::string& section, const std::string& key) const;

	// LOL
	bool get(const std::string& section, const std::string& key, std::string& output) const;
	bool get(const std::string& section, const std::string& key, bool& output) const;
	bool get(const std::string& section, const std::string& key, int& output) const;
	bool get(const std::string& section, const std::string& key, float& output) const;
	bool get(const std::string& section, const std::string& key, float2& output) const;
	bool get(const std::string& section, const std::string& key, float3& output) const;
	bool get(const std::string& section, const std::string& key, float4& output) const;

	void set(const std::string& section, const std::string& key, const char* value);
	void set(const std::string& section, const std::string& key, const std::string_view& value);
	void set(const std::string& section, const std::string& key, const bool value);
	void set(const std::string& section, const std::string& key, const int value);
	void set(const std::string& section, const std::string& key, const float value);
	void set(const std::string& section, const std::string& key, const float2& value);
	void set(const std::string& section, const std::string& key, const float3& value);
	void set(const std::string& section, const std::string& key, const float4& value);
};
