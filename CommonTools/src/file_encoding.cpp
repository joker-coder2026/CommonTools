#include "file_encoding.h"

#include <fstream>
#include <iterator>
#include <cstdint>

namespace file_encoding
{
	Encoding get(const std::string& file_path)
	{
		std::ifstream file(file_path, std::ios::binary);
		if (!file)
		{
			return Encoding::UNKNOWN;
		}

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();

		size_t len = content.size();
		auto data = (const uint8_t*)content.data();

		// BOM 判断
		if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
			return Encoding::UTF8_BOM;
		if (len >= 2 && data[0] == 0xFF && data[1] == 0xFE)
			return Encoding::UTF16_LE;
		if (len >= 2 && data[0] == 0xFE && data[1] == 0xFF)
			return Encoding::UTF16_BE;

		// 判断是否是 UTF-8(能区分中文GBK 和 中文UTF8)，如果是纯英文内容则判定为UTF-8
		bool is_utf8 = true;
		size_t i = 0;
		while (i < len)
		{
			// 单字节 0~127：英文，UTF8/GBK 通用
			if (data[i] <= 0x7F)
			{
				i++;
			}
			// 双字节 UTF8
			else if ((data[i] & 0xE0) == 0xC0)
			{
				if (i + 1 >= len || (data[i + 1] & 0xC0) != 0x80)
					is_utf8 = false;
				i += 2;
			}
			// 三字节 UTF8(中文主要在这里)
			else if ((data[i] & 0xF0) == 0xE0)
			{
				if (i + 2 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80)
					is_utf8 = false;
				i += 3;
			}
			// 四字节 UTF8
			else if ((data[i] & 0xF8) == 0xF0)
			{
				if (i + 3 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0)
					!= 0x80)
					is_utf8 = false;
				i += 4;
			}
			// 不符合 UTF8 规则 → 一定是 GBK
			else
			{
				is_utf8 = false;
				i++;
			}
		}

		if (is_utf8)
		{
			return Encoding::UTF8;
		}
		return Encoding::GBK;
	}

	void set(const Encoding& encoding)
	{
		// 内容转换
	}
}
