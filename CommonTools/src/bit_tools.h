#pragma once
// ============================================================================
// 基础数据转换位数据(bit32_tools)：32位位操作工具
// ============================================================================
#include "common_export.h"

namespace bit32_tools //位操作工具(32位)
{
	/**
	 * @brief 按不同位存储至一个int32变量
	 * @param value 储存变量
	 * @param bit_idx 位索引
	 * @param bit_value 位索引对应的值
	*/
	void COMMONTOOLS_API set(int& value, int bit_idx, bool bit_value);

	/**
	 * @brief 从一个int32变量中获取不同位储存的值
	 * @param value 储存变量
	 * @param bit_idx 位索引
	 * @return 位索引对应的值
	*/
	bool COMMONTOOLS_API get(int value, int bit_idx);
}
