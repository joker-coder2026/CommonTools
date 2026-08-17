#include "bit_tools.h"

namespace bit32_tools
{
	void set(int& value, int bit_idx, bool bit_value)
	{
		if (bit_idx < 0 || bit_idx >= 32)
			return;

		bit_value ? (value |= (1U << bit_idx)) : (value &= ~(1U << bit_idx));
		// 	if (bit_value)
		// 		value |= (1 << bit_idx); // 置1:用位或操作
		// 	else
		// 		value &= ~(1 << bit_idx); // 置0:用位或操作，再取反
	}

	bool get(int value, int bit_idx)
	{
		if (bit_idx < 0 || bit_idx >= 32)
			return false;

		return (value >> bit_idx) & 1; // 提取指定位：先右移，再与1做与运算
	}
}
