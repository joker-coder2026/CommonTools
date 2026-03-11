#pragma once
#include <chrono>
#include <stdexcept>
#include <iostream>
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

// ======== 前置声明 ========
struct sqlite3;
struct sqlite3_stmt;
struct sqlite3_blob;

namespace spdlog
{
	class logger;
}

// ======== 前置声明 ========

// 局部抑制 C4251 警告
#pragma warning(push)
#pragma warning(disable: 4251)
namespace CommonTools
{
#pragma region BitTools
	// 位操作工具模板类（模板参数：BitWidth=32/64）
	// 模板参数仅支持 32 / 64 位，非法位宽（如 16）会在编译期报错，避免运行时错误；
	// 类型萃取自动匹配整数类型，无需手动指定，减少使用错误。
	template<int BitWidth>
	class COMMONTOOLS_API BitTools
	{
	public:
		// 类型萃取：根据位宽自动匹配整数类型
		using ValueType = typename std::conditional<BitWidth == 32, int, int64_t>::type;
		using UnsignedType = typename std::conditional<BitWidth == 32, uint32_t, uint64_t>::type;

		// ========== 核心接口（32/64位通用） ==========
		// 设置指定位（0/1）
		static void Set(ValueType& value, int bit_idx, bool bit_value);
		// 获取指定位值
		static bool Get(ValueType value, int bit_idx);
		// 翻转指定位
		static void Toggle(ValueType& value, int bit_idx);
		// 判断指定位是否为1（语义封装）
		static bool IsSet(ValueType value, int bit_idx) { return Get(value, bit_idx); }
		// 统计置1位数（Brian Kernighan算法）
		static int CountSetBits(ValueType value) noexcept;

	private:
		// 编译期常量：位宽最大值（32位=31，64位=63）
		static constexpr int MaxBitIndex = BitWidth - 1;
		// 编译期常量：移位基础值（32位=1U，64位=1ULL）
		static constexpr UnsignedType ShiftBase = static_cast<UnsignedType>(1);

		// 校验位索引（内联+无符号比较，零开销）
		static inline void CheckBitIndex(int bit_idx)
		{
			if (static_cast<UnsignedType>(bit_idx) > MaxBitIndex)
			{
				throw std::out_of_range(
					"BitTools<" + std::to_string(BitWidth) + ">: bit index must be 0~"
					+ std::to_string(MaxBitIndex) + " (current: " + std::to_string(bit_idx) + ")"
				);
			}
		}
	};

	// 模板实例化声明（对外暴露32/64位版本，避免客户端手动实例化）
	template class COMMONTOOLS_API BitTools<32>;
	template class COMMONTOOLS_API BitTools<64>;

	// 类型别名（简化使用，语义更友好）
	using Bit32Tools = BitTools<32>;
	using Bit64Tools = BitTools<64>;
#pragma endregion

#pragma region StringUtils
	class COMMONTOOLS_API StringUtils
	{
	public:

		/**
		 * @brief
		 * @param [in] value - bool，short，int，float，double等常规数据类型的值
		 * @param [in] precision - 浮点数时小数有效位数
		 * @return 字符串数值
		 **/
		template <typename T>
		static std::string ToString(const T& value, int precision = -1);

		/**
		 * @brief 字符串分割
		 * @param [in] str - 将要分割的字符串
		 * @param [in] sep - 用于分割字符串的符号，支持单个字符。如 ',')
		 * @param [in] max_split_num - 控制分割结果的最大数量
		 * @return 分割结果
		 **/
		static std::vector<std::string> Split(const std::string& str, const char& sep, int max_split_num = -1);

		/**
		 * @brief 字符串分割
		 * @param [in] str - 将要分割的字符串
		 * @param [in] seps - 用于分割字符串的符号，支持组合字符。如 ",\\/"
		 * @param [in] max_split_num - 控制分割结果的最大数量（=0时，不控制数量；>0时，仅当最大数量number大于分割结果数量时有效）
		 * @return 分割结果
		 **/
		static std::vector<std::string> Split(const std::string& str, const std::string& seps, int max_split_num = -1);

		/**
		 * @brief
		 * @param [in] list - 将要合并的字符串列表
		 * @param [in] sep - 用于分割字符串的符号
		 * @return 合并结果
		 **/
		static std::string Merge(const std::vector<std::string>& list, const char& sep);

		/**
		 * @brief
		 * @param [in] list - 将要合并的字符串列表
		 * @param [in] seps - 用于分割字符串的符号
		 * @return 合并结果
		 **/
		static std::string Merge(const std::vector<std::string>& list, const std::string& seps);

		/**
		 * @brief 字符串格式化
		 * @param [in] format - 格式化参数
		 * @return 格式化结果
		 **/
		static std::string Format(const char* format, ...);

		/**
		 * @brief 字符串格式化
		 * @param [out] out - 格式化字符串
		 * @param [in] format - 格式化参数
		 * @return 格式化结果
		 **/
		static int Format(std::string& out, const char* format, ...);

		/**
		 * @brief GBK 转 UTF-8
		 * @param [in] gbk - 转换前 GBK 原始字符串
		 * @return 转换后 UTF-8 字符串
		 **/
		static std::string G2U(const std::string& gbk);

		/**
		* @brief UTF-8 转 GBK
		* @param [in] utf8 - 转换前 UTF-8 原始字符串
		* @return 转换后 GBK 字符串
		**/
		static std::string U2G(const std::string& utf8);

		/**
		* @brief 字符串移除左侧空白字符
		* @param [in] str - 原始字符串
		* @return 移除后的字符串
		**/
		static std::string TrimLeft(const std::string& str);

		/**
		* @brief 字符串移除右侧空白字符
		* @param [in] str - 原始字符串
		* @return 移除后的字符串
		**/
		static std::string TrimRight(const std::string& str);

		/**
		* @brief 字符串移除首尾空白字符
		* @param [in] str - 原始字符串
		* @return 移除后的字符串
		**/
		static std::string Trim(const std::string& str);

		/**
		* @brief 字符串移除首尾自定义字符集
		* @param [in] str - 原始字符串
		* @param [in] chars -
		* @return 移除后的字符串
		**/
		static std::string Trim(const std::string& str, const std::string& chars);

		/**
		* @brief 字符串大写
		* @param [in] str - 原始字符串
		* @return 大写后的字符串
		**/
		static std::string ToUpper(const std::string& str);

		/**
		* @brief 字符串小写
		* @param [in] str - 原始字符串
		* @return 小写后的字符串
		**/
		static std::string ToLower(const std::string& str);
	};
#pragma endregion

#pragma region TimeUtils
	class COMMONTOOLS_API TimePoint
	{
	public:
		explicit TimePoint(std::time_t timestamp = 0);
		explicit TimePoint(const std::chrono::system_clock::time_point& tp);

		int64_t ToTimeStamp();
		std::string ToString(const std::string& format);

		static TimePoint Now();
		static std::string ToString(int64_t timestamp, const std::string& format);
		static int64_t ToTimestamp(const std::string& timeStr, const std::string& format);
	private:
		std::chrono::system_clock::time_point time_point_;
		static void FormatMs(int ms, char* buf);
		static int ClampInt(int val, int min_val, int max_val);
	}; 
#pragma endregion

#pragma region FileSystem
	class COMMONTOOLS_API FileSystem
	{
	public:
		/**
		 * @brief 检查文件或目录是否存在
		 * @param [in] path - 文件路径或目录路径
		 * @return 检查结果
		 **/
		static bool Exists(const std::string& path);

		/**
		 * @brief 检查是否为文件
		 * @param [in] path - 文件路径
		 * @return 检查结果
		 **/
		static bool IsFile(const std::string& path);
		static bool CreateFileA(const std::string& path);

		/**
		 * @brief 创建文件
		 * @param [in] path - 文件路径
		 * @return 创建结果
		 **/
		static bool CreateFile(const std::string& path);

		/**
		 * @brief 重命名文件
		 * @param [in] srcpath - 原文件路径
		 * @param [in] dstpath - 新文件路径
		 * @return 重命名结果
		 **/
		static bool RenameFile(const std::string& srcpath, const std::string& dstpath);
		static bool CopyFileA(const std::string& srcpath, const std::string& dstpath);
		static bool MoveFileA(const std::string& srcpath, const std::string& dstpath);
		static bool DeleteFileA(const std::string& path);

		/**
		 * @brief 拷贝文件
		 * @param [in] srcpath - 原文件路径
		 * @param [in] dstpath - 新文件路径
		 * @return 拷贝结果
		 **/
		static bool CopyFile(const std::string& srcpath, const std::string& dstpath);

		/**
		 * @brief 移动文件
		 * @param [in] srcpath - 原文件路径
		 * @param [in] dstpath - 新文件路径
		 * @return 移动结果
		 **/
		static bool MoveFile(const std::string& srcpath, const std::string& dstpath);

		/**
		 * @brief 删除文件
		 * @param [in] path - 文件路径
		 * @return 删除结果
		 **/
		static bool DeleteFile(const std::string& path);

		/**
		 * @brief 获取目录下所有文件或获取指定格式文件
		 * @param [in] path - 文件路径
		 * @param [in] extension - 扩展名为空时，获取目录下全部文件；扩展名不为空时，获取指定格式文件（如：".txt"）
		 * @return 文件列表结果
		 **/
		static std::vector<std::string> GetFiles(const std::string& path, const std::string& extension);

		/**
		 * @brief 获取文件大小（字节单位）
		 * @param [in] path - 文件路径
		 * @return 文件大小
		 **/
		static size_t GetFileSize(const std::string& path);

		/**
		 * @brief 获取文件创建时间
		 * @param [in] path - 文件路径
		 * @return 创建时间
		 **/
		static time_t GetFileCreateTime(const std::string& path);

		/**
		 * @brief 获取文件修改时间
		 * @param [in] path - 文件路径
		 * @return 修改时间
		 **/
		static time_t GetFileModifiedTime(const std::string& path);

		/**
		 * @brief 获取文件名（不包含路径）
		 * @param [in] path - 文件路径
		 * @return 文件名
		 **/
		static std::string GetFileName(const std::string& path);

		/**
		 * @brief 获取文件路径（不包含文件名）
		 * @param [in] path - 文件路径
		 * @return 文件路径
		 **/
		static std::string GetFilePath(const std::string& path);

		/**
		 * @brief 获取文件扩展名
		 * @param [in] path - 文件路径
		 * @return 文件扩展名
		 **/
		static std::string GetFileExtensionName(const std::string& path);

		/**
		 * @brief 检查是否为目录
		 * @param [in] path - 目录路径
		 * @return 检查结果
		 **/
		static bool IsDirectory(const std::string& path);

		/**
		 * @brief 逐级创建目录
		 * @param [in] path - 目录路径
		 * @return 创建结果
		 **/
		static bool CreateDirectorys(const std::string& path);

		/**
		 * @brief 逐级删除目录
		 * @param [in] path - 目录路径
		 * @return 删除结果
		 **/
		static bool DeleteDirectorys(const std::string& path);

		/**
		 * @brief 获取目录下所有子目录
		 * @param [in] path - 目录路径
		 * @return 目录列表
		 **/
		static std::vector<std::string> GetDirectorys(const std::string& path);

		/**
		 * @brief 获取实例当前工作目录
		 * @param [in] path - 目录路径
		 * @return 工作目录
		 **/
		static std::string GetCurrentWorkDirectory();

		/**
		 * @brief 设置实例当前工作目录
		 * @param [in] path - 目录路径
		 * @return 设置结果
		 **/
		static bool SetCurrentWorkDirectory(const std::string& path);


		/**
		 * @brief 读取指定文件全部内容
		 * @param [in] path - 文件路径
		 * @return 读取内容
		 **/
		static std::string ReadAllText(const std::string& path);

		/**
		 * @brief 全部内容写入指定文件
		 * @param [in] path - 文件路径
		 * @return 写入结果
		 **/
		static bool WriteAllText(const std::string& path, const std::string& text);
	};
#pragma endregion

#pragma region IniManager
	class COMMONTOOLS_API IniManager
	{
		struct Convert
		{
			static std::string ToString(int value);
			static std::string ToString(bool value);
			static std::string ToString(double value);
			static int ToInt(const std::string& value, int defaultValue = 0);
			static bool ToBool(const std::string& value, bool defaultValue = false);
			static double ToDouble(const std::string& value, double defaultValue = 0.0);
		};

	public:
		explicit IniManager(const std::string& filePath);

		IniManager(const IniManager&) = delete;
		IniManager& operator=(const IniManager&) = delete;

		bool WriteValue(const std::string& section, const std::string& key, const std::string& value);
		std::string ReadValue(const std::string& section, const std::string& key, const std::string& defaultValue);

		bool WriteInt(const std::string& section, const std::string& key, int value);
		bool WriteBool(const std::string& section, const std::string& key, bool value);
		bool WriteDouble(const std::string& section, const std::string& key, double value);

		int ReadInt(const std::string& section, const std::string& key, int defaultValue = 0);
		bool ReadBool(const std::string& section, const std::string& key, bool defaultValue = false);
		double ReadDouble(const std::string& section, const std::string& key, double defaultValue = 0.0);

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

		std::string m_filePath;
		std::string m_lastError;
	};
#pragma endregion

#pragma region XmlManager
	class COMMONTOOLS_API XmlManager
	{
	public:
		XmlManager();
		~XmlManager();
	};
#pragma endregion

#pragma region JsonManager
	class COMMONTOOLS_API JsonManager
	{
	public:
		JsonManager();
		~JsonManager();
	};
#pragma endregion

#pragma region SQLServerManager
	class COMMONTOOLS_API SQLServerManager
	{
	};
#pragma endregion

#pragma region SqliteManager
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
			int intVal = 0;
			int64_t int64Val = 0;
			std::string strVal;
			double doubleVal = 0.0;
			std::vector<char> blobVal;

			SqliteParam(bool val) : type(ParamType::Bool), intVal(val ? 1 : 0)
			{
			}

			SqliteParam(int32_t val) : type(ParamType::Int), intVal(val)
			{
			}

			SqliteParam(uint32_t val) : type(ParamType::UInt), int64Val(static_cast<int64_t>(val))
			{
			}

			SqliteParam(int64_t val) : type(ParamType::Int64), int64Val(val)
			{
			}

			SqliteParam(uint64_t val) : type(ParamType::UInt64), int64Val(static_cast<int64_t>(val))
			{
			}

			SqliteParam(const std::string& val) : type(ParamType::String), strVal(val)
			{
			}

			SqliteParam(double val) : type(ParamType::Double), doubleVal(val)
			{
			}

			SqliteParam(const char* val) : type(ParamType::String), strVal(val)
			{
			}

			SqliteParam(const char* val, size_t len) : type(ParamType::Blob), blobVal(val, val + len)
			{
			}

			SqliteParam(const std::vector<char>& val) : type(ParamType::Blob), blobVal(val)
			{
			}

			SqliteParam(std::vector<char>&& val) : type(ParamType::Blob), blobVal(std::move(val))
			{
			}
		};

		using ParamsList = std::vector<SqliteParam>;
		using BatchParamsList = std::vector<ParamsList>;
		using UMapList = std::vector<std::unordered_map<std::string, std::string>>;

		SqliteManager() noexcept;
		~SqliteManager();
		explicit SqliteManager(const std::string& fileName);

		SqliteManager(SqliteManager&& other) noexcept;
		SqliteManager& operator=(SqliteManager&& other) noexcept;
		SqliteManager(const SqliteManager&) = delete;
		SqliteManager& operator=(const SqliteManager&) = delete;

		bool Open(const std::string& fileName);
		void Close();
		bool IsOpen() noexcept;

		void SetMaxStmtCacheCount(size_t count) noexcept;

		bool ExecuteNonQuery(const std::string& sql, const ParamsList& params = ParamsList());
		bool ExecuteBatchNonQuery(const std::string& sql, const BatchParamsList& paramsList); // 此函数内已使用事务

		bool ExecuteQuery(const std::string& sql, const ParamsList& params, UMapList& result);
		bool ExecuteQueryPage(const std::string& sql, const ParamsList& params, int currentPage, int pageSize,
		                      UMapList& data, int& totalCount, int& totalPages);

		bool ReadBlobChunk(const std::string& tableName, const std::string& colName, int64_t rowId, size_t offset,
		                   size_t chunkSize, std::vector<char>& chunkData);
		bool WriteBlobChunk(const std::string& tableName, const std::string& colName, int64_t rowId, size_t offset,
		                    const std::vector<char>& chunkData);

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

		sqlite3* m_db = nullptr; // 数据库句柄
		mutable std::recursive_mutex m_mutex; // 线程安全锁
		std::string m_lastErrMsg; // 错误信息
		std::string m_fileName; // 数据库路径
		size_t m_maxStmtCacheCount = 50; // 最大缓存语句数
		std::unordered_map<std::string, sqlite3_stmt*> m_stmtCache; // 预处理语句缓存
	};

#pragma endregion

#pragma region ThreadPool
	class COMMONTOOLS_API ThreadPool
	{
	public:
		explicit ThreadPool(size_t threads) : stop(false)
		{
			for (size_t i = 0; i < threads; ++i)
			{
				workers.emplace_back([this]
				{
					for (;;)
					{
						std::function<void()> task;

						{
							std::unique_lock<std::mutex> lock(this->queue_mutex);
							this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

							if (this->stop && this->tasks.empty())
								return;

							task = std::move(this->tasks.front());
							this->tasks.pop();
						}

						try
						{
							task();
						}
						catch (...)
						{
							// Handle exceptions thrown by tasks
							// Could add logging here if needed
						}
					}
				});
			}
		}

		~ThreadPool()
		{
			{
				std::unique_lock<std::mutex> lock(queue_mutex);
				stop = true;
			}
			condition.notify_all();
			for (std::thread& worker : workers)
				worker.join();
		}

		template <class F, class... Args>
		std::future<std::result_of_t<F(Args...)>> enqueue(F&& f, Args&&... args)
		{
			using return_type = std::result_of_t<F(Args...)>;

			auto task = std::make_shared<std::packaged_task<return_type()>>(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...));

			std::future<return_type> res = task->get_future();
			{
				std::unique_lock<std::mutex> lock(queue_mutex);

				// don't allow enqueueing after stopping the pool
				if (stop)
					throw std::runtime_error("enqueue on stopped ThreadPool");

				tasks.emplace([task]() { (*task)(); });
			}
			condition.notify_one();
			return res;
		}

	private:
		// need to keep track of threads so we can join them
		std::vector<std::thread> workers;

		// the task queue
		std::queue<std::function<void()>> tasks;

		// synchronization
		std::mutex queue_mutex;
		std::condition_variable condition;
		std::atomic<bool> stop;
	};
#pragma endregion

#pragma region CustomSettings
	//（CS → CustomSettings）

	// 数据类型
	enum class CSType { Null, Int, Double, String };

	// 键访问器
	class COMMONTOOLS_API CSKey
	{
	public:
		CSKey(class CSImpl* impl, const std::string& fileName, const std::string& section, const std::string& key);
		CSKey(class CSImpl* impl, const std::string& fileName, const std::string& section);
		CSKey operator[](const std::string& key);

		CSKey& SetInt(const int& value);
		CSKey& SetDouble(const double& value);
		CSKey& SetString(const std::string& value);
		CSKey& SetDescription(const std::string& value);

		int GetInt(const int& defaultValue = 0) const;
		double GetDouble(const double& defaultValue = 0.0) const;
		std::string GetString(const std::string& defaultValue = "") const;
		std::string GetDescription(const std::string& defaultValue = "") const;
		CSType GetType() const;

	private:
		class CSImpl* m_impl;
		std::string m_fileName;
		std::string m_section;
		std::string m_key;
	};

	// 节访问器
	class COMMONTOOLS_API CSSection
	{
	public:
		CSSection(class CSImpl* impl, const std::string& fileName);
		CSKey operator[](const std::string& section);

	private:
		class CSImpl* m_impl;
		std::string m_fileName;
	};

	// 自定义配置类
	class COMMONTOOLS_API CustomSettings
	{
	public:
		struct COMMONTOOLS_API Members
		{
			std::string fileName; // 文件名
			std::string section; // 节点
			std::string key; // 键名
			std::string value; // 值
			std::string description; // 描述
		};

		static CustomSettings& GetInstance();

		CSSection operator[](const std::string& fileName);

		CSKey Access(const std::string& fileName, const std::string& section, const std::string& key);

		std::vector<std::string> GetJsonFileList();

		std::string JsonToString() const;

		std::string GetLastErrorMsg() const;

		bool LoadJsonFile(const std::string& fileName);
		bool SaveJsonFile(const std::string& fileName, const bool& isNewFile = false);
		bool DeleteJsonFile(const std::string& fileName);
		bool RenameJsonFile(const std::string& oldFileName, const std::string& newFileName);

		bool LoadJsonFiles(); // 加载所有文件
		bool SaveJsonFiles(); // 保存所有文件

		bool JsonToVector(const std::string& fileName, std::vector<Members>& currentFileData);
		bool VectorToJson(const std::string& fileName, const std::vector<Members>& currentFileData);

		// 移除JSON下对象
		// objectPath 格式：fileName/section/key → "demo.json/test/enable"
		bool RemoveJsonObject(const std::string& objectPath);

	private:
		CustomSettings();
		~CustomSettings();

		CustomSettings(const CustomSettings&) = delete;
		CustomSettings(const CustomSettings&&) = delete;
		CustomSettings operator =(const CustomSettings&) = delete;

		class CSImpl* m_impl;
	};

#define CFG CommonTools::CustomSettings::GetInstance()

#pragma endregion

#pragma region LoggerManager
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
			LogName logName = LogName::DEFAULT; // 日志器名称
			std::string filePath = "d:/log"; // 文件路径
			std::string fileName = "default.ini"; // 文件名

			LogLevel level = LogLevel::Off; // 日志级别
			LogOutput outputs = LogOutput::None; // 日志输出目标

			std::size_t maxFileSize = 10 * 1024 * 1024; // 按大小滚动时的文件大小限制
			std::size_t maxFileCount = 100; // 按大小滚动时的文件数量限制

			bool enableDatabase = true; // 启用写日志数据库

			Config() = default;
			explicit Config(const LogName& name);
		};

		struct COMMONTOOLS_API MetaMsg
		{
			std::chrono::system_clock::time_point time;
			LogName logger_name; // 日志器名
			size_t thread_id = 0;
			LogOutput output = LogOutput::None;
			LogLevel level = LogLevel::Off;
			const char* file = nullptr; // 文件名（可能为nullptr）
			int line = 0; // 行号（可能为0）
			const char* func = nullptr; // 函数名（可能为nullptr）
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

		Config m_config;

		std::map<LogOutput, std::function<void(const MetaMsg&)>> m_customCallbacks;
		std::atomic<bool> m_initialized;

		std::unordered_map<LogOutput, std::shared_ptr<spdlog::logger>> m_loggerCache;
		std::mutex m_cacheMutex;
		std::chrono::steady_clock::time_point m_lastCleanupTime;
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

		std::atomic<bool> m_running{false};
		std::mutex m_queueMutex;
		std::condition_variable m_cv;
		std::queue<Logger::MetaMsg> m_queueMsg;
		std::unordered_set<std::queue<Logger::MetaMsg>*> m_queues;

		std::mutex m_dbMutex; // 数据库锁
		std::shared_ptr<SqliteManager> m_db; // 当前数据库实例
		std::string m_dbLastDate; // 当前数据库日期
	};


	class COMMONTOOLS_API LoggerManager
	{
	public:
		static LoggerManager& GetInstance();

		std::shared_ptr<Logger> logger(const LogName& name);
		std::vector<LogName> loggerNames() const;

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

		std::unordered_map<LogName, std::shared_ptr<Logger>> m_loggers; // 日志器映射
		mutable std::mutex m_loggerMutex;

		std::unordered_map<std::string, int> m_cleanupMap; // <日志路径，保留天数>
		std::atomic<bool> m_stopCleanupThread{false}; // 停止线程标志
		std::thread m_cleanupThread; // 后台清理线程
		std::mutex m_cleanupMutex; // 保护清理配置
	};

	//TODO:如何区分模组，不同相机类型
#define LOG CommonTools::LoggerManager::GetInstance()
#define LOG_MOUNT(level, format, ...) LOG.logger(CommonTools::LogName::MOUNT)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_MOTION(level, format, ...) LOG.logger(CommonTools::LogName::MOTION)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_DATA(level, format, ...) LOG.logger(CommonTools::LogName::DATA)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_OPTIMIZE(level, format, ...) LOG.logger(CommonTools::LogName::OPTIMIZE)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_SLAVE_CONTROL(level, format, ...) LOG.logger(CommonTools::LogName::SLAVE_CONTROL)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)
#define LOG_CAMERA_TOP(level, format, ...) LOG.logger(CommonTools::LogName::CAMERA_TOP)->LogRecord(__FILE__, __LINE__, __FUNCTION__, level, format, ##__VA_ARGS__)

#pragma endregion

#pragma region 进度条信息
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
}; // namespace CommonTools

// 恢复警告状态（避免全局禁用）
#pragma warning(pop)