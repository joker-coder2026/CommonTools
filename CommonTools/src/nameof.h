#pragma once
// ============================================================================
// 获取名称/获取类型(nameof_detail) + NAMEOF/TYPEOF 宏
// ============================================================================
#include "common_export.h"
#include <cstring>
#include <typeinfo>

namespace nameof_detail
{
	// 获取变量名
	inline const char* get_name(const char* str)
	{
		const char* name = str;
		for (; *str; ++str)
		{
			if (*str == '.' || *str == ':' || *str == ' ')
				name = str + 1;
		}
		return name;
	}

	// 获取类型名
	template <typename T>
	const char* get_type(const T&)
	{
		const char* name = typeid(T).name();
		// STD类型处理
		if (strstr(name, "basic_string"))
			return "string";
		if (strstr(name, "vector"))
			return "vector";
		if (strstr(name, "unordered_map"))
			return "unordered_map";

		// 自定义类型处理
		if (strstr(name, "struct ") == name)
			name += 7;
		if (strstr(name, "class ") == name)
			name += 6;

		return get_name(name);
	}
}


#define NAMEOF(...) nameof_detail::get_name(#__VA_ARGS__)
#define TYPEOF(val) nameof_detail::get_type(val)
