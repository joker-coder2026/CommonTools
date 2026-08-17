#include "logger.h"

#include "string_utils.h"
#include "file_system.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/common.h"

#include <windows.h>
#include <Shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

#include <cstdio>
#include <cstring>
#include <ctime>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <mutex>

namespace common_tools
{
	namespace
	{
		bool get_file_create_time(const std::string& file_path, time_t& create_time)
		{
			WIN32_FILE_ATTRIBUTE_DATA file_data = {0};
			if (!GetFileAttributesExA(file_path.c_str(), GetFileExInfoStandard, &file_data))
			{
				return false;
			}

			SYSTEMTIME sys_time = {0};
			if (!FileTimeToSystemTime(&file_data.ftCreationTime, &sys_time))
			{
				return false;
			}

			tm tm_time = {0};
			tm_time.tm_year = sys_time.wYear - 1900;
			tm_time.tm_mon = sys_time.wMonth - 1;
			tm_time.tm_mday = sys_time.wDay;
			tm_time.tm_hour = sys_time.wHour;
			tm_time.tm_min = sys_time.wMinute;
			tm_time.tm_sec = sys_time.wSecond;

			create_time = mktime(&tm_time);
			return create_time != -1;
		}

		void delete_expired_log_files(const std::string& dir_path, int days)
		{
			std::string search_path = dir_path + "\\*";
			WIN32_FIND_DATAA find_data = {0};
			HANDLE handle = FindFirstFileA(search_path.c_str(), &find_data);

			if (handle == INVALID_HANDLE_VALUE)
			{
				return;
			}

			do
			{
				std::string file_name = find_data.cFileName;
				if (file_name == "." || file_name == "..")
				{
					continue;
				}

				std::string full_path = dir_path + "\\" + file_name;

				if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				{
					delete_expired_log_files(full_path, days);

					if (PathIsDirectoryEmptyA(full_path.c_str()))
					{
						RemoveDirectoryA(full_path.c_str());
					}
				}
				else
				{
					time_t create_time = 0;
					if (!get_file_create_time(full_path, create_time))
					{
						continue;
					}

					// 判断是否过期
					time_t current_time = time(nullptr);
					time_t expireTime = create_time + static_cast<time_t>(days) * 24 * 60 * 60;
					if (current_time > expireTime)
					{
						DeleteFileA(full_path.c_str());
					}
				}
			}
			while (FindNextFileA(handle, &find_data));

			FindClose(handle);
		}
	}

	class Logger::CustomSink : public spdlog::sinks::base_sink<std::mutex>
	{
	public:
		explicit CustomSink(const std::function<void(const MetaMsg&)>& callback)
			: callback_(callback)
		{
		}

	protected:
		void sink_it_(const spdlog::details::log_msg& msg) override
		{
			if (callback_)
			{
				auto tt = std::chrono::system_clock::to_time_t(msg.time);
				//  		auto tm = *std::localtime(&tt); // by ht 20260325 release模式安全检查报错
				struct tm tm_buff;
				localtime_s(&tm_buff, &tt);

				char buf[64]{};
				std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buff);
				auto ms = std::chrono::duration_cast<std::chrono::microseconds>(msg.time.time_since_epoch()) % 1000;
				std::ostringstream oss_time;
				oss_time << buf << "." << std::setfill('0') << std::setw(3) << ms.count();

				MetaMsg meta_msg;
				meta_msg.level = static_cast<LogLevel>(msg.level);
				meta_msg.file = msg.source.filename ? msg.source.filename : "";
				meta_msg.line = msg.source.line;
				meta_msg.message = fmt::to_string(msg.payload);
				meta_msg.time = oss_time.str();
				callback_(meta_msg);
			}
		}

		void flush_() override
		{
			// 自定义sink无需flush
		}

	private:
		std::function<void(const MetaMsg&)> callback_; // 回调函数
	};


	Logger::Config::Config(const LogName& name)
		: log_name(name)
	{
	}

	Logger::Logger()
		: config_(LogName::DEFAULT),
		  is_initialized_(false)
	{
	}

	Logger::Logger(const LogName& name)
		: config_(name),
		  is_initialized_(false)
	{
	}

	Logger::Logger(const Config& config)
		: config_(config),
		  is_initialized_(false)
	{
	}

	Logger::~Logger()
	{
		shutdown();
	}

	Logger::Config& Logger::get_config()
	{
		return config_;
	}

	void Logger::set_output_callback(LogOutput type, const std::function<void(const MetaMsg&)>& callback)
	{
		custom_callbacks_[type] = callback;
	}

	void Logger::trace(const char* format, ...)
	{
		if (LogLevel::Trace < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Trace, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::debug(const char* format, ...)
	{
		if (LogLevel::Debug < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Debug, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::info(const char* format, ...)
	{
		if (LogLevel::Info < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Info, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::warn(const char* format, ...)
	{
		if (LogLevel::Warn < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Warn, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::error(const char* format, ...)
	{
		if (LogLevel::Error < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Error, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::critical(const char* format, ...)
	{
		if (LogLevel::Critical < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Critical, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::log_record(LogLevel level, const char* format, ...)
	{
		if (level < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(level, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::trace(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Trace < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Trace, file, line, function, buffer.get());
	}

	void Logger::debug(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Debug < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args));
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Debug, file, line, function, buffer.get());
	}

	void Logger::info(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Info < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Info, file, line, function, buffer.get());
	}

	void Logger::warn(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Warn < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Warn, file, line, function, buffer.get());
	}

	void Logger::error(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Error < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Error, file, line, function, buffer.get());
	}

	void Logger::critical(const char* file, int line, const char* function, const char* format, ...)
	{
		if (LogLevel::Critical < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(LogLevel::Critical, file, line, function, buffer.get());
	}

	void Logger::log_record(const char* file, const int line, const char* function, LogLevel level, const char* format,
	                       ...)
	{
		if (level < config_.level)
			return;

		va_list args;
		va_start(args, format);
		auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)); // by ht 20260325 防止Int类型溢出
		if (length < 0)
		{
			va_end(args);
			return;
		}
		std::unique_ptr<char[]> buffer(new char[length + 1]);
		if (!buffer)
		{
			va_end(args);
			return;
		}
		vsnprintf(buffer.get(), length + 1, format, args);
		va_end(args);

		log(level, file, line, function, buffer.get());
	}

	std::string Logger::format_log_message(LogOutput outputs, LogLevel level, const char* file, int line,
	                                     const char* function, const char* message)
	{
		std::stringstream thread_id;
		thread_id << std::this_thread::get_id();
		size_t tid = 0;
		thread_id >> tid;

		std::string level_name;
		if (level == LogLevel::InfoRed)
			level_name = "info";
		else if (level == LogLevel::Info)
			level_name = "info";
		else if (level == LogLevel::InfoGreen)
			level_name = "info";
		switch (level)
		{
		case LogLevel::Trace:
			level_name = "trace";
			break;
		case LogLevel::Debug:
			level_name = "debug";
			break;
		case LogLevel::Info:
			level_name = "info";
			break;
		case LogLevel::Warn:
			level_name = "warn";
			break;
		case LogLevel::Error:
			level_name = "error";
			break;
		case LogLevel::Critical:
			level_name = "critical";
			break;
		case LogLevel::InfoRed:
		case LogLevel::InfoGreen:
		case LogLevel::InfoBlack: default:
			level_name = "info";
			break;
		}

		std::string file_name;
		if (file)
			file_name = file;

		size_t pos = file_name.find_last_of("\\/");
		if (pos != std::string::npos)
			file_name = file_name.substr(pos + 1);

		std::string line_num;
		if (line > 0)
			line_num = std::to_string(line);

		std::string func_name;
		if (function)
			func_name = function;

		auto now = std::chrono::system_clock::now();
		auto tt = std::chrono::system_clock::to_time_t(now);
		// 	auto tm = *std::localtime(&tt); // by ht 20260325 release模式安全检查报错
		struct tm tm_buff;
		localtime_s(&tm_buff, &tt);

		char buf[64]{};
		std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm_buff);
		auto ms = std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()) % 1000;
		std::ostringstream oss_time;
		oss_time << buf << "." << std::setfill('0') << std::setw(3) << ms.count();

		bool first = true;
		for (auto& item : custom_callbacks_)
		{
			if ((static_cast<int>(outputs & item.first)) && item.second)
			{
				MetaMsg meta_msg;
				meta_msg.time = oss_time.str();
				meta_msg.logger_name = LogName::DEFAULT;
				meta_msg.thread_id = tid;
				meta_msg.level = level;
				meta_msg.output = item.first;
				meta_msg.file = file_name;
				meta_msg.line = line;
				meta_msg.func = func_name;
				meta_msg.message = message;

				item.second(meta_msg);

				if (first && config_.enable_database) // 一条消息
				{
					first = false;
					LogDbManager::get_instance().add_msg_to_queue(meta_msg);
				}
			}
		}

		std::string result = "[" + oss_time.str() + "] [" + thread_id.str() + "] [" + level_name + "] [" + file_name +
			":" + line_num + ":" + func_name + "] → " + message;

		return result;
	}

	void Logger::log(LogLevel level, const char* file, int line, const char* function, const char* message)
	{
		if (config_.outputs == LogOutput::None || !has_valid_output(config_.outputs))
		{
			return;
		}

		auto temp_logger = get_cached_logger(config_.outputs);
		if (temp_logger)
		{
			std::string fmt_msg = format_log_message(config_.outputs, level, file, line, function, message);

			switch (level)
			{
			case LogLevel::Trace:
				temp_logger->trace(fmt_msg);
				break;
			case LogLevel::Debug:
				temp_logger->debug(fmt_msg);
				break;
			case LogLevel::Info:
				temp_logger->info(fmt_msg);
				break;
			case LogLevel::Warn:
				temp_logger->warn(fmt_msg);
				break;
			case LogLevel::Error:
				temp_logger->error(fmt_msg);
				break;
			case LogLevel::Critical:
				temp_logger->critical(fmt_msg);
				break;
			case LogLevel::InfoRed:
			case LogLevel::InfoGreen:
			case LogLevel::InfoBlack: default:
				temp_logger->info(fmt_msg);
				break;
			}

			temp_logger->flush();
		}
	}

	bool Logger::has_valid_output(LogOutput outputs) const
	{
		if (outputs == LogOutput::None)
		{
			return false;
		}

		if (static_cast<int>(outputs) == 0)
		{
			return false;
		}

		if ((static_cast<int>(outputs & LogOutput::File)) && config_.file_path.empty())
		{
			return false;
		}

		if ((static_cast<int>(outputs & LogOutput::Gui)) && custom_callbacks_.find(LogOutput::Gui) == custom_callbacks_.
			end())
		{
			return false;
		}

		if ((static_cast<int>(outputs & LogOutput::VsTrace)) && custom_callbacks_.find(LogOutput::VsTrace) ==
			custom_callbacks_.end())
		{
			return false;
		}

		if ((static_cast<int>(outputs & LogOutput::Tracer)) && custom_callbacks_.find(LogOutput::Tracer) ==
			custom_callbacks_.end())
		{
			return false;
		}

		return true;
	}

	bool Logger::initialize()
	{
		try
		{
			spdlog::set_level(static_cast<spdlog::level::level_enum>(config_.level));

			static std::once_flag init_flag;
			std::call_once(init_flag, []() { spdlog::init_thread_pool(8192, 1); }); // 确保线程池只初始化一次(异步日志依赖此线程池)

			is_initialized_ = true;
			return true;
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			std::cerr << ex.what() << std::endl;
			is_initialized_ = false;
			return false;
		}
	}

	bool Logger::is_initialized() const
	{
		return is_initialized_;
	}

	void Logger::flush()
	{
		if (config_.outputs != LogOutput::None && has_valid_output(config_.outputs))
		{
			auto logger = get_cached_logger(config_.outputs);
			if (logger)
			{
				logger->flush();
			}
		}
	}

	void Logger::shutdown()
	{
		is_initialized_ = false;
		// 注意：不在这里清理spdlog的全局状态，因为可能有其他日志器在使用
	}

	std::shared_ptr<spdlog::logger> Logger::get_cached_logger(LogOutput outputs)
	{
		std::lock_guard<std::mutex> lock(cache_mutex_);

		// 定期清理过期缓存(每小时一次)
		constexpr auto cleanup_interval = std::chrono::hours(1);
		auto now = std::chrono::steady_clock::now();
		if (now - last_cleanup_time_ > cleanup_interval)
		{
			logger_cache_.clear();
			last_cleanup_time_ = now;
		}

		if (logger_cache_.count(outputs))
		{
			return logger_cache_[outputs];
		}

		// 缓存不存在，创建新日志器
		auto logger = create_temp_logger(outputs);
		if (logger)
		{
			logger_cache_[outputs] = logger;
		}
		return logger;
	}

	std::shared_ptr<spdlog::logger> Logger::create_temp_logger(LogOutput outputs)
	{
		std::vector<std::shared_ptr<spdlog::sinks::sink>> sinks;

		if (static_cast<int>(outputs & LogOutput::Console))
		{
			auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			sinks.push_back(console_sink);
		}

		if (static_cast<int>(outputs & LogOutput::File))
		{
			std::shared_ptr<spdlog::sinks::sink> file_sink = nullptr;

			do
			{
				if (config_.file_path.empty() || config_.file_name.empty())
				{
					break;
				}

				SYSTEMTIME st;
				GetLocalTime(&st);

				std::string date = string_utils::format("%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);

				std::string full_path = config_.file_path;
				if (!full_path.empty() && full_path.back() != '/' && full_path.back() != '\\')
				{
					full_path += '/';
				}

				full_path += date; // d:/log/yyyy-mm-dd

				file_system::create_directories(full_path);

				if (!full_path.empty() && full_path.back() != '/' && full_path.back() != '\\')
				{
					full_path += '/';
				}

				//full_path += date + "_" + config_.file_name; // d:/log/yyyy-mm-dd/yyyy-mm-dd_xxx.ini
				full_path += config_.file_name; // d:/log/yyyy-mm-dd/xxx.ini

				try
				{
					file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
						full_path, config_.max_file_size, config_.max_file_count);
				}
				catch (const spdlog::spdlog_ex& ex)
				{
					std::cerr << ex.what() << std::endl;
				}
			}
			while (false);

			if (file_sink)
			{
				sinks.push_back(file_sink);
			}
		}

		// 删除spdlog自动回调，使用手动实现的回调
		//if (static_cast<int>(outputs & LogOutput::Gui))
		//{
		//	auto it = custom_callbacks_.find(LogOutput::Gui);
		//	if (it != custom_callbacks_.end() && it->second)
		//	{
		//		auto custom_sink = create_custom_sink(it->second);
		//		sinks.push_back(custom_sink);
		//	}
		//}
		//
		//if (static_cast<int>(outputs & LogOutput::VsTrace))
		//{
		//	auto it = custom_callbacks_.find(LogOutput::VsTrace);
		//	if (it != custom_callbacks_.end() && it->second)
		//	{
		//		auto custom_sink = create_custom_sink(it->second);
		//		sinks.push_back(custom_sink);
		//	}
		//}
		//
		//if (static_cast<int>(outputs & LogOutput::XTrace))
		//{
		//	auto it = custom_callbacks_.find(LogOutput::XTrace);
		//	if (it != custom_callbacks_.end() && it->second)
		//	{
		//		auto custom_sink = create_custom_sink(it->second);
		//		sinks.push_back(custom_sink);
		//	}
		//}

		if (sinks.empty())
		{
			return nullptr;
		}

		// 异步
		//try
		//{
		//    static std::atomic<uint64_t> temp_logger_counter{ 0 };
		//    std::ostringstream oss;
		//    oss << "temp_logger_" << std::this_thread::get_id() << "_" << ++temp_logger_counter;
		//    std::string temp_logger_name = oss.str();
		//
		//    auto temp_logger = std::make_shared<spdlog::async_logger>(
		//        temp_logger_name,
		//        sinks.begin(),
		//        sinks.end(),
		//        spdlog::thread_pool(),  // 使用全局线程池
		//        spdlog::async_overflow_policy::block  // 缓冲区满时阻塞(避免丢失日志)
		//        );
		//
		//    temp_logger->set_level(static_cast<spdlog::level::level_enum>(config_.level));
		//    temp_logger->set_pattern("%v");
		//    return temp_logger;
		//}
		//catch (const spdlog::spdlog_ex& ex)
		//{
		//    std::cerr << ex.what() << std::endl;
		//    return nullptr;
		//}

		// 同步
		try
		{
			static std::atomic<uint64_t> temp_logger_counter{0};
			std::ostringstream oss;
			oss << "temp_logger_" << std::this_thread::get_id() << "_" << ++temp_logger_counter;
			std::string temp_logger_name = oss.str();

			auto temp_logger = std::make_shared<spdlog::logger>(temp_logger_name, sinks.begin(), sinks.end());
			temp_logger->set_level(static_cast<spdlog::level::level_enum>(config_.level));
			temp_logger->set_pattern("%v");
			return temp_logger;
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			std::cerr << ex.what() << std::endl;
			return nullptr;
		}
	}

	std::shared_ptr<Logger::CustomSink> Logger::create_custom_sink(const std::function<void(const MetaMsg&)>& callback)
	{
		return std::make_shared<CustomSink>(callback);
	}

	LogDbManager& LogDbManager::get_instance()
	{
		static LogDbManager instance;
		return instance;
	}

	bool LogDbManager::init()
	{
		if (is_running_)
			return true;
		is_running_ = true;

		std::thread(&LogDbManager::worker_thread, this).detach();
		return true;
	}

	void LogDbManager::exit()
	{
		if (!is_running_)
			return;
		is_running_ = false;

		cv_.notify_one();

		std::lock_guard<std::mutex> lock(db_mutex_);
		if (db_)
		{
			db_->close();
		}
		db_.reset();
	}

	void LogDbManager::add_msg_to_queue(const Logger::MetaMsg& msg)
	{
		if (!is_running_)
		{
			return;
		}
		queue_msg_.emplace(msg);
		cv_.notify_one();
	}

	void LogDbManager::worker_thread()
	{
		constexpr uint8_t FIELD_COUNT_MAX = 20; // 最大字段数
		auto CREATE_SQL = R"(CREATE TABLE IF NOT EXISTS logs (id INTEGER PRIMARY KEY AUTOINCREMENT, 
						timestamp TEXT,
						logger_name TEXT,
						thread_id INTEGER,
						output INTEGER,
						level INTEGER,
						file_name TEXT,
						line_num INTEGER,
						func_name TEXT,
						message TEXT,
						user_name TEXT,
						s11 TEXT, s12 TEXT, s13 TEXT, s14 TEXT, s15 TEXT, s16 TEXT, s17 TEXT, s18 TEXT, s19 TEXT, s20 TEXT);)";
		auto INSERT_SQL = R"(INSERT INTO logs (
						timestamp, 
						logger_name,
						thread_id, 
						output,
						level, 
						file_name, 
						line_num, 
						func_name, 
						message,
						user_name,
						s11, s12, s13, s14, s15, s16, s17, s18, s19, s20) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);)";

		auto LamdbaFunc = [this, FIELD_COUNT_MAX, CREATE_SQL, INSERT_SQL](const std::vector<Logger::MetaMsg>& meta_msg)
		{
			if (!meta_msg.size())
				return false;

			std::lock_guard<std::mutex> lock(db_mutex_);

			SYSTEMTIME st;
			GetLocalTime(&st);
			std::string date = string_utils::format("%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);

			if (date != db_last_date_ || !db_ || !db_->is_open())
			{
				if (db_)
					db_->close();

				file_system::create_directories("d:/log/logger");
				db_ = std::make_shared<SqliteManager>("d:/log/logger/" + date + "_log.db");
				// d:/log/logger/2025-10-10_log.db

				if (!db_->is_open())
					return false;

				db_->execute_non_query(CREATE_SQL);
				db_last_date_ = date;
			}

			SqliteManager::BatchParamsList params_list;
			params_list.reserve(meta_msg.size());
			for (const auto& msg : meta_msg)
			{
				SqliteManager::ParamsList params;

				// 压入顺序与日志数据库字段保持一致，SQLite支持UTF-8
				params.emplace_back(msg.time);
				params.emplace_back(static_cast<int>(msg.logger_name));
				params.emplace_back(static_cast<uint64_t>(msg.thread_id));
				params.emplace_back(static_cast<int>(msg.output));
				params.emplace_back(static_cast<int>(msg.level));
				params.emplace_back(string_utils::GBKToUTF8(msg.file));
				params.emplace_back(msg.line);
				params.emplace_back(string_utils::GBKToUTF8(msg.func));
				params.emplace_back(string_utils::GBKToUTF8(msg.message));
				while (params.size() < FIELD_COUNT_MAX)
				{
					params.emplace_back("");
				}
				params_list.push_back(std::move(params));
			}
			db_->execute_batch_non_query(INSERT_SQL, params_list);

			return true;
		};

		while (is_running_)
		{
			std::vector<Logger::MetaMsg> batch;
			{
				std::lock_guard<std::mutex> lock(queue_mutex_);
				for (auto* q : queues_)
				{
					while (q && !q->empty() && batch.size() < 1000)
					{
						batch.push_back(std::move(q->front()));
						q->pop();
					}
				}
				queues_.insert(&queue_msg_);
			}

			LamdbaFunc(batch);

			std::unique_lock<std::mutex> lock(queue_mutex_);
			cv_.wait_for(lock, std::chrono::milliseconds(1000), [this]()
			{
				if (!is_running_)
					return true;
				for (auto* q : queues_)
				{
					if (q && !q->empty())
						return true;
				}
				return false;
			});
		}

		// 退出前处理剩余日志
		std::vector<Logger::MetaMsg> remain;
		{
			std::lock_guard<std::mutex> lock(queue_mutex_);
			for (auto* q : queues_)
			{
				while (q && !q->empty())
				{
					remain.push_back(std::move(q->front()));
					q->pop();
				}
			}
		}

		if (remain.empty() || !db_ || !db_->is_open())
			return;

		LamdbaFunc(remain);
	}

	LoggerManager& LoggerManager::get_instance()
	{
		static LoggerManager instance;
		return instance;
	}

	bool LoggerManager::create_logger(const LogName& name)
	{
		std::lock_guard<std::mutex> lock(logger_mutex_);

		if (loggers_.find(name) != loggers_.end())
		{
			return true;
		}

		auto logger = std::make_shared<Logger>(name);
		logger->get_config() = Logger::Config(name);

		if (!logger->is_initialized())
			logger->initialize();

		loggers_[name] = logger;

		return logger ? true : false;
	}

	bool LoggerManager::create_logger(const Logger::Config& config)
	{
		std::lock_guard<std::mutex> lock(logger_mutex_);

		if (loggers_.find(config.log_name) != loggers_.end())
		{
			return true;
		}

		auto logger = std::make_shared<Logger>(config);

		if (!logger->is_initialized())
			logger->initialize(); // 内部已持有 config

		loggers_[config.log_name] = logger;

		return logger ? true : false;
	}

	std::shared_ptr<Logger> LoggerManager::get_logger(const LogName& name)
	{
		std::lock_guard<std::mutex> lock(logger_mutex_);

		auto it = loggers_.find(name);
		if (it != loggers_.end())
		{
			return it->second;
		}
		// 没有找到，返回默认 Logger
		return loggers_[LogName::DEFAULT];
	}

	std::vector<LogName> LoggerManager::logger_names() const
	{
		std::lock_guard<std::mutex> lock(logger_mutex_);

		std::vector<LogName> names;
		for (const auto& pair : loggers_)
		{
			if (pair.first != LogName::DEFAULT)
				names.push_back(pair.first);
		}
		return names;
	}

	std::string LoggerManager::log_name_to_str(const LogName& name)
	{
		switch (name)
		{
		case LogName::MOUNT:
			return "mount";
		case LogName::MOTION:
			return "motion";
		case LogName::DB_DATA:
			return "smt_data";
		case LogName::OPTIMIZE:
			return "optimize";
		case LogName::SLAVE_CONTROL:
			return "slave_control";
		case LogName::CAMERA_TOP:
			return "camera_top";
		case LogName::DEFAULT:
			return "default";
		default:
			return "unknown";
		}
	}

	LoggerManager::LoggerManager()
	{
		LogDbManager::get_instance().init();

		//【1】创建日志器(当模块日志器不存在时返回默认日志器)
		for (int log_name_index = 0; log_name_index < static_cast<int>(LogName::LOGNAME_MAX); ++log_name_index)
		{
			auto log_name = static_cast<LogName>(log_name_index);
			Logger::Config config(log_name);
			config.file_path = "d:/log";
			config.file_name = log_name_to_str(log_name) + ".ini";
			config.level = LogLevel::Debug;
			create_logger(config);
		}

		//【2】启动定期清理线程
		stop_cleanup_thread_ = false;
		cleanup_thread_ = std::thread(&LoggerManager::cleanup_thread, this);
	}

	LoggerManager::~LoggerManager()
	{
		stop_cleanup_thread_ = true;
		if (cleanup_thread_.joinable())
		{
			cleanup_thread_.join();
		}

		LogDbManager::get_instance().exit();
	}

	void LoggerManager::add_cleanup_directory(const std::string& path, int days)
	{
		std::lock_guard<std::mutex> lock(cleanup_mutex_);
		if (days <= 0)
			cleanup_map_.erase(path);
		else
			cleanup_map_[path] = days;
	}

	void LoggerManager::cleanup_thread()
	{
		while (!stop_cleanup_thread_)
		{
			std::string msg("Start cleanup thread...\n");

			// 拷贝配置(避免持有锁期间耗时)
			std::unordered_map<std::string, int> cleanup_map;
			{
				std::lock_guard<std::mutex> lock(cleanup_mutex_);
				cleanup_map = cleanup_map_;
			}

			for (const auto& config : cleanup_map)
			{
				const std::string& path = config.first;
				int days = config.second;

				if (days <= 0)
				{
					continue;
				}

				if (!PathIsDirectoryA(path.c_str()))
				{
					continue;
				}

				delete_expired_log_files(path, days);
			}

			constexpr int cleanup_interval = 3600; // 等待指定间隔(秒)，避免频繁刷新IO
			for (int i = 0; i < cleanup_interval && !stop_cleanup_thread_; ++i)
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}
	}
}
