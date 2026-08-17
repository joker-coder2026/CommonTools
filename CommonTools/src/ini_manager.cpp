#include "ini_manager.h"

#include <windows.h>
#include <fstream>
#include <cctype>
#include <cstring>

namespace common_tools
{
	namespace
	{
		bool ContainsDriveLetter(const std::string& path)
		{
			if (path.length() >= 2 && std::isalpha(path[0]) && path[1] == ':')
			{
				return true; // 例如 "C:"
			}
			return false;
		}

		bool ContainsPathSeparator(const std::string& path)
		{
			if (path.find('\\') != std::string::npos)
			{
				return true;
			}
			if (path.find('/') != std::string::npos)
			{
				return true;
			}
			return false;
		}

		std::string GetExecutablePath()
		{
			char buffer[MAX_PATH];
			::GetModuleFileName(nullptr, buffer, MAX_PATH);

			std::string path(buffer);
			size_t lastPos = path.find_last_of("\\/");
			if (lastPos != std::string::npos)
			{
				path = path.substr(0, lastPos);
			}
			return path;
		}
	}

	IniManager::IniManager(const std::string& file_path)
	{
		set_last_error("");
		file_path_ = "";

		if (ContainsDriveLetter(file_path) && ContainsPathSeparator(file_path))
		{
			file_path_ = file_path;
		}
		else
		{
			std::string current_path = GetExecutablePath();
			file_path_ = current_path + "\\" + file_path;
		}
	}

	bool IniManager::write_value(const std::string& section, const std::string& key, const std::string& value)
	{
		set_last_error("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), key.c_str(), value.c_str(), file_path_.c_str());
		if (!result)
		{
			set_last_error("Failed to write value to INI file");
		}
		return result != 0;
	}

	std::string IniManager::read_value(const std::string& section, const std::string& key,
	                                  const std::string& default_value)
	{
		set_last_error("");

		char buffer[256] = {0};
		DWORD size = ::GetPrivateProfileString(section.c_str(), key.c_str(), default_value.c_str(), buffer,
		                                       sizeof(buffer), file_path_.c_str());
		if (size == 0 && !section_exists(section))
		{
			set_last_error("Section not found in INI file");
		}
		return buffer;
	}

	bool IniManager::write_int(const std::string& section, const std::string& key, int value)
	{
		return write_value(section, key, to_string(value));
	}

	bool IniManager::write_bool(const std::string& section, const std::string& key, bool value)
	{
		return write_value(section, key, to_string(value));
	}

	bool IniManager::write_double(const std::string& section, const std::string& key, double value)
	{
		return write_value(section, key, to_string(value));
	}

	int IniManager::read_int(const std::string& section, const std::string& key, int default_value)
	{
		return from_string(read_value(section, key, ""), default_value);
	}

	bool IniManager::read_bool(const std::string& section, const std::string& key, bool default_value)
	{
		return from_string(read_value(section, key, ""), default_value);
	}

	double IniManager::read_double(const std::string& section, const std::string& key, double default_value)
	{
		return from_string(read_value(section, key, ""), default_value);
	}

	std::map<std::string, std::string> IniManager::ReadSection(const std::string& section)
	{
		set_last_error("");

		std::map<std::string, std::string> result;

		if (!section_exists(section))
		{
			set_last_error("Section not found in INI file");
			return result;
		}

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSection(section.c_str(), buffer, sizeof(buffer), file_path_.c_str());

		if (size == 0)
			return result;

		char* p = buffer;
		while (*p)
		{
			std::string line(p);
			size_t pos = line.find('=');
			if (pos != std::string::npos)
			{
				std::string key = line.substr(0, pos);
				std::string value = line.substr(pos + 1);
				result[key] = value;
			}
			p += strlen(p) + 1;
		}

		return result;
	}

	bool IniManager::write_section(const std::string& section, const std::map<std::string, std::string>& keyValues)
	{
		set_last_error("");

		// 先删除现有 section
		if (!delete_section(section))
		{
			set_last_error("Failed to clear existing section");
			return false;
		}

		// 写入新的键值对
		for (auto it = keyValues.begin(); it != keyValues.end(); ++it)
		{
			if (!write_value(section, it->first, it->second))
			{
				set_last_error("Failed to write key-value pair");
				return false;
			}
		}

		return true;
	}

	bool IniManager::delete_key(const std::string& section, const std::string& key)
	{
		set_last_error("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), key.c_str(), nullptr, file_path_.c_str());
		if (!result)
		{
			set_last_error("Failed to delete key from INI file");
		}
		return result != 0;
	}

	bool IniManager::delete_section(const std::string& section)
	{
		set_last_error("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), nullptr, nullptr, file_path_.c_str());
		if (!result)
		{
			set_last_error("Failed to delete section from INI file");
		}
		return result != 0;
	}

	bool IniManager::file_exists()
	{
		set_last_error("");

		std::ifstream file(file_path_);
		bool exist = file.good();
		if (file.is_open())
			file.close();
		return exist;
	}

	bool IniManager::backup_file(const std::string& backupPath)
	{
		set_last_error("");

		std::string file_path(file_path_);

		std::string actual_backup_path = backupPath;
		if (actual_backup_path.empty())
		{
			size_t pos = file_path.rfind('.');
			if (pos != std::string::npos)
			{
				actual_backup_path = file_path.substr(0, pos) + ".bak" + file_path.substr(pos);
			}
			else
			{
				actual_backup_path = file_path + ".bak";
			}
		}

		BOOL result = ::CopyFile(file_path.c_str(), actual_backup_path.c_str(), FALSE); // 覆盖已存在的文件
		if (!result)
		{
			set_last_error("Failed to backup INI file");
		}

		return result != 0;
	}

	bool IniManager::section_exists(const std::string& section)
	{
		set_last_error("");

		std::vector<std::string> sections = get_section_names();
		return std::find(sections.begin(), sections.end(), section) != sections.end();
	}

	bool IniManager::key_exists(const std::string& section, const std::string& key)
	{
		set_last_error("");

		std::vector<std::string> keys = get_key_names(section);
		return std::find(keys.begin(), keys.end(), key) != keys.end();
	}

	std::vector<std::string> IniManager::get_section_names()
	{
		set_last_error("");

		std::vector<std::string> sections;

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSectionNames(buffer, sizeof(buffer), file_path_.c_str());

		if (size == 0)
			return sections;

		char* p = buffer;
		while (*p)
		{
			sections.push_back(std::string(p));
			p += strlen(p) + 1;
		}

		return sections;
	}

	std::vector<std::string> IniManager::get_key_names(const std::string& section)
	{
		set_last_error("");

		std::vector<std::string> keys;

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSection(section.c_str(), buffer, sizeof(buffer), file_path_.c_str());

		if (size == 0)
			return keys;

		char* p = buffer;
		while (*p)
		{
			std::string line(p);
			size_t pos = line.find('=');
			if (pos != std::string::npos)
			{
				keys.push_back(line.substr(0, pos));
			}
			p += strlen(p) + 1;
		}

		return keys;
	}

	std::string IniManager::get_last_error()
	{
		return last_error_;
	}

	void IniManager::set_last_error(const std::string& error)
	{
		last_error_ = error;
	}
}
