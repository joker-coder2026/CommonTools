#include "timestamp.h"

#include <chrono>
#include <algorithm>

namespace timestamp
{
	int64_t get_current_time_us()
	{
		auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::microseconds>(now).count();
	}

	int64_t get_current_time_ms()
	{
		auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
	}

	int64_t get_current_time_ss()
	{
		auto now = std::chrono::high_resolution_clock::now().time_since_epoch();
		return std::chrono::duration_cast<std::chrono::seconds>(now).count();
	}

	int64_t get_interval_time_us(int64_t start_us)
	{
		return std::max<int64_t>(0, get_current_time_us() - start_us);
	}

	int64_t get_interval_time_ms(int64_t start_ms)
	{
		return std::max<int64_t>(0, get_current_time_ms() - start_ms);
	}

	int64_t get_interval_time_ss(int64_t start_ss)
	{
		return std::max<int64_t>(0, get_current_time_ss() - start_ss);
	}
}
