#pragma once
// ============================================================================
// 进度条信息(ProgressInfo)
// ============================================================================
#include "common_export.h"
#include <string>
#include <cstdint>

namespace common_tools
{
	/** @brief 进度条显示信息结构体，用于界面进度控件的数据传递 */
	struct COMMONTOOLS_API ProgressInfo
	{
		bool show_ctrl = false; // 是否显示控件
		bool show_percent = false; // 是否显示百分比
		uint32_t min_value = 0; // 进度最小值
		uint32_t max_value = 100; // 进度最大值
		uint32_t current_value = 0; // 当前进度值
		float percent_value = 0.0f; // 当前百分比（0~100）
		std::string title_text; // 标题文本
		std::string tips_text; // 提示文本
	};
}
