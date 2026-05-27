#pragma once
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <future>
#include <queue>
#include <map>
#include <mutex>
#include <memory>
#include <atomic>
#include <unordered_set>
#include <unordered_map>

#ifdef _WINDLL
#ifdef COMMONTOOLS_DLL
#define COMMONTOOLS_API __declspec(dllexport)
#else
#define COMMONTOOLS_API __declspec(dllimport)
#endif // COMMONTOOLS_API
#else
#define COMMONTOOLS_API //_WINEXE
#endif // _WINDLL

// 通用的返回值枚举
enum ReturnCode
{
	RC_SUCCESS = 0, ///<成功
	RC_FAILED = 1, ///<失败
	RC_ERROR = 2, ///<错误
	RC_CANCELLED = 3, ///<已取消
	RC_ABORTED = 4, ///<已终止
	RC_TIMEOUT = 5, ///<已超时
	RC_BUSY = 6, ///<正在忙/占用中
	RC_NULL = 7, ///<空/空指针/...
};


// 前置声明
struct sqlite3;
struct sqlite3_stmt;
struct sqlite3_blob;


namespace spdlog
{
	class logger;
}


namespace common_tools
{
#pragma region INI配置文件管理类（IniManager）
	class COMMONTOOLS_API IniManager
	{
		template <typename T>
		std::string ToString(const T& value)
		{
			std::ostringstream oss;
			if (typeid(value) == typeid(float))
				oss << std::setprecision(6) << value;
			else if (typeid(value) == typeid(double))
				oss << std::setprecision(15) << value;
			else
				oss << value;
			return oss.str();
		}

		template <typename T>
		T FromString(const std::string& str, const T& default_value)
		{
			if (str.empty())
				return default_value;
			T value{};
			if (std::is_same<T, bool>::value)
			{
				std::string s = str;
				std::transform(s.begin(), s.end(), s.begin(), tolower);
				value = (s == "true" || s == "1" || s == "yes");
			}
			else
			{
				std::istringstream iss(str);
				if (!(iss >> value) || !iss.eof())
					value = default_value;
			}
			return value;
		}

	public:
		explicit IniManager(const std::string& file_path);

		IniManager(const IniManager&) = delete;

		IniManager(const IniManager&&) = delete;

		IniManager& operator=(const IniManager&) = delete;

		IniManager& operator=(const IniManager&&) = delete;

		bool WriteValue(const std::string& section, const std::string& key, const std::string& value);

		std::string ReadValue(const std::string& section, const std::string& key, const std::string& default_value);

		bool WriteInt(const std::string& section, const std::string& key, int value);

		bool WriteBool(const std::string& section, const std::string& key, bool value);

		bool WriteDouble(const std::string& section, const std::string& key, double value);

		int ReadInt(const std::string& section, const std::string& key, int default_value = 0);

		bool ReadBool(const std::string& section, const std::string& key, bool default_value = false);

		double ReadDouble(const std::string& section, const std::string& key, double default_value = 0.0);

		std::map<std::string, std::string> ReadSection(const std::string& section);

		bool WriteSection(const std::string& section, const std::map<std::string, std::string>& keyValues);

		bool DeleteKey(const std::string& section, const std::string& key);

		bool DeleteSection(const std::string& section);

		bool FileExists();

		bool BackupFile(const std::string& backupPath);

		bool SectionExists(const std::string& section);

		bool KeyExists(const std::string& section, const std::string& key);

		std::vector<std::string> GetSectionNames();

		std::vector<std::string> GetKeyNames(const std::string& section);

		std::string GetLastError();

	private:
		void SetLastError(const std::string& error);

		std::string file_path_;
		std::string last_error_;
	};
#pragma endregion

#pragma region XML配置文件管理类（XmlManager）
	class COMMONTOOLS_API XmlManager
	{
	public:
		XmlManager();

		~XmlManager();
	};
#pragma endregion

#pragma region  JSON配置文件管理类（JsonManager）
	class COMMONTOOLS_API JsonManager
	{
	public:
		JsonManager();

		~JsonManager();
	};
#pragma endregion

#pragma region SQLServer配置文件管理类（SQLServerManager）
	class COMMONTOOLS_API SQLServerManager
	{
	};
#pragma endregion

#pragma region Sqlite配置文件管理类（SqliteManager）
	class COMMONTOOLS_API SqliteManager
	{
	public:
		enum class ParamType
		{
			Null,
			Bool,
			Int,
			UInt,
			Int64,
			UInt64,
			String,
			Double,
			Blob,
		};


		struct SqliteParam
		{
			ParamType type = ParamType::Null;
			int int_val = 0;
			int64_t int64_val = 0;
			std::string str_val;
			double double_val = 0.0;
			std::vector<char> blob_val;

			SqliteParam(bool val)
				: type(ParamType::Bool),
				  int_val(val ? 1 : 0)
			{
			}

			SqliteParam(int32_t val)
				: type(ParamType::Int),
				  int_val(val)
			{
			}

			SqliteParam(uint32_t val)
				: type(ParamType::UInt),
				  int64_val(static_cast<int64_t>(val))
			{
			}

			SqliteParam(int64_t val)
				: type(ParamType::Int64),
				  int64_val(val)
			{
			}

			SqliteParam(uint64_t val)
				: type(ParamType::UInt64),
				  int64_val(static_cast<int64_t>(val))
			{
			}

			SqliteParam(const std::string& val)
				: type(ParamType::String),
				  str_val(val)
			{
			}

			SqliteParam(double val)
				: type(ParamType::Double),
				  double_val(val)
			{
			}

			SqliteParam(const char* val)
				: type(ParamType::String),
				  str_val(val)
			{
			}

			SqliteParam(const char* val, size_t len)
				: type(ParamType::Blob),
				  blob_val(val, val + len)
			{
			}

			SqliteParam(const std::vector<char>& val)
				: type(ParamType::Blob),
				  blob_val(val)
			{
			}

			SqliteParam(std::vector<char>&& val)
				: type(ParamType::Blob),
				  blob_val(std::move(val))
			{
			}
		};


		using ParamsList = std::vector<SqliteParam>;
		using BatchParamsList = std::vector<ParamsList>;
		using UMapList = std::vector<std::unordered_map<std::string, std::string>>;

		SqliteManager() noexcept;

		~SqliteManager();

		explicit SqliteManager(const std::string& file_name);

		SqliteManager(SqliteManager&& other) noexcept;

		SqliteManager& operator=(SqliteManager&& other) noexcept;

		SqliteManager(const SqliteManager&) = delete;

		SqliteManager& operator=(const SqliteManager&) = delete;

		bool Open(const std::string& file_name);

		void Close();

		bool IsOpen() noexcept;

		void SetMaxStmtCacheCount(size_t count) noexcept;

		bool ExecuteNonQuery(const std::string& sql, const ParamsList& params = ParamsList());

		bool ExecuteBatchNonQuery(const std::string& sql, const BatchParamsList& params_list); // 此函数内已使用事务

		bool ExecuteQuery(const std::string& sql, const ParamsList& params, UMapList& result);

		bool ExecuteQueryPage(const std::string& sql, const ParamsList& params, int current_page, int page_size,
		                      UMapList& data, int& total_count, int& total_pages);

		bool ReadBlobChunk(const std::string& table_name, const std::string& col_name, int64_t row_id, size_t offset,
		                   size_t chunk_size, std::vector<char>& chunk_data);

		bool WriteBlobChunk(const std::string& table_name, const std::string& col_name, int64_t row_id, size_t offset,
		                    const std::vector<char>& chunk_data);

		bool BeginTransaction();

		bool CommitTransaction();

		bool RollbackTransaction();

		int64_t GetLastInsertId() const noexcept;

		std::string GetLastErrorMsg() const noexcept;

	private:
		bool CheckConnection() noexcept;

		bool TryReconnect() noexcept;

		sqlite3_stmt* GetStmtCache(const std::string& sql);

		void AddStmtCache(const std::string& sql, sqlite3_stmt* stmt);

		void ClearStmtCache();

		bool BindParams(sqlite3_stmt* stmt, const ParamsList& params);

		bool ExecutePreparedQuery(sqlite3_stmt* stmt, UMapList& result);

		sqlite3* db_ = nullptr; // 数据库句柄
		mutable std::recursive_mutex mutex_; // 线程安全锁
		std::string last_error_; // 错误信息
		std::string file_name_; // 数据库路径
		size_t stmt_cache_max_count_ = 50; // 最大缓存语句数
		std::unordered_map<std::string, sqlite3_stmt*> stmt_cache_; // 预处理语句缓存
	};

#pragma endregion

#pragma region 线程池类（ThreadPool）
	class COMMONTOOLS_API ThreadPool
	{
	public:
		explicit ThreadPool(size_t threads = 4);

		~ThreadPool();

		size_t pending_tasks();

		template <class F, class... Args>
		std::future<std::result_of_t<F(Args...)>> enqueue(F&& f, Args&&... args)
		{
			using return_type = std::result_of_t<F(Args...)>;

			auto task = std::make_shared<std::packaged_task<return_type()>>(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...));

			std::future<return_type> res = task->get_future();
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);

				// don't allow enqueueing after stopping the pool
				if (stop_)
					throw std::runtime_error("enqueue on stopped ThreadPool");

				tasks_.emplace([task]() { (*task)(); });
			}
			condition_.notify_one();
			return res;
		}

	private:
		// need to keep track of threads so we can join them
		std::vector<std::thread> workers_;

		// the task queue
		std::queue<std::function<void()>> tasks_;

		// synchronization
		std::mutex queue_mutex_;
		std::condition_variable condition_;
		std::atomic<bool> stop_;
	};
#pragma endregion

#pragma region 自定义配置类（CfgCustom）
	// 数据类型
	enum class ConfigDataType { Null, Int, Double, String };


	// 键访问器
	class COMMONTOOLS_API ConfigKey
	{
	public:
		ConfigKey(class ConfigImpl* impl, const std::string& file_name, const std::string& section,
		          const std::string& key);

		ConfigKey(class ConfigImpl* impl, const std::string& file_name, const std::string& section);

		ConfigKey operator[](const std::string& key);

		ConfigKey& SetInt(const int& value);

		ConfigKey& SetDouble(const double& value);

		ConfigKey& SetString(const std::string& value);

		ConfigKey& SetDescription(const std::string& value);

		int GetInt(const int& default_value = 0, const std::string& description = "") const;

		double GetDouble(const double& default_value = 0.0, const std::string& description = "") const;

		std::string GetString(const std::string& default_value = "", const std::string& description = "") const;

		std::string GetDescription(const std::string& default_value = "") const;

		ConfigDataType GetType() const;

	private:
		class ConfigImpl* impl_;
		std::string file_name_;
		std::string section_;
		std::string key_;
	};


	// 节访问器
	class COMMONTOOLS_API ConfigSection
	{
	public:
		ConfigSection(class ConfigImpl* impl, const std::string& file_name);

		ConfigKey operator[](const std::string& section);

	private:
		class ConfigImpl* impl_;
		std::string file_name_;
	};


	// 自定义配置类
	class COMMONTOOLS_API ConfigCustom
	{
	public:
		struct COMMONTOOLS_API Members
		{
			std::string file_name; // 文件名
			std::string section; // 节点
			std::string key; // 键名
			std::string value; // 值
			std::string description; // 描述
		};


		static ConfigCustom& GetInstance();

		ConfigSection operator[](const std::string& file_name);

		std::vector<std::string> GetJsonFileList();

		std::string JsonToString() const;

		std::string GetLastErrorMsg() const;

		bool LoadJsonFile(const std::string& file_name);

		bool SaveJsonFile(const std::string& file_name, const bool& is_new_file = false);

		bool DeleteJsonFile(const std::string& file_name);

		bool RenameJsonFile(const std::string& old_file_name, const std::string& new_file_name);

		bool LoadJsonFiles(); // 加载所有文件
		bool SaveJsonFiles(); // 保存所有文件

		bool JsonToVector(const std::string& file_name, std::vector<Members>& current_file_data);

		bool VectorToJson(const std::string& file_name, const std::vector<Members>& current_file_data);

		bool RemoveJsonObject(const std::string& object_path);

		// 移除JSON下对象。object_path 格式：fileName/section/key → "demo.json/test/enable"

	private:
		ConfigCustom();

		~ConfigCustom();

		ConfigCustom(const ConfigCustom&) = delete;

		ConfigCustom(const ConfigCustom&&) = delete;

		ConfigCustom& operator=(const ConfigCustom&) = delete;

		ConfigCustom& operator=(const ConfigCustom&&) = delete;

		class ConfigImpl* impl_;
	};


#define CFG common_tools::ConfigCustom::GetInstance()

#pragma endregion

#pragma region 日志管理类（LoggerManager）
	using LoggerName = enum class COMMONTOOLS_API LogName // 日志器枚举，根据此枚举创建不同日志器
	{
		DEFAULT = 0,
		MOUNT,
		MOTION,
		SMT_DATA,
		OPTIMIZE,
		SLAVE_CONTROL,
		CAMERA_TOP,
		LOGNAME_MAX, //最大数量
	};

	using LoggerLevel = enum class COMMONTOOLS_API LogLevel // 对应 spdlog::level::level_enum
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

	using LoggerOutput = enum class COMMONTOOLS_API LogOutput
	{
		None = 0, // 00000 = 0      // 输出   无
		Console = 1 << 0, // 00001 = 1      // 输出到 控制台
		File = 1 << 1, // 00010 = 2      // 输出到 文件
		Gui = 1 << 2, // 00100 = 4      // 输出到 界面
		VsTrace = 1 << 3, // 01000 = 8      // 输出到 VS TRACE
		Tracer = 1 << 4 // 10000 = 16     // 输出到 TRACER.exe
	};

	inline LogOutput operator|(LogOutput a, LogOutput b)
	{
		return static_cast<LogOutput>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	inline LogOutput operator&(LogOutput a, LogOutput b)
	{
		return static_cast<LogOutput>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}

	inline LogOutput operator^(LogOutput a, LogOutput b)
	{
		return static_cast<LogOutput>(static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
	}

	inline LogOutput operator~(LogOutput a) { return static_cast<LogOutput>(~static_cast<uint32_t>(a)); }

	inline LogOutput operator|=(LogOutput& a, LogOutput b) { return a = a | b; }

	inline LogOutput operator&=(LogOutput& a, LogOutput b) { return a = a & b; }

	inline LogOutput operator^=(LogOutput& a, LogOutput b) { return a = a ^ b; }


	class COMMONTOOLS_API Logger
	{
	public:
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

			explicit Config(const LogName& name);
		};


		struct COMMONTOOLS_API MetaMsg
		{
			std::string time;
			LogName logger_name; // 日志器名
			size_t thread_id = 0;
			LogOutput output = LogOutput::None;
			LogLevel level = LogLevel::Off;
			std::string file; // 文件名
			int line = 0; // 行号（可能为0）
			std::string func; // 函数名
			std::string message;
		};


		Logger();

		~Logger();

		explicit Logger(const LogName& name);

		explicit Logger(const Config& config);

		Logger(const Logger& other) = delete;

		Logger& operator=(const Logger& other) = delete;

		Logger(Logger&& other) = default;

		Logger& operator=(Logger&& other) = default;

		bool Initialize();

		bool IsInitialized() const;

		void Flush();

		void Shutdown();

		Config& GetConfig();

		void SetOutputCallback(LogOutput type, const std::function<void(const MetaMsg&)>& callback);

		void Trace(const char* format, ...);

		void Debug(const char* format, ...);

		void Info(const char* format, ...);

		void Warn(const char* format, ...);

		void Error(const char* format, ...);

		void Critical(const char* format, ...);

		void LogRecord(LogLevel level, const char* format, ...);

		void Trace(const char* file, int line, const char* function, const char* format, ...);

		void Debug(const char* file, int line, const char* function, const char* format, ...);

		void Info(const char* file, int line, const char* function, const char* format, ...);

		void Warn(const char* file, int line, const char* function, const char* format, ...);

		void Error(const char* file, int line, const char* function, const char* format, ...);

		void Critical(const char* file, int line, const char* function, const char* format, ...);

		void LogRecord(const char* file, int line, const char* function, LogLevel level, const char* format, ...);

	private:
		std::shared_ptr<spdlog::logger> GetCachedLogger(LogOutput outputs);

		std::shared_ptr<spdlog::logger> CreateTempLogger(LogOutput outputs);

		std::string FormatLogMessage(LogOutput outputs, LogLevel level, const char* file, int line,
		                             const char* function, const char* message);

		void Log(LogLevel level, const char* file, int line, const char* function, const char* message);

		bool HasValidOutput(LogOutput outputs) const;

		class CustomSink;

		std::shared_ptr<CustomSink> CreateCustomSink(const std::function<void(const MetaMsg&)>& callback);

		Config config_;

		std::map<LogOutput, std::function<void(const MetaMsg&)>> custom_callbacks_;
		std::atomic<bool> is_initialized_;

		std::unordered_map<LogOutput, std::shared_ptr<spdlog::logger>> logger_cache_;
		std::mutex cache_mutex_;
		std::chrono::steady_clock::time_point last_cleanup_time_;
	};


	class COMMONTOOLS_API LogDbManager
	{
	public:
		LogDbManager(const LogDbManager&) = delete;

		LogDbManager& operator=(const LogDbManager&) = delete;

		static LogDbManager& GetInstance();

		bool Init();

		void Exit();

		void AddMsgToQueue(const Logger::MetaMsg& msg);

	private:
		LogDbManager() = default;

		~LogDbManager() = default;

		void WorkerThread();

		std::atomic<bool> is_running_{false};
		std::mutex queue_mutex_;
		std::condition_variable cv_;
		std::queue<Logger::MetaMsg> queue_msg_;
		std::unordered_set<std::queue<Logger::MetaMsg>*> queues_;

		std::mutex db_mutex_; // 数据库锁
		std::shared_ptr<SqliteManager> db_; // 当前数据库实例
		std::string db_last_date_; // 当前数据库日期
	};


	class COMMONTOOLS_API LoggerManager
	{
	public:
		static LoggerManager& GetInstance();

		std::shared_ptr<Logger> GetLogger(const LogName& name);

		std::vector<LogName> LoggerNames() const;

	private:
		LoggerManager();

		~LoggerManager();

		LoggerManager(const LoggerManager&) = delete;

		LoggerManager& operator=(const LoggerManager&) = delete;

		bool createLogger(const LogName& name);

		bool createLogger(const Logger::Config& config);

		std::string LogNameToStr(const LogName& name);

	public:
		void AddCleanupDirectory(const std::string& path, int days);

	private:
		void CleanupThread();

		std::unordered_map<LogName, std::shared_ptr<Logger>> loggers_; // 日志器映射
		mutable std::mutex logger_mutex_;

		std::unordered_map<std::string, int> cleanup_map_; // <日志路径，保留天数>
		std::atomic<bool> stop_cleanup_thread_{false}; // 停止线程标志
		std::thread cleanup_thread_; // 后台清理线程
		std::mutex cleanup_mutex_; // 保护清理配置
	};


	//TODO:如何区分模组，不同相机类型
#define LOG common_tools::LoggerManager::GetInstance()
#define LOG_MOUNT(level, format, ...) LOG.GetLogger(common_tools::LogName::MOUNT)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_MOTION(level, format, ...) LOG.GetLogger(common_tools::LogName::MOTION)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_DATA(level, format, ...) LOG.GetLogger(common_tools::LogName::DATA)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_OPTIMIZE(level, format, ...) LOG.GetLogger(common_tools::LogName::OPTIMIZE)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_SLAVE_CONTROL(level, format, ...) LOG.GetLogger(common_tools::LogName::SLAVE_CONTROL)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_CAMERA_TOP(level, format, ...) LOG.GetLogger(common_tools::LogName::CAMERA_TOP)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)

#pragma endregion

#pragma region 进度条信息（ProgressInfo）
	struct COMMONTOOLS_API ProgressInfo
	{
		bool show_ctrl = false;
		bool show_precent = false;
		uint32_t min_value = 0;
		uint32_t max_value = 100;
		uint32_t current_value = 0;
		float precent_value = 0.0f;
		std::string title_text;
		std::string tips_text;
	};
#pragma endregion

#pragma region 键值对配置类（KeyValueMap）
	class COMMONTOOLS_API KeyValueMap
	{
	public:
		KeyValueMap();

		KeyValueMap(const KeyValueMap& other);

		KeyValueMap& operator=(const KeyValueMap& other);

		~KeyValueMap();

		// set：键→区分大小写
		void set(const std::string& key, const bool& value);

		void set(const std::string& key, const int& value);

		void set(const std::string& key, const float& value);

		void set(const std::string& key, const double& value);

		void set(const std::string& key, const std::string& value);

		// get：键→区分大小写
		bool get(const std::string& key, bool default_value = false);

		int get(const std::string& key, int default_value = 0);

		float get(const std::string& key, float default_value = 0.0f);

		double get(const std::string& key, double default_value = 0.0);

		std::string get(const std::string& key, const std::string& default_value = "");

		std::string to_string(bool style = false) const; // 序列化
		void from_string(const std::string& str); // 反序列化
	private:
		struct Impl;
		std::unique_ptr<Impl> impl_;
		mutable std::mutex mutex_;
	};
#pragma endregion
}; // namespace common_tools

#pragma region 基础数据转换位数据
namespace bit32_tools //位操作工具（32位）
{
	/**
	 * @brief 按不同位存储至一个int32变量
	 * @param value 储存变量
	 * @param bit_idx 位索引
	 * @param bit_value 位索引对应的值
	*/
	void COMMONTOOLS_API Set(int& value, int bit_idx, bool bit_value);

	/**
	 * @brief 从一个int32变量中获取不同位储存的值
	 * @param value 储存变量
	 * @param bit_idx 位索引
	 * @return 位索引对应的值
	*/
	bool COMMONTOOLS_API Get(int value, int bit_idx);
}
#pragma endregion

#pragma region 字符串工具
namespace string_utils
{
	/**
	 * @brief 字符串分割
	 * @param [in] str - 将要分割的字符串
	 * @param [in] delimiter - 用于分割字符串的符号，支持单个字符。如 ','
	 * @param [in] number - 控制分割结果的最大数量
	 * @return 分割结果
	 **/
	std::vector<std::string> COMMONTOOLS_API Split(const std::string& str, char delimiter, int pad_number = 0);

	/**
	 * @brief 字符串分割
	 * @param [in] str - 将要分割的字符串
	 * @param [in] delimiters - 用于分割字符串的符号，支持组合字符。如 ",\\/"
	 * @param [in] number - 控制分割结果的最大数量（=0时，不控制数量；>0时，仅当最大数量number大于分割结果数量时有效）
	 * @return 分割结果
	 **/
	std::vector<std::string> COMMONTOOLS_API Split(const std::string& str, std::string delimiters, int pad_number = 0);

	/**
	 * @brief
	 * @param [in] list - 将要合并的字符串列表
	 * @param [in] delimiter - 用于分割字符串的符号
	 * @return 合并结果
	 **/
	std::string COMMONTOOLS_API Merge(const std::vector<std::string>& list, char delimiter);

	/**
	 * @brief
	 * @param [in] list - 将要合并的字符串列表
	 * @param [in] delimiter - 用于分割字符串的符号
	 * @return 合并结果
	 **/
	std::string COMMONTOOLS_API Merge(const std::vector<std::string>& list, std::string delimiters);

	/**
	 * @brief 字符串格式化
	 * @param [in] format - 格式化参数
	 * @return 格式化结果
	 **/
	std::string COMMONTOOLS_API Format(const char* format, ...);

	/**
	 * @brief 字符串格式化
	 * @param [out] out - 格式化字符串
	 * @param [in] format - 格式化参数
	 * @return 格式化结果
	 **/
	void COMMONTOOLS_API Format(std::string& out, const char* format, ...);

	/**
	 * @brief
	 * @param [in] value - bool，short，int，float，double等常规数据类型的值
	 * @param [in] precision - 浮点数时小数有效位数
	 * @return 字符串数值
	 **/
	template <typename T>
	std::string ToString(const T& value, int precision)
	{
		std::ostringstream oss;
		if (typeid(value) == typeid(float) || typeid(value) == typeid(double) || typeid(value) == typeid(long double))
		{
			if (precision > -1)
				oss << std::setprecision(precision) << value;
			else
				oss << value;
		}
		else
			oss << value;
		return oss.str();
	}

	/**
	 * @brief GBK 转 UTF-8
	 * @param [in] gbk - 转换前 GBK 原始字符串
	 * @return 转换后 UTF-8 字符串
	 **/
	std::string COMMONTOOLS_API G2U(const std::string& gbk);

	/**
	* @brief UTF-8 转 GBK
	* @param [in] utf8 - 转换前 UTF-8 原始字符串
	* @return 转换后 GBK 字符串
	**/
	std::string COMMONTOOLS_API U2G(const std::string& utf8);

	/**
	* @brief 字符串移除左侧空白字符
	* @param [in] str - 原始字符串
	* @return 移除后的字符串
	**/
	std::string COMMONTOOLS_API TrimLeft(const std::string& str);

	/**
	* @brief 字符串移除右侧空白字符
	* @param [in] str - 原始字符串
	* @return 移除后的字符串
	**/
	std::string COMMONTOOLS_API TrimRight(const std::string& str);

	/**
	* @brief 字符串移除首尾空白字符
	* @param [in] str - 原始字符串
	* @return 移除后的字符串
	**/
	std::string COMMONTOOLS_API Trim(const std::string& str);

	/**
	* @brief 字符串移除首尾自定义字符集
	* @param [in] str - 原始字符串
	* @param [in] chars -
	* @return 移除后的字符串
	**/
	std::string COMMONTOOLS_API Trim(const std::string& str, const std::string& chars);

	/**
	* @brief 字符串大写
	* @param [in] str - 原始字符串
	* @return 大写后的字符串
	**/
	std::string COMMONTOOLS_API ToUpper(const std::string& str);

	/**
	* @brief 字符串小写
	* @param [in] str - 原始字符串
	* @return 小写后的字符串
	**/
	std::string COMMONTOOLS_API ToLower(const std::string& str);

	/**
	* @brief 字符串中子串替换
	* @param [in] str - 原始字符串
	* @param [in] old_str - 替换前子串
	* @param [in] new_str - 替换后子串
	* @return 结果字符串
	**/
	std::string COMMONTOOLS_API Repalce(const std::string& str, const std::string& old_str, const std::string& new_str);

	/**
	* @brief 基础类型数组数据拼接为字符串（bool，int，float...）
	* @param [in] array - 数组数据
	* @param [in] length - 数组长度
	* @param [in] delimiter - 拼接分隔符
	* @return 拼接字符串
	**/
	template <typename T>
	std::string ArrayToString(const T* array, const int& length, const std::string& delimiter)
	{
		std::ostringstream oss;
		for (int i = 0; i < length; ++i)
		{
			if (std::is_same<T, bool>::value)
				oss << static_cast<int>(array[i]);
			else
				oss << array[i];
			if (i < length - 1)
				oss << delimiter.c_str();
		}
		return oss.str();
	}

	/**
	* @brief 基础类型数组数据拼接为字符串（自动推导数组长度）
	* @param [in] array - 数组数据
	* @param [in] delimiter - 拼接分隔符
	* @return 拼接字符串
	**/
	template <typename T, size_t N>
	std::string ArrayToString(const T (&array)[N], const std::string& delimiter)
	{
		return ArrayToString(array, N, delimiter);
	}
}
#pragma endregion

#pragma region 文件目录操作
namespace file_system
{
	/**
	 * @brief 检查文件或目录是否存在
	 * @param [in] path - 文件路径或目录路径
	 * @return 检查结果
	 **/
	bool COMMONTOOLS_API Exists(const std::string& path);

	/**
	 * @brief 检查是否为文件
	 * @param [in] path - 文件路径
	 * @return 检查结果
	 **/
	bool COMMONTOOLS_API IsFile(const std::string& path);

	/**
	 * @brief 创建文件
	 * @param [in] path - 文件路径
	 * @return 创建结果
	 **/
	bool COMMONTOOLS_API CreateFileX(const std::string& path);

	/**
	 * @brief 重命名文件
	 * @param [in] src_path - 原文件路径
	 * @param [in] dst_path - 新文件路径
	 * @return 重命名结果
	 **/
	bool COMMONTOOLS_API RenameFile(const std::string& src_path, const std::string& dst_path);

	/**
	 * @brief 拷贝文件
	 * @param [in] src_path - 原文件路径
	 * @param [in] dst_path - 新文件路径
	 * @return 拷贝结果
	 **/
	bool COMMONTOOLS_API CopyFileX(const std::string& src_path, const std::string& dst_path);

	/**
	 * @brief 移动文件
	 * @param [in] src_path - 原文件路径
	 * @param [in] dst_path - 新文件路径
	 * @return 移动结果
	 **/
	bool COMMONTOOLS_API MoveFileX(const std::string& src_path, const std::string& dst_path);

	/**
	 * @brief 删除文件
	 * @param [in] path - 文件路径
	 * @return 删除结果
	 **/
	bool COMMONTOOLS_API DeleteFileX(const std::string& path);

	/**
	 * @brief 获取目录下所有文件或获取指定格式文件
	 * @param [in] path - 文件路径
	 * @param [in] extension - 扩展名为空时，获取目录下全部文件；扩展名不为空时，获取指定格式文件（如：".txt"）
	 * @return 文件列表结果
	 **/
	std::vector<std::string> GetFiles(const std::string& path, const std::string& extension);

	/**
	 * @brief 获取文件大小（字节单位）
	 * @param [in] path - 文件路径
	 * @return 文件大小
	 **/
	size_t COMMONTOOLS_API GetFileSize(const std::string& path);

	/**
	 * @brief 获取文件创建时间
	 * @param [in] path - 文件路径
	 * @return 创建时间
	 **/
	time_t COMMONTOOLS_API GetFileCreateTime(const std::string& path);

	/**
	 * @brief 获取文件修改时间
	 * @param [in] path - 文件路径
	 * @return 修改时间
	 **/
	time_t COMMONTOOLS_API GetFileModifiedTime(const std::string& path);

	/**
	 * @brief 获取文件名（不包含路径）
	 * @param [in] path - 文件路径
	 * @return 文件名
	 **/
	std::string COMMONTOOLS_API GetFileName(const std::string& path);

	/**
	 * @brief 获取文件路径（不包含文件名）
	 * @param [in] path - 文件路径
	 * @return 文件路径
	 **/
	std::string COMMONTOOLS_API GetFilePath(const std::string& path);

	/**
	 * @brief 获取文件扩展名
	 * @param [in] path - 文件路径
	 * @return 文件扩展名
	 **/
	std::string COMMONTOOLS_API GetFileExtensionName(const std::string& path);

	/**
	 * @brief 检查是否为目录
	 * @param [in] path - 目录路径
	 * @return 检查结果
	 **/
	bool COMMONTOOLS_API IsDirectory(const std::string& path);

	/**
	 * @brief 逐级创建目录
	 * @param [in] path - 目录路径
	 * @return 创建结果
	 **/
	bool COMMONTOOLS_API CreateDirectorys(const std::string& path);

	/**
	 * @brief 逐级删除目录
	 * @param [in] path - 目录路径
	 * @return 删除结果
	 **/
	bool COMMONTOOLS_API DeleteDirectorys(const std::string& path);

	/**
	 * @brief 获取目录下所有子目录
	 * @param [in] path - 目录路径
	 * @return 目录列表
	 **/
	std::vector<std::string> COMMONTOOLS_API GetDirectorys(const std::string& path);

	/**
	 * @brief 获取实例当前工作目录
	 * @param [in] path - 目录路径
	 * @return 工作目录
	 **/
	std::string COMMONTOOLS_API GetCurrentWorkDirectory();

	/**
	 * @brief 设置实例当前工作目录
	 * @param [in] path - 目录路径
	 * @return 设置结果
	 **/
	bool COMMONTOOLS_API SetCurrentWorkDirectory(const std::string& path);

	/**
	 * @brief 读取指定文件全部内容
	 * @param [in] path - 文件路径
	 * @return 读取内容
	 **/
	std::string COMMONTOOLS_API ReadAllText(const std::string& path);

	/**
	 * @brief 全部内容写入指定文件
	 * @param [in] path - 文件路径
	 * @return 写入结果
	 **/
	bool COMMONTOOLS_API WriteAllText(const std::string& path, const std::string& text);
}
#pragma endregion

#pragma region 文件编码检查
namespace file_encoding
{
	enum class Encoding
	{
		GBK, // ANSI 中文编码
		UTF8, // UTF-8 无BOM
		UTF8_BOM, // UTF-8 带BOM
		UTF16_LE, // Windows Unicode
		UTF16_BE, // 大端Unicode
		UNKNOWN
	};


	Encoding COMMONTOOLS_API get(const std::string& file_path);

	void COMMONTOOLS_API set(const Encoding& encoding);
}
#pragma endregion

//#pragma region WEB视图
//namespace web_viewer
//{
//	void COMMONTOOLS_API show_web_by_str(const std::string& str, const std::string& web_title = "");
//
//	void COMMONTOOLS_API show_web_by_txt(const std::string& file_path, const std::string& web_title = "");
//
//	void COMMONTOOLS_API show_web_by_csv(const std::string& file_path, const std::string& web_title = "");
//
//	void COMMONTOOLS_API show_web_by_html(const std::string& file_path, const std::string& web_title = "");
//}
//#pragma endregion

#pragma region 高精度时间戳
namespace timestamp
{
	int64_t COMMONTOOLS_API get_current_time_us(); // 微秒级时间戳
	int64_t COMMONTOOLS_API get_current_time_ms(); // 毫秒级时间戳
	int64_t COMMONTOOLS_API get_current_time_ss(); // 秒级时间戳

	int64_t COMMONTOOLS_API get_interval_time_us(int64_t start_us); // 获取微秒级时间间隔。形参为开始记录的微秒时间戳
	int64_t COMMONTOOLS_API get_interval_time_ms(int64_t start_ms); // 获取毫秒级时间间隔。形参为开始记录的毫秒时间戳
	int64_t COMMONTOOLS_API get_interval_time_ss(int64_t start_ss); // 获取秒级时间间隔。形参为开始记录的秒时间戳
}
#pragma endregion

#pragma region 获取名称/获取类型
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

#pragma endregion

// 扩展C++17/17+才支持的接口
#pragma region STD标准库
namespace std
{
	/**
	 * @brief 数值范围限制函数
	 * @param [in] val - 待限制原始值
	 * @param [in] lo - 区间下限
	 * @param [in] hi - 区间上限
	 * @return T 被限制在[lo, hi] 范围内的值
	 * @remark 需保证 lo <= hi，否则结果不符合预期
	 */
	template <typename T>
	constexpr T clamp(T val, T lo, T hi)
	{
		return val < lo ? lo : (val > hi ? hi : val);
	}

	/*template <typename T1, typename T2>
	inline auto safe_min(const T1& a, const T2& b)
	{
		return (a < b) ? a : b;
	}

	template <typename T1, typename T2>
	inline auto safe_max(const T1& a, const T2& b)
	{
		return (a > b) ? a : b;
	}*/
}
#pragma endregion
