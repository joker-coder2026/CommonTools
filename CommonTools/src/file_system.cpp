#include "file_system.h"

#include <sys/stat.h>
#include <direct.h>   // for _mkdir, _rmdir
#include <corecrt_io.h> // for _access
#include <windows.h>
#include <tchar.h>

#include <fstream>
#include <iterator>
#include <cstdio>
#include <cstring>
#include <cerrno>

namespace file_system
{
	bool exists(const std::string& path)
	{
		return _access(path.c_str(), 0) == 0;
	}

	bool is_file(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
		{
			return false;
		}
		return (info.st_mode & S_IFMT) == S_IFREG;
	}

	bool create_file(const std::string& path)
	{
		std::ofstream file(path);
		if (!file)
		{
			return false;
		}
		file.close();
		return true;
	}

	bool rename_file(const std::string& src_path, const std::string& dst_path)
	{
		std::ifstream file(src_path);
		if (!file)
		{
			return false;
		}
		file.close();

		if (std::rename(src_path.c_str(), dst_path.c_str()) != 0)
		{
			if (::MoveFileEx(src_path.c_str(), dst_path.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
			{
				return false;
			}
		}
		return true;
	}

	bool copy_file(const std::string& src_path, const std::string& dst_path)
	{
		if (!is_file(src_path))
		{
			return false;
		}

		std::ifstream in(src_path, std::ios::binary);
		if (!in.is_open())
		{
			return false;
		}

		std::ofstream out(dst_path, std::ios::binary);
		if (!out.is_open())
		{
			in.close();
			return false;
		}

		out << in.rdbuf();

		in.close();
		out.close();
		return true;
	}

	bool move_file(const std::string& src_path, const std::string& dst_path)
	{
		if (!is_file(src_path))
		{
			return false;
		}

		if (rename(src_path.c_str(), dst_path.c_str()) != 0)
		{
			if (::MoveFileEx(src_path.c_str(), dst_path.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
			{
				return false;
			}
		}
		return true;
	}

	bool delete_file(const std::string& path)
	{
		if (!is_file(path))
		{
			return false;
		}
		return ::remove(path.c_str()) == 0;
	}

	std::time_t get_file_create_time(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
		{
			return 0;
		}
		return info.st_ctime;
	}

	std::time_t get_file_modified_time(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
		{
			return 0;
		}
		return info.st_mtime;
	}

	std::string get_file_extension_name(const std::string& path)
	{
		size_t pos = path.find_last_of('.');
		if (pos == std::string::npos)
		{
			return "";
		}
		return path.substr(pos + 1);
	}

	std::string get_file_name(const std::string& path)
	{
		size_t pos = path.find_last_of("\\/");
		if (pos == std::string::npos)
		{
			return path;
		}
		return path.substr(pos + 1);
	}

	std::string get_file_path(const std::string& path)
	{
		size_t pos = path.find_last_of("\\/");
		if (pos == std::string::npos)
		{
			return "";
		}
		return path.substr(0, pos);
	}

	size_t get_file_size(const std::string& path)
	{
		if (!is_file(path))
		{
			return 0;
		}

		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in.is_open())
		{
			return 0;
		}

		std::streampos size = in.tellg();
		in.close();
		return size;
	}

	std::vector<std::string> get_files(const std::string& path, const std::string& extension)
	{
		std::vector<std::string> files;
		if (!is_directory(path))
			return files;

		WIN32_FIND_DATA find_data;
		HANDLE handle = ::FindFirstFile((path + "\\*").c_str(), &find_data);
		if (handle == INVALID_HANDLE_VALUE)
			return files;

		do
		{
			if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				std::string filename(find_data.cFileName);
				if (extension.empty() || filename.rfind(extension) == filename.length() - extension.length())
				{
					files.push_back(path + "\\" + filename);
				}
			}
		}
		while (::FindNextFile(handle, &find_data));
		FindClose(handle);

		return files;
	}

	bool is_directory(const std::string& path)
	{
		std::string temp(path);
		while (!temp.empty() && (temp.back() == '\\' || temp.back() == '/'))
		{
			temp.pop_back();
		}
		if (temp.length() == 2 && temp[1] == ':')
		{
			return false;
		}

		struct stat info;
		if (stat(temp.c_str(), &info) != 0)
		{
			return false;
		}
		return (info.st_mode & S_IFMT) == S_IFDIR;
	}

	bool create_directories(const std::string& path)
	{
		if (exists(path))
		{
			return is_directory(path);
		}

		// 递归创建目录
		size_t pos = 0;
		do
		{
			pos = path.find_first_of("\\/", pos + 1);
			std::string subdir = path.substr(0, pos);
			if (!exists(subdir) && _mkdir(subdir.c_str()) != 0 && errno != EEXIST)
			{
				return false;
			}
		}
		while (pos != std::string::npos);

		return true;
	}

	bool delete_directories(const std::string& path)
	{
		if (!is_directory(path))
		{
			return false;
		}

		std::string temp(path);
		while (!temp.empty() && (temp.back() == '\\' || temp.back() == '/'))
		{
			temp.pop_back();
		}
		if (temp.length() == 2 && temp[1] == ':')
		{
			return false;
		}

		TCHAR dir[MAX_PATH + 1] = {0};
		strcpy_s(dir, MAX_PATH, temp.c_str());
		strcat_s(dir, MAX_PATH, "\\*");

		WIN32_FIND_DATA find_data;
		HANDLE handle = ::FindFirstFile(dir, &find_data);

		if (handle == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		do
		{
			if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
			{
				continue;
			}
			TCHAR file_path[MAX_PATH] = {0};
			sprintf_s(file_path, MAX_PATH, "%s\\%s", temp.c_str(), find_data.cFileName);

			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!delete_directories(file_path))
				{
					FindClose(handle);
					return false;
				}
			}
			else
			{
				if (!delete_file(file_path))
				{
					FindClose(handle);
					return false;
				}
			}
		}
		while (::FindNextFile(handle, &find_data) != 0);

		FindClose(handle);

		return ::RemoveDirectory(path.c_str()) != 0;
	}

	std::vector<std::string> get_directories(const std::string& path)
	{
		std::vector<std::string> dirs;
		if (!is_directory(path))
		{
			return dirs;
		}

		WIN32_FIND_DATA find_data;
		HANDLE handle = ::FindFirstFile((path + "\\*").c_str(), &find_data);
		if (handle == INVALID_HANDLE_VALUE)
		{
			return dirs;
		}

		do
		{
			if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && strcmp(find_data.cFileName, ".") != 0 &&
				strcmp(find_data.cFileName, "..") != 0)
			{
				dirs.push_back(find_data.cFileName);
			}
		}
		while (::FindNextFile(handle, &find_data) != 0);

		FindClose(handle);

		return dirs;
	}

	std::string get_current_work_directory()
	{
		char buffer[MAX_PATH]{};
		::GetCurrentDirectory(MAX_PATH, buffer);
		return buffer;
	}

	bool set_current_work_directory(const std::string& path)
	{
		return ::SetCurrentDirectory(path.c_str()) != 0;
	}

	std::string read_all_text(const std::string& path)
	{
		if (!is_file(path))
		{
			return "";
		}

		std::ifstream in(path);
		if (!in.is_open())
		{
			return "";
		}

		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		in.close();

		return content;
	}

	bool write_all_text(const std::string& path, const std::string& text)
	{
		std::ofstream out(path);
		if (!out.is_open())
		{
			return false;
		}

		out << text;
		out.close();
		return true;
	}
}
