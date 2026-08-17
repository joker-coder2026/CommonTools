#pragma once
// ============================================================================
// 高精度时间戳(timestamp)
// ============================================================================
#include "common_export.h"
#include <cstdint>

namespace timestamp
{
	/**
	 * @brief 获取当前时间戳（微秒级）
	 * @return 自 Unix 纪元以来的微秒数
	 **/
	int64_t COMMONTOOLS_API get_current_time_us();

	/**
	 * @brief 获取当前时间戳（毫秒级）
	 * @return 自 Unix 纪元以来的毫秒数
	 **/
	int64_t COMMONTOOLS_API get_current_time_ms();

	/**
	 * @brief 获取当前时间戳（秒级）
	 * @return 自 Unix 纪元以来的秒数
	 **/
	int64_t COMMONTOOLS_API get_current_time_ss();

	/**
	 * @brief 计算微秒级时间间隔
	 * @param [in] start_us - 开始记录的微秒时间戳（由 get_current_time_us 获得）
	 * @return 从 start_us 到当前经过的微秒数（不小于 0）
	 **/
	int64_t COMMONTOOLS_API get_interval_time_us(int64_t start_us);

	/**
	 * @brief 计算毫秒级时间间隔
	 * @param [in] start_ms - 开始记录的毫秒时间戳（由 get_current_time_ms 获得）
	 * @return 从 start_ms 到当前经过的毫秒数（不小于 0）
	 **/
	int64_t COMMONTOOLS_API get_interval_time_ms(int64_t start_ms);

	/**
	 * @brief 计算秒级时间间隔
	 * @param [in] start_ss - 开始记录的秒时间戳（由 get_current_time_ss 获得）
	 * @return 从 start_ss 到当前经过的秒数（不小于 0）
	 **/
	int64_t COMMONTOOLS_API get_interval_time_ss(int64_t start_ss);
}
