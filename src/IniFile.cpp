#include "IniFile.h"
#include <sstream>
#include <fstream>

static const IniSection _EmptySection;
static const std::string _EmptyString;
static const std::string _ArraySeparator(",");

IniFile::IniFile()
{
}

IniFile::~IniFile()
{
}

IniFile::IniFile(const std::string& filename)
{
	load(filename);
}

bool IniFile::load(const std::string& filename)
{
	if (ready && (dataFilename == filename))
	{
		return true;
	}

	ready = false;
	dataFilename = filename;
	data.clear();
	sections.clear();

	{
		std::ifstream fs(filename);
		if (!fs.is_open())
		{
			return false;
		}

		std::stringstream ss;
		ss << fs.rdbuf();
		data = ss.str();
	}

	std::istringstream ss(data);
	std::string line;
	std::string secName;
	IniSection* sec = nullptr;

	while (std::getline(ss, line))
	{
		auto comm = line.find(';');
		auto s1 = line.find('[');
		if (s1 != std::string::npos) // [section_name] ; comment
		{
			s1++;
			auto s2 = line.find(']');
			if (s2 != std::string::npos && s2 > s1 && s2 < comm)
			{
				secName = line.substr(s1, s2 - s1);
				auto result = sections.insert({secName, IniSection()});
				sec = &(result.first->second);
			}
		}
		else if (!secName.empty()) // key=value ; comment
		{
			auto p1 = line.find('=');
			if (p1 != std::string::npos && p1 > 0 && p1 < comm)
			{
				auto p2 = line.find_first_of(";\n", p1);
				if (p2 != std::string::npos)
					p2 -= p1;

				auto key = line.substr(0, p1);
				auto val = line.substr(p1 + 1, p2);
				sec->keys.insert({key, val});
			}
		}
	}

	ready = true;
	return true;
}

bool IniFile::save(const std::string& filename) const
{
	std::ofstream fs(filename);
	if (!fs.is_open())
	{
		return false;
	}

	for (const auto& secIter : sections)
	{
		if (!secIter.second.keys.empty())
		{
			fs << "[" << secIter.first << "]\n";
			for (const auto& kvIter : secIter.second.keys)
			{
				fs << kvIter.first << "=" << kvIter.second << "\n";
			}
			fs << "\n";
		}
	}

	fs.flush();
	return true;
}

bool IniFile::hasSection(const std::string& section) const
{
	return sections.find(section) != sections.end();
}

bool IniFile::hasKey(const std::string& section, const std::string& key) const
{
	auto isec = sections.find(section);
	if (isec != sections.end())
	{
		const auto& keys = isec->second.keys;
		auto ikey = keys.find(key);
		return (ikey != keys.end());
	}
	return false;
}

const IniSection& IniFile::getSection(const std::string& section) const
{
	auto isec = sections.find(section);
	if (isec != sections.end())
	{
		return isec->second;
	}
	return _EmptySection;
}

const std::string& IniFile::getString(const std::string& section, const std::string& key) const
{
	auto isec = sections.find(section);
	if (isec != sections.end())
	{
		const auto& keys = isec->second.keys;
		auto ikey = keys.find(key);
		if (ikey != keys.end())
			return ikey->second;
	}
	return _EmptyString;
}

// GET VALUE

bool IniFile::get(const std::string& section, const std::string& key, std::string& output) const
{
	auto isec = sections.find(section);
	if (isec != sections.end())
	{
		const auto& keys = isec->second.keys;
		auto ikey = keys.find(key);
		if (ikey != keys.end())
		{
			output = ikey->second;
			return true;
		}
	}
	return false;
}

bool IniFile::get(const std::string& section, const std::string& key, bool& output) const
{
	int val = 0;
	if (get(section, key, val))
	{
		output = (val != 0);
		return true;
	}
	return false;
}

bool IniFile::get(const std::string& section, const std::string& key, int& output) const
{
	const auto& s = getString(section, key);
	if (!s.empty())
	{
		output = std::stoi(s);
		return true;
	}
	return false;
}

bool IniFile::get(const std::string& section, const std::string& key, float& output) const
{
	const auto& s = getString(section, key);
	if (!s.empty())
	{
		output = std::stof(s);
		return true;
	}
	return false;
}

bool IniFile::get(const std::string& section, const std::string& key, float2& output) const
{
	const auto& s = getString(section, key);
	auto v = split(s, _ArraySeparator);
	if (v.size() == 2)
	{
		output = {std::stof(v[0]), std::stof(v[1])};
		return true;
	}
	return false;
}

bool IniFile::get(const std::string& section, const std::string& key, float3& output) const
{
	const auto& s = getString(section, key);
	auto v = split(s, _ArraySeparator);
	if (v.size() == 3)
	{
		output = {std::stof(v[0]), std::stof(v[1]), std::stof(v[2])};
		return true;
	}
	return false;
}

bool IniFile::get(const std::string& section, const std::string& key, float4& output) const
{
	const auto& s = getString(section, key);
	auto v = split(s, _ArraySeparator);
	if (v.size() == 4)
	{
		output = {std::stof(v[0]), std::stof(v[1]), std::stof(v[2]), std::stof(v[3])};
	}
	return false;
}

// SET VALUE

void IniFile::set(const std::string& section, const std::string& key, const char* value)
{
	auto& sec = sections[section];
	sec.keys[key] = value;
}

void IniFile::set(const std::string& section, const std::string& key, const std::string_view& value)
{
	auto& sec = sections[section];
	sec.keys[key] = value;
}

void IniFile::set(const std::string& section, const std::string& key, const bool value)
{
	set(section, key, (int)(value ? 1 : 0));
}

void IniFile::set(const std::string& section, const std::string& key, const int value)
{
	set(section, key, strf("%d", value));
}

void IniFile::set(const std::string& section, const std::string& key, const float value)
{
	set(section, key, strf("%f", value));
}

void IniFile::set(const std::string& section, const std::string& key, const float2& value)
{
	set(section, key, strf("%f,%f", value.x, value.y));
}

void IniFile::set(const std::string& section, const std::string& key, const float3& value)
{
	set(section, key, strf("%f,%f,%f", value.x, value.y, value.z));
}

void IniFile::set(const std::string& section, const std::string& key, const float4& value)
{
	set(section, key, strf("%f,%f,%f,%f", value.x, value.y, value.z, value.w));
}
