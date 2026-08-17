#pragma once
// ============================================================================
// 日志管理类(LoggerManager)：LogName / LogLevel / LogOutput / Logger / LogDbManager / LoggerManager
// 基于 spdlog 封装的多日志器管理：支持控制台/文件/GUI 回调/VS trace/Tracer 输出、
// 级别过滤、文件滚动、日志落库（SQLite，按日期分库）与过期日志自动清理。
// 通过 LOG 宏访问单例：LOG_MOUNT(LogLevel::Info, "...", ...)
// ============================================================================
#include "common_export.h"
#include "sqlite_manager.h"

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace common_tools
{
	/**
	 * @brief 日志器名称枚举，根据此枚举创建不同日志器
	 * 通过 LOG_XXX 宏按模块记录日志
	 **/
	enum class COMMONTOOLS_API LogName
	{
		DEFAULT = 0, // 默认日志器
		MOUNT, // 装配模块
		MOTION, // 运动模块
		DB_DATA, // 数据库数据模块
		OPTIMIZE, // 优化模块
		SLAVE_CONTROL, // 从站控制模块
		CAMERA_TOP, // 顶部相机模块
		LOGNAME_MAX, //最大数量（标记枚举结束）
	};

	/**
	 * @brief 日志级别枚举（对应 spdlog::level::level_enum）
	 * InfoRed/InfoGreen/InfoBlack 为带颜色的 info 级别
	 **/
	enum class COMMONTOOLS_API LogLevel
	{
		Trace,
		Debug,
		Info,
		Warn,
		Error,
		Critical,
		InfoRed, //dz_add_info(0, ...);
		InfoGreen, //dz_add_info(1, ...);
		InfoBlack, //dz_add_info(2, ...);
		Off
	};

	/** @brief 日志输出目标位标志（支持按位组合） */
	enum class COMMONTOOLS_API LogOutput
	{
		None = 0, // 00000 = 0      // 输出   无
		Console = 1 << 0, // 00001 = 1      // 输出到 控制台
		File = 1 << 1, // 00010 = 2      // 输出到 文件
		Gui = 1 << 2, // 00100 = 4      // 输出到 界面
		VsTrace = 1 << 3, // 01000 = 8      // 输出到 VS TRACE
		Tracer = 1 << 4 // 10000 = 16     // 输出到 TRACER.exe
	};

	/** @brief 输出目标位或运算 */
	inline LogOutput operator|(LogOutput a, LogOutput b)
	{
		return static_cast<LogOutput>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	/** @brief 输出目标位与运算 */
	inline LogOutput operator&(LogOutput a, LogOutput b)
	{
		return static_cast<LogOutput>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}

	/** @brief 输出目标位异或运算 */
	inline LogOutput operator^(LogOutput a, LogOutput b)
	{
		return static_cast<LogOutput>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
	}

	/** @brief 输出目标位取反运算 */
	inline LogOutput operator~(LogOutput a) { return static_cast<LogOutput>(~static_cast<uint32_t>(a)); }

	/** @brief 输出目标位或赋值运算 */
	inline LogOutput operator|=(LogOutput& a, LogOutput b) { return a = a | b; }

	/** @brief 输出目标位与赋值运算 */
	inline LogOutput operator&=(LogOutput& a, LogOutput b) { return a = a & b; }

	/** @brief 输出目标位异或赋值运算 */
	inline LogOutput operator^=(LogOutput& a, LogOutput b) { return a = a ^ b; }


	/**
	 * @brief 日志器：单日志器实现，负责格式化、输出目标路由与回调分发
	 **/
	class COMMONTOOLS_API Logger
	{
	public:
		/** @brief 日志器配置结构体 */
		struct COMMONTOOLS_API Config
		{
			LogName log_name = LogName::DEFAULT; // 日志器名称
			std::string file_path = "d:/log"; // 文件路径
			std::string file_name = "default.ini"; // 文件名

			LogLevel level = LogLevel::Off; // 日志级别
			LogOutput outputs = LogOutput::None; // 日志输出目标

			std::size_t max_file_size = 10 * 1024 * 1024; // 按大小滚动时的文件大小限制
			std::size_t max_file_count = 100; // 按大小滚动时的文件数量限制

			bool enable_database = true; // 启用写日志数据库

			Config() = default;

			/** @brief 按日志器名称构造默认配置 */
			explicit Config(const LogName& name);
		};


		/** @brief 日志消息元数据（回调与落库使用） */
		struct COMMONTOOLS_API MetaMsg
		{
			std::string time; // 时间（HH:MM:SS.mmm）
			LogName logger_name; // 日志器名
			size_t thread_id = 0; // 线程 ID
			LogOutput output = LogOutput::None; // 输出目标
			LogLevel level = LogLevel::Off; // 日志级别
			std::string file; // 文件名
			int line = 0; // 行号(可能为0)
			std::string func; // 函数名
			std::string message; // 日志内容
		};


		/** @brief 构造默认日志器 */
		Logger();

		/** @brief 析构函数 */
		~Logger();

		/** @brief 按名称构造日志器 */
		explicit Logger(const LogName& name);

		/** @brief 按完整配置构造日志器 */
		explicit Logger(const Config& config);

		Logger(const Logger& other) = delete;

		Logger& operator=(const Logger& other) = delete;

		Logger(Logger&& other) = default;

		Logger& operator=(Logger&& other) = default;

		/**
		 * @brief 初始化日志器（设置级别并初始化 spdlog 线程池）
		 * @return 初始化是否成功
		 **/
		bool initialize();

		/**
		 * @brief 检查日志器是否已初始化
		 * @return 已初始化返回 true
		 **/
		bool is_initialized() const;

		/** @brief 刷新所有输出目标（落盘） */
		void flush();

		/** @brief 关闭日志器（置为未初始化，不清理 spdlog 全局状态） */
		void shutdown();

		/**
		 * @brief 获取日志器配置引用（可修改后重新生效）
		 * @return 配置引用
		 **/
		Config& get_config();

		/**
		 * @brief 注册输出目标回调（Gui/VsTrace/Tracer 等目标由回调接收消息）
		 * @param [in] type - 输出目标类型
		 * @param [in] callback - 回调函数（接收 MetaMsg）
		 **/
		void set_output_callback(LogOutput type, const std::function<void(const MetaMsg&)>& callback);

		/** @brief 记录 trace 级日志（printf 风格） */
		void trace(const char* format, ...);

		/** @brief 记录 debug 级日志（printf 风格） */
		void debug(const char* format, ...);

		/** @brief 记录 info 级日志（printf 风格） */
		void info(const char* format, ...);

		/** @brief 记录 warn 级日志（printf 风格） */
		void warn(const char* format, ...);

		/** @brief 记录 error 级日志（printf 风格） */
		void error(const char* format, ...);

		/** @brief 记录 critical 级日志（printf 风格） */
		void critical(const char* format, ...);

		/**
		 * @brief 按指定级别记录日志（printf 风格）
		 * @param [in] level - 日志级别
		 * @param [in] format - 格式串
		 **/
		void log_record(LogLevel level, const char* format, ...);

		/** @brief 带文件/行号/函数名的 trace 级日志 */
		void trace(const char* file, int line, const char* function, const char* format, ...);

		/** @brief 带文件/行号/函数名的 debug 级日志 */
		void debug(const char* file, int line, const char* function, const char* format, ...);

		/** @brief 带文件/行号/函数名的 info 级日志 */
		void info(const char* file, int line, const char* function, const char* format, ...);

		/** @brief 带文件/行号/函数名的 warn 级日志 */
		void warn(const char* file, int line, const char* function, const char* format, ...);

		/** @brief 带文件/行号/函数名的 error 级日志 */
		void error(const char* file, int line, const char* function, const char* format, ...);

		/** @brief 带文件/行号/函数名的 critical 级日志 */
		void critical(const char* file, int line, const char* function, const char* format, ...);

		/**
		 * @brief 带文件/行号/函数名按级别记录日志
		 * @param [in] file - 源文件名（通常传 __FILE__）
		 * @param [in] line - 行号（通常传 __LINE__）
		 * @param [in] function - 函数名（通常传 __FUNCTION__）
		 * @param [in] level - 日志级别
		 * @param [in] format - 格式串
		 **/
		void log_record(const char* file, int line, const char* function, LogLevel level, const char* format, ...);

	private:
		/** @brief 获取（或创建）对应输出组合的缓存 spdlog logger */
		std::shared_ptr<spdlog::logger> get_cached_logger(LogOutput outputs);

		/** @brief 按输出组合创建临时 spdlog logger（含文件滚动） */
		std::shared_ptr<spdlog::logger> create_temp_logger(LogOutput outputs);

		/** @brief 格式化日志消息（时间/线程/级别/文件行号）并触发回调 */
		std::string format_log_message(LogOutput outputs, LogLevel level, const char* file, int line,
		                             const char* function, const char* message);

		/** @brief 核心写日志入口（路由到 spdlog 输出） */
		void log(LogLevel level, const char* file, int line, const char* function, const char* message);

		/** @brief 检查输出组合是否有效（目标是否已注册回调等） */
		bool has_valid_output(LogOutput outputs) const;

		class CustomSink;

		/** @brief 创建自定义 spdlog sink（回调分发） */
		std::shared_ptr<CustomSink> create_custom_sink(const std::function<void(const MetaMsg&)>& callback);

		Config config_; // 日志器配置

		std::map<LogOutput, std::function<void(const MetaMsg&)>> custom_callbacks_; // 输出目标回调表
		std::atomic<bool> is_initialized_; // 初始化标志

		std::unordered_map<LogOutput, std::shared_ptr<spdlog::logger>> logger_cache_; // 输出组合→spdlog logger 缓存
		std::mutex cache_mutex_; // 缓存锁
		std::chrono::steady_clock::time_point last_cleanup_time_; // 缓存清理时间点
	};


	/**
	 * @brief 日志数据库管理器（单例）：异步将日志写入 SQLite（按日期分库）
	 **/
	class COMMONTOOLS_API LogDbManager
	{
	public:
		LogDbManager(const LogDbManager&) = delete;

		LogDbManager& operator=(const LogDbManager&) = delete;

		/**
		 * @brief 获取单例实例
		 * @return 全局唯一实例
		 **/
		static LogDbManager& get_instance();

		/**
		 * @brief 初始化（启动后台写库线程）
		 * @return 是否成功
		 **/
		bool init();

		/** @brief 退出（通知线程结束并关闭数据库） */
		void exit();

		/**
		 * @brief 将日志消息加入写库队列（异步）
		 * @param [in] msg - 日志消息元数据
		 **/
		void add_msg_to_queue(const Logger::MetaMsg& msg);

	private:
		LogDbManager() = default;

		~LogDbManager() = default;

		/** @brief 后台工作线程：批量写库 */
		void worker_thread();

		std::atomic<bool> is_running_{false}; // 运行标志
		std::mutex queue_mutex_; // 队列锁
		std::condition_variable cv_; // 队列条件变量
		std::queue<Logger::MetaMsg> queue_msg_; // 消息队列
		std::unordered_set<std::queue<Logger::MetaMsg>*> queues_; // 消息队列集合（支持多队列）

		std::mutex db_mutex_; // 数据库锁
		std::shared_ptr<SqliteManager> db_; // 当前数据库实例
		std::string db_last_date_; // 当前数据库日期
	};


	/**
	 * @brief 日志管理器（单例）：按 LogName 维护多日志器，提供 LOG 宏访问入口，
	 * 并支持过期日志文件定期清理
	 **/
	class COMMONTOOLS_API LoggerManager
	{
	public:
		/**
		 * @brief 获取单例实例
		 * @return 全局唯一实例
		 **/
		static LoggerManager& get_instance();

		/**
		 * @brief 获取指定名称的日志器（不存在时返回默认日志器）
		 * @param [in] name - 日志器名称
		 * @return 日志器共享指针
		 **/
		std::shared_ptr<Logger> get_logger(const LogName& name);

		/**
		 * @brief 获取所有已注册日志器名称（不含默认）
		 * @return 日志器名称列表
		 **/
		std::vector<LogName> logger_names() const;

	private:
		LoggerManager();

		~LoggerManager();

		LoggerManager(const LoggerManager&) = delete;

		LoggerManager& operator=(const LoggerManager&) = delete;

		/** @brief 按名称创建日志器并注册 */
		bool create_logger(const LogName& name);

		/** @brief 按配置创建日志器并注册 */
		bool create_logger(const Logger::Config& config);

		/** @brief 日志器名称转字符串（用于日志文件名） */
		std::string log_name_to_str(const LogName& name);

	public:
		/**
		 * @brief 注册日志目录清理规则
		 * @param [in] path - 目录路径
		 * @param [in] days - 保留天数（<=0 时移除该规则）
		 **/
		void add_cleanup_directory(const std::string& path, int days);

	private:
		/** @brief 后台清理线程：按规则删除过期日志文件 */
		void cleanup_thread();

		std::unordered_map<LogName, std::shared_ptr<Logger>> loggers_; // 日志器映射
		mutable std::mutex logger_mutex_; // 日志器表锁

		std::unordered_map<std::string, int> cleanup_map_; // <日志路径，保留天数>
		std::atomic<bool> stop_cleanup_thread_{false}; // 停止线程标志
		std::thread cleanup_thread_; // 后台清理线程
		std::mutex cleanup_mutex_; // 保护清理配置
	};


	//TODO:如何区分模组，不同相机类型
/** @brief 全局日志管理器单例宏：LOG.get_logger(LogName::XXX)->info(...) */
#define LOG common_tools::LoggerManager::get_instance()
/** @brief 装配模块日志宏 */
#define LOG_MOUNT(level, format, ...) LOG.get_logger(LogName::MOUNT)->log_record(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
/** @brief 运动模块日志宏 */
#define LOG_MOTION(level, format, ...) LOG.get_logger(LogName::MOTION)->log_record(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
/** @brief 数据库数据模块日志宏 */
#define LOG_DATA(level, format, ...) LOG.get_logger(LogName::DB_DATA)->log_record(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
/** @brief 优化模块日志宏 */
#define LOG_OPTIMIZE(level, format, ...) LOG.get_logger(LogName::OPTIMIZE)->log_record(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
/** @brief 从站控制模块日志宏 */
#define LOG_SLAVE_CONTROL(level, format, ...) LOG.get_logger(LogName::SLAVE_CONTROL)->log_record(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
/** @brief 顶部相机模块日志宏 */
#define LOG_CAMERA_TOP(level, format, ...) LOG.get_logger(LogName::CAMERA_TOP)->log_record(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
}
