#include "CommonTools.h"

#include <fstream>
#include <cctype>
#include <algorithm>
#include <direct.h>   // for _mkdir, _rmdir
#include <corecrt_io.h> // for _access
#include <regex>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>

#include <windows.h>
#include <Shellapi.h>

//#include "tinyxml.h"
#include "json/json.h"
#include "sqlite3.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/async.h"
#include "spdlog/common.h"

#include <Shlwapi.h>
//#include <XTrace.h>
#pragma comment(lib, "Shlwapi.lib")
namespace common_tools
{
#pragma region INI配置文件管理类（IniManager）
	bool ContainsDriveLetter(const std::string& path)
	{
		if (path.length() >= 2 && std::isalpha(path[0]) && path[1] == ':')
		{
			return true; // 例如 "C:"
		}
		return false;
	}

	bool ContainsPathSeparator(const std::string& path)
	{
		if (path.find('\\') != std::string::npos)
		{
			return true;
		}
		if (path.find('/') != std::string::npos)
		{
			return true;
		}
		return false;
	}

	std::string GetExecutablePath()
	{
		char buffer[MAX_PATH];
		::GetModuleFileName(nullptr, buffer, MAX_PATH);

		std::string path(buffer);
		size_t lastPos = path.find_last_of("\\/");
		if (lastPos != std::string::npos)
		{
			path = path.substr(0, lastPos);
		}
		return path;
	}

	IniManager::IniManager(const std::string& file_path)
	{
		SetLastError("");
		file_path_ = "";

		if (ContainsDriveLetter(file_path) && ContainsPathSeparator(file_path))
		{
			file_path_ = file_path;
		}
		else
		{
			std::string current_path = GetExecutablePath();
			file_path_ = current_path + "\\" + file_path;
		}
	}

	bool IniManager::WriteValue(const std::string& section, const std::string& key, const std::string& value)
	{
		SetLastError("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), key.c_str(), value.c_str(), file_path_.c_str());
		if (!result)
		{
			SetLastError("Failed to write value to INI file");
		}
		return result != 0;
	}

	std::string IniManager::ReadValue(const std::string& section, const std::string& key,
	                                  const std::string& default_value)
	{
		SetLastError("");

		char buffer[256] = {0};
		DWORD size = ::GetPrivateProfileString(section.c_str(), key.c_str(), default_value.c_str(), buffer,
		                                       sizeof(buffer), file_path_.c_str());
		if (size == 0 && !SectionExists(section))
		{
			SetLastError("Section not found in INI file");
		}
		return buffer;
	}

	bool IniManager::WriteInt(const std::string& section, const std::string& key, int value)
	{
		return WriteValue(section, key, ToString(value));
	}

	bool IniManager::WriteBool(const std::string& section, const std::string& key, bool value)
	{
		return WriteValue(section, key, ToString(value));
	}

	bool IniManager::WriteDouble(const std::string& section, const std::string& key, double value)
	{
		return WriteValue(section, key, ToString(value));
	}

	int IniManager::ReadInt(const std::string& section, const std::string& key, int default_value)
	{
		return FromString(ReadValue(section, key, ""), default_value);
	}

	bool IniManager::ReadBool(const std::string& section, const std::string& key, bool default_value)
	{
		return FromString(ReadValue(section, key, ""), default_value);
	}

	double IniManager::ReadDouble(const std::string& section, const std::string& key, double default_value)
	{
		return FromString(ReadValue(section, key, ""), default_value);
	}

	std::map<std::string, std::string> IniManager::ReadSection(const std::string& section)
	{
		SetLastError("");

		std::map<std::string, std::string> result;

		if (!SectionExists(section))
		{
			SetLastError("Section not found in INI file");
			return result;
		}

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSection(section.c_str(), buffer, sizeof(buffer), file_path_.c_str());

		if (size == 0)
			return result;

		char* p = buffer;
		while (*p)
		{
			std::string line(p);
			size_t pos = line.find('=');
			if (pos != std::string::npos)
			{
				std::string key = line.substr(0, pos);
				std::string value = line.substr(pos + 1);
				result[key] = value;
			}
			p += strlen(p) + 1;
		}

		return result;
	}

	bool IniManager::WriteSection(const std::string& section, const std::map<std::string, std::string>& keyValues)
	{
		SetLastError("");

		// 先删除现有 section
		if (!DeleteSection(section))
		{
			SetLastError("Failed to clear existing section");
			return false;
		}

		// 写入新的键值对
		for (auto it = keyValues.begin(); it != keyValues.end(); ++it)
		{
			if (!WriteValue(section, it->first, it->second))
			{
				SetLastError("Failed to write key-value pair");
				return false;
			}
		}

		return true;
	}

	bool IniManager::DeleteKey(const std::string& section, const std::string& key)
	{
		SetLastError("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), key.c_str(), nullptr, file_path_.c_str());
		if (!result)
		{
			SetLastError("Failed to delete key from INI file");
		}
		return result != 0;
	}

	bool IniManager::DeleteSection(const std::string& section)
	{
		SetLastError("");

		BOOL result = ::WritePrivateProfileString(section.c_str(), nullptr, nullptr, file_path_.c_str());
		if (!result)
		{
			SetLastError("Failed to delete section from INI file");
		}
		return result != 0;
	}

	bool IniManager::FileExists()
	{
		SetLastError("");

		std::ifstream file(file_path_);
		bool exist = file.good();
		if (file.is_open())
			file.close();
		return exist;
	}

	bool IniManager::BackupFile(const std::string& backupPath)
	{
		SetLastError("");

		std::string file_path(file_path_);

		std::string actual_backup_path = backupPath;
		if (actual_backup_path.empty())
		{
			size_t pos = file_path.rfind('.');
			if (pos != std::string::npos)
			{
				actual_backup_path = file_path.substr(0, pos) + ".bak" + file_path.substr(pos);
			}
			else
			{
				actual_backup_path = file_path + ".bak";
			}
		}

		BOOL result = ::CopyFile(file_path.c_str(), actual_backup_path.c_str(), FALSE); // 覆盖已存在的文件
		if (!result)
		{
			SetLastError("Failed to backup INI file");
		}

		return result != 0;
	}

	bool IniManager::SectionExists(const std::string& section)
	{
		SetLastError("");

		std::vector<std::string> sections = GetSectionNames();
		return std::find(sections.begin(), sections.end(), section) != sections.end();
	}

	bool IniManager::KeyExists(const std::string& section, const std::string& key)
	{
		SetLastError("");

		std::vector<std::string> keys = GetKeyNames(section);
		return std::find(keys.begin(), keys.end(), key) != keys.end();
	}

	std::vector<std::string> IniManager::GetSectionNames()
	{
		SetLastError("");

		std::vector<std::string> sections;

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSectionNames(buffer, sizeof(buffer), file_path_.c_str());

		if (size == 0)
			return sections;

		char* p = buffer;
		while (*p)
		{
			sections.push_back(std::string(p));
			p += strlen(p) + 1;
		}

		return sections;
	}

	std::vector<std::string> IniManager::GetKeyNames(const std::string& section)
	{
		SetLastError("");

		std::vector<std::string> keys;

		char buffer[32768] = {0};
		DWORD size = ::GetPrivateProfileSection(section.c_str(), buffer, sizeof(buffer), file_path_.c_str());

		if (size == 0)
			return keys;

		char* p = buffer;
		while (*p)
		{
			std::string line(p);
			size_t pos = line.find('=');
			if (pos != std::string::npos)
			{
				keys.push_back(line.substr(0, pos));
			}
			p += strlen(p) + 1;
		}

		return keys;
	}

	std::string IniManager::GetLastError()
	{
		return last_error_;
	}

	void IniManager::SetLastError(const std::string& error)
	{
		last_error_ = error;
	}
#pragma endregion

#pragma region XML配置文件管理类（XmlManager）
	XmlManager::XmlManager()
	{
	}

	XmlManager::~XmlManager()
	{
	}
#pragma endregion

#pragma region  JSON配置文件管理类（JsonManager）
	JsonManager::JsonManager()
	{
	}

	JsonManager::~JsonManager()
	{
	}

#pragma endregion

#pragma region SQLServer配置文件管理类（SQLServerManager）
	// 暂未实现
#pragma endregion

#pragma region Sqlite配置文件管理类（SqliteManager）

	SqliteManager::SqliteManager() noexcept = default;

	SqliteManager::SqliteManager(const std::string& file_name)
	{
		Open(file_name);
	}

	SqliteManager::~SqliteManager()
	{
		Close();
	}

	SqliteManager::SqliteManager(SqliteManager&& other) noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(other.mutex_);
		db_ = other.db_;
		last_error_ = std::move(other.last_error_);
		file_name_ = std::move(other.file_name_);
		stmt_cache_max_count_ = other.stmt_cache_max_count_;
		stmt_cache_ = std::move(other.stmt_cache_);

		other.db_ = nullptr;
		other.stmt_cache_.clear();
	}

	SqliteManager& SqliteManager::operator=(SqliteManager&& other) noexcept
	{
		if (this != &other)
		{
			std::lock_guard<std::recursive_mutex> lock_self(mutex_);
			std::lock_guard<std::recursive_mutex> lock_other(other.mutex_);

			Close();
			db_ = other.db_;
			last_error_ = std::move(other.last_error_);
			file_name_ = std::move(other.file_name_);
			stmt_cache_max_count_ = other.stmt_cache_max_count_;
			stmt_cache_ = std::move(other.stmt_cache_);

			other.db_ = nullptr;
			other.stmt_cache_.clear();
		}
		return *this;
	}

	void SqliteManager::SetMaxStmtCacheCount(size_t count) noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		stmt_cache_max_count_ = count;
		// 超过新上限则清理缓存
		while (stmt_cache_.size() > stmt_cache_max_count_ && !stmt_cache_.empty())
		{
			auto it = stmt_cache_.begin();
			sqlite3_finalize(it->second);
			stmt_cache_.erase(it);
		}
	}

	bool SqliteManager::CheckConnection() noexcept
	{
		last_error_.clear();
		if (!db_)
		{
			last_error_ = "Database is nullptr";
			return false;
		}

		sqlite3_stmt* stmt = nullptr;
		int rc = sqlite3_prepare_v2(db_, "SELECT 1;", -1, &stmt, nullptr);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			if (stmt)
				sqlite3_finalize(stmt); // 手动释放
			return false;
		}

		rc = sqlite3_step(stmt);
		if (rc != SQLITE_ROW && rc != SQLITE_DONE)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_finalize(stmt); // 手动释放
			return false;
		}

		sqlite3_finalize(stmt); // 手动释放
		return true;
	}

	bool SqliteManager::TryReconnect() noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (file_name_.empty())
		{
			last_error_ = "No database path for reconnect";
			return false;
		}

		if (db_)
		{
			Close();
		}

		// 重新打开
		int rc = sqlite3_open_v2(file_name_.c_str(), &db_,
		                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_close(db_);
			db_ = nullptr;
			return false;
		}

		// 重连后清空缓存
		ClearStmtCache();
		return true;
	}

	bool SqliteManager::Open(const std::string& file_name)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		file_name_ = file_name;
		if (db_)
		{
			if (CheckConnection())
			{
				last_error_ = "Database already open";
				return false;
			}
			Close();
		}

		int rc = sqlite3_open_v2(file_name_.c_str(), &db_,
		                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_close(db_);
			db_ = nullptr;
			return false;
		}

		return true;
	}

	void SqliteManager::Close()
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (db_)
		{
			ClearStmtCache();
			sqlite3_close(db_);
			db_ = nullptr;
		}
	}

	bool SqliteManager::IsOpen() noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		bool connected = CheckConnection();
		return db_ != nullptr && connected == true;
	}

	sqlite3_stmt* SqliteManager::GetStmtCache(const std::string& sql)
	{
		last_error_.clear();
		auto it = stmt_cache_.find(sql);
		if (it != stmt_cache_.end() && it->second != nullptr)
		{
			sqlite3_reset(it->second); // 重置语句可复用
			return it->second;
		}
		return nullptr;
	}

	void SqliteManager::AddStmtCache(const std::string& sql, sqlite3_stmt* stmt)
	{
		last_error_.clear();
		if (!stmt)
		{
			last_error_ = "Database is nullptr";
			return;
		}

		// 超过最大缓存数，移除最早的
		while (stmt_cache_.size() >= stmt_cache_max_count_ && !stmt_cache_.empty())
		{
			auto it = stmt_cache_.begin();
			sqlite3_finalize(it->second);
			stmt_cache_.erase(it);
		}

		stmt_cache_[sql] = stmt;
	}

	void SqliteManager::ClearStmtCache()
	{
		stmt_cache_.clear();
		for (auto& pair : stmt_cache_)
		{
			sqlite3_finalize(pair.second);
		}
	}

	bool SqliteManager::BindParams(sqlite3_stmt* stmt, const ParamsList& params)
	{
		last_error_.clear();
		int idx = 1;
		for (const auto& param : params)
		{
			int rc = SQLITE_ERROR;
			switch (param.type)
			{
			case ParamType::Null:
				rc = sqlite3_bind_null(stmt, idx);
				break;
			case ParamType::Bool:
				rc = sqlite3_bind_int(stmt, idx, param.int_val);
				break;
			case ParamType::Int:
				rc = sqlite3_bind_int(stmt, idx, param.int_val);
				break;
			case ParamType::UInt:
				rc = sqlite3_bind_int64(stmt, idx, param.int64_val);
				break;
			case ParamType::Int64:
				rc = sqlite3_bind_int64(stmt, idx, param.int64_val);
				break;
			case ParamType::UInt64:
				rc = sqlite3_bind_int64(stmt, idx, param.int64_val);
				break;
			case ParamType::String:
				rc = sqlite3_bind_text(stmt, idx, param.str_val.c_str(), static_cast<int>(param.str_val.size()),
				                       SQLITE_TRANSIENT);
				break;
			case ParamType::Double:
				rc = sqlite3_bind_double(stmt, idx, param.double_val);
				break;
			case ParamType::Blob:
				if (param.blob_val.empty())
				{
					last_error_ = "Empty blob data";
					return false;
				}
				rc = sqlite3_bind_blob(stmt, idx, param.blob_val.data(), static_cast<int>(param.blob_val.size()),
				                       SQLITE_TRANSIENT);
				break;
			}

			if (rc != SQLITE_OK)
			{
				last_error_ = sqlite3_errmsg(db_);
				return false;
			}
			idx++;
		}
		return true;
	}

	bool SqliteManager::ExecutePreparedQuery(sqlite3_stmt* stmt, UMapList& result)
	{
		last_error_.clear();
		result.clear();
		int col_count = sqlite3_column_count(stmt);
		if (col_count == 0)
		{
			return true;
		}

		// 获取列名
		std::vector<std::string> col_names;
		for (int i = 0; i < col_count; ++i)
		{
			col_names.emplace_back(sqlite3_column_name(stmt, i));
		}

		// 遍历结果行
		int rc;
		while ((rc = sqlite3_step(stmt)) == SQLITE_ROW)
		{
			std::unordered_map<std::string, std::string> row;
			for (int i = 0; i < col_count; ++i)
			{
				if (sqlite3_column_type(stmt, i) == SQLITE_BLOB)
				{
					auto blob_data = reinterpret_cast<const uint8_t*>(sqlite3_column_blob(stmt, i));
					int blob_len = sqlite3_column_bytes(stmt, i);
					std::string hex_str;
					for (int j = 0; j < blob_len; ++j)
					{
						char buf[3];
						snprintf(buf, sizeof(buf), "%02X", blob_data[j]);
						hex_str += buf;
					}
					row[col_names[i]] = hex_str;
				}
				else
				{
					auto val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, i));
					row[col_names[i]] = val ? val : "";
				}
			}
			result.push_back(std::move(row));
		}

		if (rc != SQLITE_DONE)
		{
			last_error_ = sqlite3_errmsg(db_);
			return false;
		}
		return true;
	}

	bool SqliteManager::ExecuteNonQuery(const std::string& sql, const ParamsList& params)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_)
		{
			last_error_ = "Database not open";
			return false;
		}

		// 获取缓存语句
		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool is_cached = (stmt != nullptr);
		bool ret = false;

		if (!is_cached)
		{
			// 预处理新语句
			int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				last_error_ = sqlite3_errmsg(db_);
				return false;
			}
		}

		if (BindParams(stmt, params))
		{
			int rc = sqlite3_step(stmt);
			if (rc == SQLITE_DONE)
			{
				ret = true;
				last_error_.clear();
				if (!is_cached)
				{
					AddStmtCache(sql, stmt);
					stmt = nullptr; // 避免后续释放缓存语句
				}
			}
			else
			{
				last_error_ = sqlite3_errmsg(db_);
				// 缓存语句执行失败，移除缓存
				if (is_cached)
				{
					sqlite3_finalize(stmt);
					stmt = nullptr;
					stmt_cache_.erase(sql);
				}
			}
		}

		if (!is_cached && stmt != nullptr)
		{
			sqlite3_finalize(stmt);
			stmt = nullptr;
		}

		return ret;
	}

	bool SqliteManager::ExecuteBatchNonQuery(const std::string& sql, const BatchParamsList& params_list)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_ || params_list.empty())
		{
			last_error_ = "Database not open or empty batch params";
			return false;
		}

		if (!BeginTransaction())
		{
			return false;
		}

		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool is_cached = (stmt != nullptr);
		bool all_ok = true;

		if (!is_cached)
		{
			int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				last_error_ = sqlite3_errmsg(db_);
				RollbackTransaction();
				return false;
			}
		}

		for (const auto& params : params_list)
		{
			sqlite3_reset(stmt);
			if (!BindParams(stmt, params))
			{
				all_ok = false;
				break;
			}

			int rc = sqlite3_step(stmt);
			if (rc != SQLITE_DONE)
			{
				last_error_ = sqlite3_errmsg(db_);
				all_ok = false;
				break;
			}
		}

		if (all_ok)
		{
			CommitTransaction();
			if (!is_cached)
			{
				AddStmtCache(sql, stmt);
				stmt = nullptr; // 避免后续释放缓存语句
			}
		}
		else
		{
			RollbackTransaction();
			if (is_cached)
			{
				sqlite3_finalize(stmt);
				stmt = nullptr;
				stmt_cache_.erase(sql);
			}
		}

		if (!is_cached && stmt != nullptr)
		{
			sqlite3_finalize(stmt);
		}

		return all_ok;
	}

	bool SqliteManager::ExecuteQuery(const std::string& sql, const ParamsList& params, UMapList& result)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_)
		{
			last_error_ = "Database not open";
			return false;
		}

		sqlite3_stmt* stmt = GetStmtCache(sql);
		bool is_cached = (stmt != nullptr);
		bool ret = false;

		if (!is_cached)
		{
			int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
			if (rc != SQLITE_OK)
			{
				last_error_ = sqlite3_errmsg(db_);
				return false;
			}
		}

		if (BindParams(stmt, params))
		{
			ret = ExecutePreparedQuery(stmt, result);
			if (!ret && is_cached)
			{
				sqlite3_finalize(stmt);
				stmt = nullptr;
				stmt_cache_.erase(sql); // 缓存语句执行失败，移除
			}

			if (ret && !is_cached)
			{
				AddStmtCache(sql, stmt);
				stmt = nullptr; // 避免后续释放缓存语句
			}
		}

		if (!is_cached && stmt != nullptr)
		{
			sqlite3_finalize(stmt);
		}

		return ret;
	}

	bool SqliteManager::ExecuteQueryPage(const std::string& sql, const ParamsList& params, int current_page,
	                                     int page_size, UMapList& data, int& total_count, int& total_pages)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_ || current_page < 1 || page_size < 1)
		{
			last_error_ = "Invalid pagination parameters";
			return false;
		}

		data.clear();
		total_count = 0;
		total_pages = 0;

		// 查询总记录数
		std::string count_sql = "SELECT COUNT(*) AS total FROM (" + sql + ") AS t_count;";
		UMapList count_result;
		if (!ExecuteQuery(count_sql, params, count_result))
		{
			return false;
		}

		if (!count_result.empty())
		{
			total_count = std::stoi(count_result[0]["total"]);
		}

		// 计算分页参数
		total_pages = (total_count + page_size - 1) / page_size;
		int offset = (current_page - 1) * page_size;

		// 查询当前页
		std::string page_sql = sql + " LIMIT ? OFFSET ?;";
		ParamsList all_params = params;
		all_params.emplace_back(page_size);
		all_params.emplace_back(offset);

		return ExecuteQuery(page_sql, all_params, data);
	}

	bool SqliteManager::ReadBlobChunk(const std::string& table_name, const std::string& col_name, int64_t row_id,
	                                  size_t offset, size_t chunk_size, std::vector<char>& chunk_data)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_)
		{
			last_error_ = "Database not open";
			return false;
		}

		chunk_data.clear();
		sqlite3_blob* p_blob = nullptr;

		int rc = sqlite3_blob_open(db_, "main", table_name.c_str(), col_name.c_str(), row_id, 0, &p_blob);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			return false;
		}

		int blob_total_len = sqlite3_blob_bytes(p_blob);
		if (offset >= static_cast<size_t>(blob_total_len))
		{
			last_error_ = "Offset exceeds blob length";
			sqlite3_blob_close(p_blob);
			p_blob = nullptr;
			return true; // 偏移超限，返回空数据
		}

		// 计算实际读取长度
		size_t read_len = min(chunk_size, static_cast<size_t>(blob_total_len) - offset);
		chunk_data.resize(read_len);

		// 分块读取
		rc = sqlite3_blob_read(p_blob, chunk_data.data(), static_cast<int>(read_len), static_cast<int>(offset));
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_blob_close(p_blob);
			p_blob = nullptr;
			return false;
		}

		sqlite3_blob_close(p_blob);
		p_blob = nullptr;
		return true;
	}

	bool SqliteManager::WriteBlobChunk(const std::string& table_name, const std::string& col_name, int64_t row_id,
	                                   size_t offset, const std::vector<char>& chunk_data)
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		last_error_.clear();
		if (!db_ || chunk_data.empty())
		{
			last_error_ = "DB not open or empty chunk data";
			return false;
		}

		sqlite3_blob* p_blob = nullptr;

		int rc = sqlite3_blob_open(db_, "main", table_name.c_str(), col_name.c_str(), row_id, 1, &p_blob);
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			return false;
		}

		// 检查偏移
		int blob_total_len = sqlite3_blob_bytes(p_blob);
		if (offset > static_cast<size_t>(blob_total_len))
		{
			last_error_ = "Offset exceeds BLOB length (write)";
			sqlite3_blob_close(p_blob);
			p_blob = nullptr;
			return false;
		}

		// 分块写入
		rc = sqlite3_blob_write(p_blob, chunk_data.data(), static_cast<int>(chunk_data.size()),
		                        static_cast<int>(offset));
		if (rc != SQLITE_OK)
		{
			last_error_ = sqlite3_errmsg(db_);
			sqlite3_blob_close(p_blob);
			p_blob = nullptr;
			return false;
		}

		sqlite3_blob_close(p_blob);
		p_blob = nullptr;
		return true;
	}

	bool SqliteManager::BeginTransaction()
	{
		return ExecuteNonQuery("BEGIN TRANSACTION;");
	}

	bool SqliteManager::CommitTransaction()
	{
		return ExecuteNonQuery("COMMIT;");
	}

	bool SqliteManager::RollbackTransaction()
	{
		return ExecuteNonQuery("ROLLBACK;");
	}

	int64_t SqliteManager::GetLastInsertId() const noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return db_ ? sqlite3_last_insert_rowid(db_) : 0;
	}

	std::string SqliteManager::GetLastErrorMsg() const noexcept
	{
		std::lock_guard<std::recursive_mutex> lock(mutex_);
		return last_error_;
	}

#pragma endregion

#pragma region 线程池类（ThreadPool）
	ThreadPool::ThreadPool(size_t threads)
		: stop_(false)
	{
		for (size_t i = 0; i < threads; ++i)
		{
			workers_.emplace_back([this]
			{
				for (;;)
				{
					std::function<void()> task;

					{
						std::unique_lock<std::mutex> lock(this->queue_mutex_);
						this->condition_.wait(lock, [this] { return this->stop_ || !this->tasks_.empty(); });

						if (this->stop_ && this->tasks_.empty())
							return;

						task = std::move(this->tasks_.front());
						this->tasks_.pop();
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

	ThreadPool::~ThreadPool()
	{
		{
			std::unique_lock<std::mutex> lock(queue_mutex_);
			stop_ = true;
		}
		condition_.notify_all();
		for (std::thread& worker : workers_)
			worker.join();
	}

	size_t ThreadPool::pending_tasks()
	{
		std::unique_lock<std::mutex> lock(queue_mutex_);
		return tasks_.size();
	}

#pragma endregion

#pragma region 自定义配置类（CfgCustom）
	class ConfigImpl
	{
	public:
		Json::Value json_root_; // JSON数据根节点
		std::string base_path_; // 配置文件根目录
		std::string last_error_; // 最后错误信息
		mutable std::mutex data_mtx_; // 临界区对象
		mutable std::mutex load_file_mtx_; // 临界区对象（仅读文件时用）
		mutable std::mutex save_file_mtx_; // 临界区对象（仅写文件时用）

		ConfigImpl()
			: base_path_("d:/param/custom_settings/")
		{
			last_error_ = "";

			file_system::CreateDirectorys(base_path_);

			LoadJsonFiles();
		}

		~ConfigImpl()
		{
			SaveJsonFiles();
		}

		static std::string EnsureTrailingSlash(const std::string& path)
		{
			if (!path.empty() && (path[path.size() - 1] != '\\' && path[path.size() - 1] != '/'))
			{
				return path + "/";
			}
			return path;
		}

		static ConfigDataType JudgeStringType(const std::string& str)
		{
			std::regex int_pattern("^[+-]?\\d+$");
			std::regex float_pattern("^[+-]?(\\d+\\.?\\d*|\\.?\\d+)$");

			if (std::regex_match(str, int_pattern))
				return ConfigDataType::Int;
			if (std::regex_match(str, float_pattern))
				return ConfigDataType::Double;
			if (!str.empty())
				return ConfigDataType::String;
			return ConfigDataType::Null;
		}

		bool EnsureNodeExists(const std::string& file_name, const std::string& section, const std::string& key)
		{
			if (string_utils::Trim(file_name).empty() || string_utils::Trim(section).empty() || string_utils::Trim(key).
				empty())
				return false; // 不能创建空白名称的节点
			if (!json_root_.isMember(file_name))
				json_root_[file_name] = Json::Value(Json::objectValue);
			if (!json_root_[file_name].isMember(section))
				json_root_[file_name][section] = Json::Value(Json::objectValue);
			if (!json_root_[file_name][section].isMember(key))
				json_root_[file_name][section][key] = Json::Value(Json::objectValue);
			if (!json_root_[file_name][section][key].isMember("value"))
				json_root_[file_name][section][key]["value"] = Json::Value(Json::nullValue);
			if (!json_root_[file_name][section][key].isMember("description"))
				json_root_[file_name][section][key]["description"] = "";
			return true;
		}

		bool LoadJsonFile(const std::string& file_name)
		{
			std::lock_guard<std::mutex> lock(load_file_mtx_);

			last_error_ = "";
			std::string file_path = base_path_ + file_name;
			std::ifstream ifs(file_path, std::ios::in | std::ios::binary);

			if (!ifs.is_open())
			{
				last_error_ = "打开文件失败: " + file_path;
				return false;
			}

			Json::Reader reader;
			Json::Value jsonData;
			if (!reader.parse(ifs, jsonData))
			{
				last_error_ = "JSON解析失败: " + reader.getFormattedErrorMessages();
				ifs.close();
				return false;
			}

			json_root_[file_name] = jsonData;
			ifs.close();
			last_error_ = "加载文件成功: " + file_name;
			return true;
		}

		bool SaveJsonFile(const std::string& file_name, const bool& is_new_file = false)
		{
			std::lock_guard<std::mutex> lock(save_file_mtx_);

			last_error_ = "";
			std::string file_path = base_path_ + file_name;

			if (is_new_file)
			{
				DWORD attrib = GetFileAttributesA(file_path.c_str());
				bool exist = (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY));
				if (!exist && !json_root_.isMember(file_name))
				{
					std::ofstream ofs(file_path, std::ios::out | std::ios::binary);
					if (!ofs.is_open())
					{
						last_error_ = "创建新文件失败: " + file_path;
						return false;
					}

					Json::Value newObj(Json::objectValue);
					Json::StreamWriterBuilder writer_builder;
					writer_builder["enable_escaping_for_non_ascii"] = false; //核心：关闭中文转义
					writer_builder["emitUTF8"] = true;

					//Json::StyledWriter writer;
					//ofs << writer.write(newObj);
					//ofs.close();
					std::string strJson = Json::writeString(writer_builder, newObj);
					ofs << strJson;
					ofs.close();

					json_root_[file_name] = newObj;

					last_error_ = "创建新文件成功: " + file_name;
				}
				else
				{
					last_error_ = "文件已存在: " + file_name;
					return false;
				}
			}
			else
			{
				if (json_root_.isMember(file_name))
				{
					std::ofstream ofs(file_path);
					if (!ofs.is_open())
					{
						last_error_ = "保存文件失败: " + file_path;
						return false;
					}

					Json::StreamWriterBuilder writer_builder;
					writer_builder["enable_escaping_for_non_ascii"] = false; //核心：关闭中文转义
					writer_builder["emitUTF8"] = true;

					//Json::StyledWriter writer;
					//ofs << writer.write(json_root_[file_name]);
					//ofs.close();
					std::string strJson = Json::writeString(writer_builder, json_root_[file_name]);
					ofs << strJson;
					ofs.close();

					last_error_ = "保存文件成功: " + file_name;
				}
				else
				{
					last_error_ = "文件不存在: " + file_name;
					return false;
				}
			}

			return true;
		}

		bool DeleteJsonFile(const std::string& file_name)
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			std::string file_path = base_path_ + file_name;
			if (std::remove(file_path.c_str()) != 0)
			{
				last_error_ = "删除文件失败: " + file_path;
				return false;
			}
			json_root_.removeMember(file_name);
			last_error_ = "删除文件成功: " + file_name;
			return true;
		}

		bool RenameJsonFile(const std::string& old_file_name, const std::string& new_file_name)
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			std::string old_path = base_path_ + old_file_name;
			std::string new_path = base_path_ + new_file_name;

			if (std::rename(old_path.c_str(), new_path.c_str()) != 0)
			{
				last_error_ = "重命名文件失败: " + old_file_name + " -> " + new_file_name;
				return false;
			}

			// 更新内存中的键名
			if (json_root_.isMember(old_file_name))
			{
				json_root_[new_file_name] = json_root_[old_file_name];
				json_root_.removeMember(old_file_name);
			}
			last_error_ = "重命名文件成功: " + old_file_name + " -> " + new_file_name;
			return true;
		}

		bool LoadJsonFiles()
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			WIN32_FIND_DATAA find_data;
			std::string search_path = base_path_ + "*.json";
			HANDLE handle = FindFirstFileA(search_path.c_str(), &find_data);

			if (handle == INVALID_HANDLE_VALUE)
			{
				last_error_ = "未找到JSON文件: " + search_path;
				return false;
			}

			do
			{
				if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
				{
					std::string file_name = find_data.cFileName;
					LoadJsonFile(file_name);
				}
			}
			while (FindNextFileA(handle, &find_data) != 0);

			if (GetLastError() != ERROR_NO_MORE_FILES)
			{
				last_error_ = "遍历文件失败";
			}
			else
			{
				last_error_ = "加载所有JSON文件完成";
			}
			FindClose(handle);
			return true;
		}

		bool SaveJsonFiles()
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			int fail_count = 0;
			Json::Value::Members members = json_root_.getMemberNames();
			for (size_t i = 0; i < members.size(); ++i)
			{
				std::string member = members[i];
				size_t pos = member.rfind('.');
				if (pos == std::string::npos)
				{
					continue;
				}

				std::string suffix = member.substr(pos);
				for (size_t i = 0; i < suffix.size(); i++)
				{
					suffix[i] = tolower(suffix[i]);
				}
				if (suffix.compare(".json") != 0)
				{
					continue;
				}

				if (!SaveJsonFile(member))
				{
					fail_count++;
				}
			}

			if (fail_count == 0)
			{
				last_error_ = "所有文件保存成功";
				return true;
			}
			last_error_ = "所有文件保存失败";
			return false;
		}

		bool JsonToVector(const std::string& file_name, std::vector<ConfigCustom::Members>& current_file_data)
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			current_file_data.clear();

			if (file_name.empty())
			{
				last_error_ = "文件名为空";
				return false;
			}

			if (!json_root_.isObject())
			{
				last_error_ = "JSON根节点不是对象";
				return false;
			}

			const Json::Value& file_data = json_root_[file_name];
			if (!file_data.isObject())
			{
				last_error_ = "文件数据不是对象: " + file_name;
				return false;
			}

			Json::Value::Members sections = file_data.getMemberNames();
			for (size_t i = 0; i < sections.size(); ++i)
			{
				std::string section = sections[i];
				const Json::Value& sec_obj = file_data[section];
				if (!sec_obj.isObject())
					continue;

				Json::Value::Members keys = sec_obj.getMemberNames();
				for (size_t j = 0; j < keys.size(); ++j)
				{
					std::string key = keys[j];
					const Json::Value& key_obj = sec_obj[key];
					if (!key_obj.isObject())
						continue;

					ConfigCustom::Members item;
					item.file_name = string_utils::U2G(file_name);
					item.section = string_utils::U2G(section);
					item.key = string_utils::U2G(key);
					item.description = key_obj.isMember("description")
						                   ? string_utils::U2G(key_obj["description"].asString())
						                   : string_utils::U2G("");

					const Json::Value& val = key_obj["value"];

					ConfigDataType type = JudgeStringType(val.asString());

					if (val.isDouble() && ConfigDataType::Double == type) // 浮点数默认保留3位小数
					{
						std::ostringstream ss;
						ss.precision(3);
						ss << std::fixed << val.asDouble();
						item.value = string_utils::U2G(ss.str());
					}
					else
					{
						item.value = string_utils::U2G(val.asString());
					}

					current_file_data.push_back(item);
				}
			}

			last_error_ = "JSON转Vector成功: " + file_name;
			return true;
		}

		bool VectorToJson(const std::string& file_name, const std::vector<ConfigCustom::Members>& current_file_data)
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			json_root_.removeMember(file_name);
			json_root_[file_name] = Json::Value(Json::objectValue);
			for (size_t i = 0; i < current_file_data.size(); ++i)
			{
				const ConfigCustom::Members& member = current_file_data[i];

				ConfigCustom::Members item;
				item.file_name = string_utils::G2U(member.file_name);
				item.section = string_utils::G2U(member.section);
				item.key = string_utils::G2U(member.key);
				item.description = string_utils::G2U(member.description);
				item.value = string_utils::G2U(member.value);

				Json::Value& key_obj = json_root_[item.file_name][item.section][item.key];

				key_obj["description"] = (item.description);

				ConfigDataType type = JudgeStringType(item.value);

				if (ConfigDataType::Int == type)
					key_obj["value"] = atoi(item.value.c_str());
				else if (ConfigDataType::Double == type)
					key_obj["value"] = atof(item.value.c_str());
				else if (ConfigDataType::String == type)
					key_obj["value"] = item.value;
				else
					key_obj["value"] = Json::nullValue;
			}

			last_error_ = "Vector转JSON成功: " + file_name;
			return true;
		}

		bool RemoveJsonObject(const std::string& object_path)
		{
			std::vector<std::string> objects = string_utils::Split(object_path, '/', 3);
			if (objects.size() < 3)
			{
				return false;
			}

			ConfigCustom::Members member;
			member.file_name = objects.at(0);
			member.section = objects.at(1);
			member.key = objects.at(2);

			if (member.key.length() > 0)
			{
				json_root_[member.file_name][member.section].removeMember(member.key);
			}
			else if (member.section.length() > 0)
			{
				json_root_[member.file_name].removeMember(member.section);
			}
			else if (member.file_name.length() > 0)
			{
				json_root_.removeMember(member.file_name);
			}
			return true;
		}

		std::vector<std::string> GetJsonFileList()
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			return json_root_.getMemberNames();
		}

		std::string JsonToString()
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			return json_root_.toStyledString();
		}

		std::string GetLastErrorMsg() const
		{
			std::lock_guard<std::mutex> lock(data_mtx_);
			return last_error_;
		}
	};


	// -------------------------- ConfigKey 实现 --------------------------
	ConfigKey::ConfigKey(ConfigImpl* impl, const std::string& file_name, const std::string& section,
	                     const std::string& key)
		: impl_(impl),
		  file_name_(string_utils::ToLower(file_name)),
		  section_(string_utils::ToLower(section)),
		  key_(string_utils::ToLower(key))
	{
		impl_->EnsureNodeExists(file_name_, section_, key_);
	}

	ConfigKey::ConfigKey(ConfigImpl* impl, const std::string& file_name, const std::string& section)
		: impl_(impl),
		  file_name_(string_utils::ToLower(file_name)),
		  section_(string_utils::ToLower(section)),
		  key_("")
	{
	}

	ConfigKey ConfigKey::operator[](const std::string& key)
	{
		key_ = string_utils::ToLower(key);
		impl_->EnsureNodeExists(file_name_, section_, key_); //不能移除，当key非空时自动创建节点
		return *this;
	}

	ConfigKey& ConfigKey::SetInt(const int& value)
	{
		std::lock_guard<std::mutex> lock(impl_->data_mtx_);
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			impl_->json_root_[file_name_][section_][key_]["value"] = value;
		}
		return *this;
	}

	ConfigKey& ConfigKey::SetDouble(const double& value)
	{
		std::lock_guard<std::mutex> lock(impl_->data_mtx_);
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			impl_->json_root_[file_name_][section_][key_]["value"] = value;
		}
		return *this;
	}

	ConfigKey& ConfigKey::SetString(const std::string& value)
	{
		std::lock_guard<std::mutex> lock(impl_->data_mtx_);
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			impl_->json_root_[file_name_][section_][key_]["value"] = string_utils::G2U(value);
		}
		return *this;
	}

	ConfigKey& ConfigKey::SetDescription(const std::string& value)
	{
		std::lock_guard<std::mutex> lock(impl_->data_mtx_);
		//if (impl_->json_root_.isMember(file_name_)
		//	&& impl_->json_root_[file_name_].isMember(section_)
		//	&& impl_->json_root_[file_name_][section_].isMember(key_)
		//	&& impl_->json_root_[file_name_][section_][key_].isMember("value")
		//	&& impl_->json_root_[file_name_][section_][key_]["value"].type() != Json::nullValue
		//	)
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			impl_->json_root_[file_name_][section_][key_]["description"] = string_utils::G2U(value);
		}
		return *this;
	}

	int ConfigKey::GetInt(const int& default_value, const std::string& description) const
	{
		auto value = default_value;
		{
			std::lock_guard<std::mutex> lock(impl_->data_mtx_);

			if (impl_->EnsureNodeExists(file_name_, section_, key_))
			{
				int temp = impl_->json_root_[file_name_][section_][key_].get("value", Json::Value::maxInt).asInt();
				if (impl_->json_root_[file_name_][section_][key_]["value"].type() != Json::nullValue && temp !=
					Json::Value::maxInt)
					value = temp;
				else
					impl_->json_root_[file_name_][section_][key_]["value"] = default_value;
			}
		}
		GetDescription(description);
		return value;
	}

	double ConfigKey::GetDouble(const double& default_value, const std::string& description) const
	{
		auto value = default_value;
		{
			std::lock_guard<std::mutex> lock(impl_->data_mtx_);

			if (impl_->EnsureNodeExists(file_name_, section_, key_))
			{
				double temp = impl_->json_root_[file_name_][section_][key_].get("value", Json::Value::maxUInt64AsDouble)
				                                                           .asDouble();
				if (impl_->json_root_[file_name_][section_][key_]["value"].type() != Json::nullValue && temp !=
					Json::Value::maxUInt64AsDouble)
					value = temp;
				else
					impl_->json_root_[file_name_][section_][key_]["value"] = default_value;
			}
		}
		GetDescription(description);
		return value;
	}

	std::string ConfigKey::GetString(const std::string& default_value, const std::string& description) const
	{
		auto value = default_value; // ascii
		{
			std::lock_guard<std::mutex> lock(impl_->data_mtx_);

			if (impl_->EnsureNodeExists(file_name_, section_, key_))
			{
				std::string temp = impl_->json_root_[file_name_][section_][key_].get("value", "").asString();
				if (impl_->json_root_[file_name_][section_][key_]["value"].type() != Json::nullValue && !temp.empty())
					value = string_utils::U2G(temp);
				else
					impl_->json_root_[file_name_][section_][key_]["value"] = string_utils::G2U(default_value);
			}
		}
		GetDescription(description);
		return value;
	}

	std::string ConfigKey::GetDescription(const std::string& default_value) const
	{
		auto value = default_value; // ascii
		{
			std::lock_guard<std::mutex> lock(impl_->data_mtx_);

			if (impl_->EnsureNodeExists(file_name_, section_, key_))
			{
				std::string temp = impl_->json_root_[file_name_][section_][key_].get("description", "").asString();
				if (impl_->json_root_[file_name_][section_][key_]["value"].type() != Json::nullValue && !temp.empty())
					value = string_utils::U2G(temp);
				else
					impl_->json_root_[file_name_][section_][key_]["description"] = string_utils::G2U(default_value);
			}
		}
		return value;
	}

	ConfigDataType ConfigKey::GetType() const
	{
		std::lock_guard<std::mutex> lock(impl_->data_mtx_);

		auto type = ConfigDataType::Null;
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			if (impl_->json_root_.isMember(file_name_) && impl_->json_root_[file_name_].isMember(section_) && impl_->
				json_root_[file_name_][section_].isMember(key_) && impl_->json_root_[file_name_][section_][key_].
				isMember("value"))
			{
				Json::ValueType temp = impl_->json_root_[file_name_][section_][key_]["value"].type();
				switch (temp)
				{
				case Json::nullValue:
					type = ConfigDataType::Null;
				case Json::booleanValue:
				case Json::intValue:
				case Json::uintValue:
					type = ConfigDataType::Int;
				case Json::realValue:
					type = ConfigDataType::Double;
				case Json::stringValue:
					type = ConfigDataType::String;
				}
			}
		}
		return type;
	}

	// -------------------------- ConfigSection 实现 --------------------------

	ConfigSection::ConfigSection(ConfigImpl* impl, const std::string& file_name)
		: impl_(impl),
		  file_name_(string_utils::ToLower(file_name))
	{
	}

	ConfigKey ConfigSection::operator[](const std::string& section)
	{
		return ConfigKey(impl_, file_name_, string_utils::ToLower(section));
	}

	// -------------------------- ConfigCustom 实现 --------------------------

	ConfigCustom::ConfigCustom()
		: impl_(new ConfigImpl())
	{
		//TRACE("ConfigCustom::ConfigCustom()\n");
	}

	ConfigCustom::~ConfigCustom()
	{
		//TRACE("ConfigCustom::~ConfigCustom()\n");

		if (impl_)
		{
			delete impl_;
			impl_ = nullptr;
		}
	}

	ConfigCustom& ConfigCustom::GetInstance()
	{
		static ConfigCustom instance;
		return instance;
	}

	ConfigSection ConfigCustom::operator[](const std::string& file_name)
	{
		return ConfigSection(impl_, string_utils::ToLower(file_name));
	}

	std::vector<std::string> ConfigCustom::GetJsonFileList()
	{
		return impl_->GetJsonFileList();
	}

	std::string ConfigCustom::JsonToString() const
	{
		return impl_->JsonToString();
	}

	std::string ConfigCustom::GetLastErrorMsg() const
	{
		return impl_->GetLastErrorMsg();
	}

	bool ConfigCustom::LoadJsonFile(const std::string& file_name)
	{
		return impl_->LoadJsonFile(file_name);
	}

	bool ConfigCustom::SaveJsonFile(const std::string& file_name, const bool& is_new_file)
	{
		return impl_->SaveJsonFile(file_name, is_new_file);
	}

	bool ConfigCustom::DeleteJsonFile(const std::string& file_name)
	{
		return impl_->DeleteJsonFile(file_name);
	}

	bool ConfigCustom::RenameJsonFile(const std::string& old_file_name, const std::string& new_file_name)
	{
		return impl_->RenameJsonFile(old_file_name, new_file_name);
	}

	bool ConfigCustom::LoadJsonFiles()
	{
		return impl_->LoadJsonFiles();
	}

	bool ConfigCustom::SaveJsonFiles()
	{
		return impl_->SaveJsonFiles();
	}

	bool ConfigCustom::JsonToVector(const std::string& file_name, std::vector<Members>& current_file_data)
	{
		return impl_->JsonToVector(file_name, current_file_data);
	}

	bool ConfigCustom::VectorToJson(const std::string& file_name, const std::vector<Members>& current_file_data)
	{
		return impl_->VectorToJson(file_name, current_file_data);
	}

	bool ConfigCustom::RemoveJsonObject(const std::string& object_path)
	{
		return impl_->RemoveJsonObject(object_path);
	}

#pragma endregion

#pragma region 日志管理类（LoggerManager）

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
		Shutdown();
	}

	Logger::Config& Logger::GetConfig()
	{
		return config_;
	}

	void Logger::SetOutputCallback(LogOutput type, const std::function<void(const MetaMsg&)>& callback)
	{
		custom_callbacks_[type] = callback;
	}

	void Logger::Trace(const char* format, ...)
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

		Log(LogLevel::Trace, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Debug(const char* format, ...)
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

		Log(LogLevel::Debug, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Info(const char* format, ...)
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

		Log(LogLevel::Info, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Warn(const char* format, ...)
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

		Log(LogLevel::Warn, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Error(const char* format, ...)
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

		Log(LogLevel::Error, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Critical(const char* format, ...)
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

		Log(LogLevel::Critical, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::LogRecord(LogLevel level, const char* format, ...)
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

		Log(level, nullptr, 0, nullptr, buffer.get());
	}

	void Logger::Trace(const char* file, int line, const char* function, const char* format, ...)
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

		Log(LogLevel::Trace, file, line, function, buffer.get());
	}

	void Logger::Debug(const char* file, int line, const char* function, const char* format, ...)
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

		Log(LogLevel::Debug, file, line, function, buffer.get());
	}

	void Logger::Info(const char* file, int line, const char* function, const char* format, ...)
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

		Log(LogLevel::Info, file, line, function, buffer.get());
	}

	void Logger::Warn(const char* file, int line, const char* function, const char* format, ...)
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

		Log(LogLevel::Warn, file, line, function, buffer.get());
	}

	void Logger::Error(const char* file, int line, const char* function, const char* format, ...)
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

		Log(LogLevel::Error, file, line, function, buffer.get());
	}

	void Logger::Critical(const char* file, int line, const char* function, const char* format, ...)
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

		Log(LogLevel::Critical, file, line, function, buffer.get());
	}

	void Logger::LogRecord(const char* file, const int line, const char* function, LogLevel level, const char* format,
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

		Log(level, file, line, function, buffer.get());
	}

	std::string Logger::FormatLogMessage(LogOutput outputs, LogLevel level, const char* file, int line,
	                                     const char* function, const char* message)
	{
		std::stringstream thread_id;
		thread_id << std::this_thread::get_id();
		size_t tid = 0;
		thread_id >> tid;

		std::string level_name;
		if (level == LogLevel::InfoRed)
			level_name = "Info";
		else if (level == LogLevel::Info)
			level_name = "Info";
		else if (level == LogLevel::InfoGreen)
			level_name = "Info";
		switch (level)
		{
		case LogLevel::Trace:
			level_name = "Trace";
			break;
		case LogLevel::Debug:
			level_name = "Debug";
			break;
		case LogLevel::Info:
			level_name = "Info";
			break;
		case LogLevel::Warn:
			level_name = "Warn";
			break;
		case LogLevel::Error:
			level_name = "Error";
			break;
		case LogLevel::Critical:
			level_name = "Critical";
			break;
		case LogLevel::InfoRed:
		case LogLevel::InfoGreen:
		case LogLevel::InfoBlack: default:
			level_name = "Info";
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
					LogDbManager::GetInstance().AddMsgToQueue(meta_msg);
				}
			}
		}

		std::string result = "[" + oss_time.str() + "] [" + thread_id.str() + "] [" + level_name + "] [" + file_name +
			":" + line_num + ":" + func_name + "] → " + message;

		return result;
	}

	void Logger::Log(LogLevel level, const char* file, int line, const char* function, const char* message)
	{
		if (config_.outputs == LogOutput::None || !HasValidOutput(config_.outputs))
		{
			return;
		}

		auto temp_logger = GetCachedLogger(config_.outputs);
		if (temp_logger)
		{
			std::string fmt_msg = FormatLogMessage(config_.outputs, level, file, line, function, message);

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

	bool Logger::HasValidOutput(LogOutput outputs) const
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

	bool Logger::Initialize()
	{
		try
		{
			spdlog::set_level(static_cast<spdlog::level::level_enum>(config_.level));

			static std::once_flag init_flag;
			std::call_once(init_flag, []() { spdlog::init_thread_pool(8192, 1); }); // 确保线程池只初始化一次（异步日志依赖此线程池）

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

	bool Logger::IsInitialized() const
	{
		return is_initialized_;
	}

	void Logger::Flush()
	{
		if (config_.outputs != LogOutput::None && HasValidOutput(config_.outputs))
		{
			auto logger = GetCachedLogger(config_.outputs);
			if (logger)
			{
				logger->flush();
			}
		}
	}

	void Logger::Shutdown()
	{
		is_initialized_ = false;
		// 注意：不在这里清理spdlog的全局状态，因为可能有其他日志器在使用
	}

	std::shared_ptr<spdlog::logger> Logger::GetCachedLogger(LogOutput outputs)
	{
		std::lock_guard<std::mutex> lock(cache_mutex_);

		// 定期清理过期缓存（每小时一次）
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
		auto logger = CreateTempLogger(outputs);
		if (logger)
		{
			logger_cache_[outputs] = logger;
		}
		return logger;
	}

	std::shared_ptr<spdlog::logger> Logger::CreateTempLogger(LogOutput outputs)
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

				std::string date = string_utils::Format("%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);

				std::string full_path = config_.file_path;
				if (!full_path.empty() && full_path.back() != '/' && full_path.back() != '\\')
				{
					full_path += '/';
				}

				full_path += date; // d:/log/yyyy-mm-dd

				file_system::CreateDirectorys(full_path);

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
		//		auto custom_sink = CreateCustomSink(it->second);
		//		sinks.push_back(custom_sink);
		//	}
		//}
		//
		//if (static_cast<int>(outputs & LogOutput::VsTrace))
		//{
		//	auto it = custom_callbacks_.find(LogOutput::VsTrace);
		//	if (it != custom_callbacks_.end() && it->second)
		//	{
		//		auto custom_sink = CreateCustomSink(it->second);
		//		sinks.push_back(custom_sink);
		//	}
		//}
		//
		//if (static_cast<int>(outputs & LogOutput::XTrace))
		//{
		//	auto it = custom_callbacks_.find(LogOutput::XTrace);
		//	if (it != custom_callbacks_.end() && it->second)
		//	{
		//		auto custom_sink = CreateCustomSink(it->second);
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
		//        spdlog::async_overflow_policy::block  // 缓冲区满时阻塞（避免丢失日志）
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

	std::shared_ptr<Logger::CustomSink> Logger::CreateCustomSink(const std::function<void(const MetaMsg&)>& callback)
	{
		return std::make_shared<CustomSink>(callback);
	}

	LogDbManager& LogDbManager::GetInstance()
	{
		static LogDbManager instance;
		return instance;
	}

	bool LogDbManager::Init()
	{
		if (is_running_)
			return true;
		is_running_ = true;

		std::thread(&LogDbManager::WorkerThread, this).detach();
		return true;
	}

	void LogDbManager::Exit()
	{
		if (!is_running_)
			return;
		is_running_ = false;

		cv_.notify_one();

		std::lock_guard<std::mutex> lock(db_mutex_);
		if (db_)
		{
			db_->Close();
		}
		db_.reset();
	}

	void LogDbManager::AddMsgToQueue(const Logger::MetaMsg& msg)
	{
		if (!is_running_)
		{
			return;
		}
		queue_msg_.emplace(msg);
		cv_.notify_one();
	}

	void LogDbManager::WorkerThread()
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
			std::string date = string_utils::Format("%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);

			if (date != db_last_date_ || !db_ || !db_->IsOpen())
			{
				if (db_)
					db_->Close();

				file_system::CreateDirectorys("d:/log/logger");
				db_ = std::make_shared<SqliteManager>("d:/log/logger/" + date + "_log.db");
				// d:/log/logger/2025-10-10_log.db

				if (!db_->IsOpen())
					return false;

				bool rc = db_->ExecuteNonQuery(CREATE_SQL);
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
				params.emplace_back(string_utils::G2U(msg.file));
				params.emplace_back(msg.line);
				params.emplace_back(string_utils::G2U(msg.func));
				params.emplace_back(string_utils::G2U(msg.message));
				while (params.size() < FIELD_COUNT_MAX)
				{
					params.emplace_back("");
				}
				params_list.push_back(std::move(params));
			}
			bool rc = db_->ExecuteBatchNonQuery(INSERT_SQL, params_list);

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

			auto ok = LamdbaFunc(batch);

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

		if (remain.empty() || !db_ || !db_->IsOpen())
			return;

		auto ok = LamdbaFunc(remain);
	}

	LoggerManager& LoggerManager::GetInstance()
	{
		static LoggerManager instance;
		return instance;
	}

	bool LoggerManager::createLogger(const LogName& name)
	{
		std::lock_guard<std::mutex> lock(logger_mutex_);

		if (loggers_.find(name) != loggers_.end())
		{
			return true;
		}

		auto logger = std::make_shared<Logger>(name);
		logger->GetConfig() = Logger::Config(name);

		if (!logger->IsInitialized())
			logger->Initialize();

		loggers_[name] = logger;

		return logger ? true : false;
	}

	bool LoggerManager::createLogger(const Logger::Config& config)
	{
		std::lock_guard<std::mutex> lock(logger_mutex_);

		if (loggers_.find(config.log_name) != loggers_.end())
		{
			return true;
		}

		auto logger = std::make_shared<Logger>(config);

		if (!logger->IsInitialized())
			logger->Initialize(); // 内部已持有 config

		loggers_[config.log_name] = logger;

		return logger ? true : false;
	}

	std::shared_ptr<Logger> LoggerManager::GetLogger(const LogName& name)
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

	std::vector<LogName> LoggerManager::LoggerNames() const
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

	std::string LoggerManager::LogNameToStr(const LogName& name)
	{
		switch (name)
		{
		case LogName::MOUNT:
			return "mount";
		case LogName::MOTION:
			return "motion";
		case LogName::SMT_DATA:
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
		LogDbManager::GetInstance().Init();

		//【1】创建日志器（当模块日志器不存在时返回默认日志器）
		for (int log_name_index = 0; log_name_index < static_cast<int>(LogName::LOGNAME_MAX); ++log_name_index)
		{
			auto log_name = static_cast<LogName>(log_name_index);
			Logger::Config config(log_name);
			config.file_path = "d:/log";
			config.file_name = LogNameToStr(log_name) + ".ini";
			config.level = LogLevel::Debug;
			createLogger(config);
		}

		//【2】启动定期清理线程
		stop_cleanup_thread_ = false;
		cleanup_thread_ = std::thread(&LoggerManager::CleanupThread, this);
	}

	LoggerManager::~LoggerManager()
	{
		stop_cleanup_thread_ = true;
		if (cleanup_thread_.joinable())
		{
			cleanup_thread_.join();
		}

		LogDbManager::GetInstance().Exit();
	}

	void LoggerManager::AddCleanupDirectory(const std::string& path, int days)
	{
		std::lock_guard<std::mutex> lock(cleanup_mutex_);
		if (days <= 0)
			cleanup_map_.erase(path);
		else
			cleanup_map_[path] = days;
	}

	bool GetFileCreateTime(const std::string& file_path, time_t& create_time)
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

	void DeleteExpiredLogFiles(const std::string& dir_path, int days)
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
				DeleteExpiredLogFiles(full_path, days);

				if (PathIsDirectoryEmptyA(full_path.c_str()))
				{
					RemoveDirectoryA(full_path.c_str());
				}
			}
			else
			{
				time_t create_time = 0;
				if (!GetFileCreateTime(full_path, create_time))
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

	void LoggerManager::CleanupThread()
	{
		while (!stop_cleanup_thread_)
		{
			std::string msg("Start cleanup thread...\n");

			// 拷贝配置（避免持有锁期间耗时）
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

				DeleteExpiredLogFiles(path, days);
			}

			constexpr int cleanup_interval = 3600; // 等待指定间隔（秒），避免频繁刷新IO
			for (int i = 0; i < cleanup_interval && !stop_cleanup_thread_; ++i)
			{
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		}
	}
#pragma endregion

#pragma region 键值对配置类（KeyValueMap）
	struct COMMONTOOLS_API KeyValueMap::Impl
	{
		Json::Value root;
	};


	KeyValueMap::KeyValueMap()
		: impl_(std::make_unique<Impl>())
	{
	}

	KeyValueMap::KeyValueMap(const KeyValueMap& other)
		: impl_(std::make_unique<Impl>())
	{
		std::lock(mutex_, other.mutex_);
		std::lock_guard<std::mutex> this_lock(mutex_, std::adopt_lock);
		std::lock_guard<std::mutex> other_lock(other.mutex_, std::adopt_lock);
		impl_->root = other.impl_->root;
	}

	KeyValueMap& KeyValueMap::operator=(const KeyValueMap& other)
	{
		if (this == &other)
			return *this;

		std::lock(mutex_, other.mutex_);
		std::lock_guard<std::mutex> this_lock(mutex_, std::adopt_lock);
		std::lock_guard<std::mutex> other_lock(other.mutex_, std::adopt_lock);
		impl_->root = other.impl_->root;
		return *this;
	}

	KeyValueMap::~KeyValueMap()
	{
	}

	void KeyValueMap::set(const std::string& key, const bool& value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		impl_->root[key] = value;
	}

	void KeyValueMap::set(const std::string& key, const int& value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		impl_->root[key] = value;
	}

	void KeyValueMap::set(const std::string& key, const float& value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		impl_->root[key] = value;
	}

	void KeyValueMap::set(const std::string& key, const double& value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		impl_->root[key] = value;
	}

	void KeyValueMap::set(const std::string& key, const std::string& value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		impl_->root[key] = value;
	}

	bool KeyValueMap::get(const std::string& key, bool default_value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return impl_->root.isMember(key) ? impl_->root[key].asBool() : default_value;
	}

	int KeyValueMap::get(const std::string& key, int default_value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return impl_->root.isMember(key) ? impl_->root[key].asInt() : default_value;
	}

	float KeyValueMap::get(const std::string& key, float default_value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return impl_->root.isMember(key) ? impl_->root[key].asFloat() : default_value;
	}

	double KeyValueMap::get(const std::string& key, double default_value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return impl_->root.isMember(key) ? impl_->root[key].asDouble() : default_value;
	}

	std::string KeyValueMap::get(const std::string& key, const std::string& default_value)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		return impl_->root.isMember(key) ? impl_->root[key].asString() : default_value;
	}

	std::string KeyValueMap::to_string(bool style) const
	{
		std::lock_guard<std::mutex> lock(mutex_);
		Json::StreamWriterBuilder wb;
		wb["indentation"] = style ? "  " : "";
		return Json::writeString(wb, impl_->root);
	}

	void KeyValueMap::from_string(const std::string& str)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		Json::CharReaderBuilder rb;
		std::string error;
		std::istringstream iss(str);
		Json::parseFromStream(rb, iss, &impl_->root, &error);
	}
#pragma endregion
}

#pragma region 基础数据转换位数据
namespace bit32_tools
{
	void Set(int& value, int bit_idx, bool bit_value)
	{
		if (bit_idx < 0 || bit_idx >= 32)
			return;

		bit_value ? (value |= (1U << bit_idx)) : (value &= ~(1U << bit_idx));
		// 	if (bit_value)
		// 		value |= (1 << bit_idx); // 置1:用位或操作
		// 	else
		// 		value &= ~(1 << bit_idx); // 置0:用位或操作，再取反
	}

	bool Get(int value, int bit_idx)
	{
		if (bit_idx < 0 || bit_idx >= 32)
			return false;

		return (value >> bit_idx) & 1; // 提取指定位：先右移，再与1做与运算
	}
}
#pragma endregion

#pragma region 字符串工具
namespace string_utils
{
	std::vector<std::string> Split(const std::string& str, char delimiter, int pad_number)
	{
		std::stringstream ss(str);
		std::vector<std::string> tokens;
		std::string token;
		while (std::getline(ss, token, delimiter))
		{
			tokens.push_back(std::move(token));
		}
		if (pad_number > 0)
		{
			while (tokens.size() < pad_number)
			{
				tokens.push_back("");
			}
		}
		return tokens;
	}

	std::vector<std::string> Split(const std::string& str, std::string delimiters, int pad_number)
	{
		std::vector<std::string> tokens;
		if (!str.empty() && !delimiters.empty()) // 解决str为空时无法按pad_number填充
		{
			size_t start = 0;
			size_t end = str.find_first_of(delimiters);

			while (end != std::string::npos)
			{
				if (end != start)
				{
					tokens.push_back(str.substr(start, end - start));
				}
				start = end + 1;
				end = str.find_first_of(delimiters, start);
			}

			if (start < str.length())
			{
				tokens.push_back(str.substr(start));
			}
		}

		if (pad_number > 0)
		{
			while (tokens.size() < pad_number)
			{
				tokens.push_back("");
			}
		}
		return tokens;
	}

	std::string Merge(const std::vector<std::string>& list, char delimiter)
	{
		return Merge(list, std::string(1, delimiter));
	}

	std::string Merge(const std::vector<std::string>& list, std::string delimiters)
	{
		std::string merge;
		for (int i = 0; i < list.size(); ++i)
		{
			merge.append(list.at(i));
			if (i < list.size() - 1)
				merge.append(delimiters);
		}
		return merge;
	}

	std::string Format(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		size_t size = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)) + 1;
		va_end(args);

		std::string result;
		result.resize(size);
		va_start(args, format);
		vsnprintf(&result[0], size, format, args);
		va_end(args);
		if (size > 0)
			result.resize(size - 1); // 去除末尾的'\0'
		return result; // 直接返回string,避免野指针  
	}

	void Format(std::string& out, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		size_t size = static_cast<size_t>(vsnprintf(nullptr, 0, format, args)) + 1;
		va_end(args);

		out.clear();
		out.resize(size);
		va_start(args, format);
		vsnprintf(&out[0], size, format, args);
		va_end(args);
		if (size > 0)
			out.resize(size - 1); // 去除末尾的'\0'
	}

	std::string G2U(const std::string& gbk)
	{
#ifdef _WIN32
		std::string gbk_str(gbk);
		int utf8_len = MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, nullptr, 0);
		if (utf8_len <= 0)
			throw std::runtime_error("GBK to WideChar failed");

		std::wstring wstr(utf8_len, 0);
		MultiByteToWideChar(CP_ACP, 0, gbk_str.c_str(), -1, &wstr[0], utf8_len);

		int gbk_len = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (gbk_len <= 0)
			throw std::runtime_error("WideChar to UTF8 failed");

		std::string utf8_str(gbk_len, 0);
		WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &utf8_str[0], gbk_len, nullptr, nullptr);
		utf8_str.pop_back(); // 移除末尾的\0
		return utf8_str;
#else
		throw std::runtime_error("Windows API unavailable on this platform");
#endif
	}

	std::string U2G(const std::string& utf8)
	{
#ifdef _WIN32
		std::string utf8_str(utf8);
		int utf8_len = MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, nullptr, 0);
		if (utf8_len <= 0)
			throw std::runtime_error("UTF8 to WideChar failed");

		std::wstring wstr(utf8_len, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8_str.c_str(), -1, &wstr[0], utf8_len);

		int gbk_len = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
		if (gbk_len <= 0)
			throw std::runtime_error("WideChar to GBK failed");

		std::string gbk_str(gbk_len, 0);
		WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &gbk_str[0], gbk_len, nullptr, nullptr);
		gbk_str.pop_back();
		return gbk_str;
#else
		throw std::runtime_error("Windows API unavailable on this platform");
#endif
	}

	std::string TrimLeft(const std::string& str)
	{
		size_t start = str.find_first_not_of(" \t\n\r\v\f");
		return (start == std::string::npos) ? str : str.substr(start);
	}

	std::string TrimRight(const std::string& str)
	{
		size_t end = str.find_last_not_of(" \t\n\r\v\f");
		return (end == std::string::npos) ? str : str.substr(0, end + 1);
	}

	std::string Trim(const std::string& str)
	{
		return TrimLeft(TrimRight(str));
	}

	std::string Trim(const std::string& str, const std::string& chars)
	{
		size_t start = str.find_first_not_of(chars);
		std::string temp = (start == std::string::npos) ? str : str.substr(start);
		size_t end = temp.find_last_not_of(chars);
		return (end == std::string::npos) ? temp : temp.substr(0, end + 1);
	}

	std::string ToUpper(const std::string& str)
	{
		std::string res = str;
		for (char& c : res)
		{
			c = static_cast<char>(toupper(static_cast<unsigned char>(c))); // c = toupper(c);
		}
		return res;
	}

	std::string ToLower(const std::string& str)
	{
		std::string res = str;
		for (char& c : res)
		{
			c = static_cast<char>(tolower(static_cast<unsigned char>(c))); // c = tolower(c);
		}
		return res;
	}

	std::string Repalce(const std::string& str, const std::string& old_str, const std::string& new_str)
	{
		std::string tmp(str);
		std::string::size_type pos = 0;
		while ((pos = tmp.find(old_str)) != std::string::npos)
		{
			tmp.replace(pos, old_str.length(), new_str);
			pos += new_str.length();
		}
		return tmp;
	}
}
#pragma endregion

#pragma region 文件目录操作
namespace file_system
{
	bool Exists(const std::string& path)
	{
		return _access(path.c_str(), 0) == 0;
	}

	bool IsFile(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
		{
			return false;
		}
		return (info.st_mode & S_IFMT) == S_IFREG;
	}

	bool CreateFileX(const std::string& path)
	{
		std::ofstream file(path);
		if (!file)
		{
			return false;
		}
		file.close();
		return true;
	}

	bool RenameFile(const std::string& src_path, const std::string& dst_path)
	{
		std::ifstream file(src_path);
		if (!file)
		{
			return false;
		}
		file.close();

		if (std::rename(src_path.c_str(), dst_path.c_str()) != 0)
		{
			if (::MoveFileEx(src_path.c_str(), dst_path.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
			{
				return false;
			}
		}
		return true;
	}

	bool CopyFileX(const std::string& src_path, const std::string& dst_path)
	{
		if (!IsFile(src_path))
		{
			return false;
		}

		std::ifstream in(src_path, std::ios::binary);
		if (!in.is_open())
		{
			return false;
		}

		std::ofstream out(dst_path, std::ios::binary);
		if (!out.is_open())
		{
			in.close();
			return false;
		}

		out << in.rdbuf();

		in.close();
		out.close();
		return true;
	}

	bool MoveFileX(const std::string& src_path, const std::string& dst_path)
	{
		if (!IsFile(src_path))
		{
			return false;
		}

		if (rename(src_path.c_str(), dst_path.c_str()) != 0)
		{
			if (::MoveFileEx(src_path.c_str(), dst_path.c_str(), MOVEFILE_REPLACE_EXISTING) == 0)
			{
				return false;
			}
		}
		return true;
	}

	bool DeleteFileX(const std::string& path)
	{
		if (!IsFile(path))
		{
			return false;
		}
		return ::remove(path.c_str()) == 0;
	}

	std::time_t GetFileCreateTime(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
		{
			return 0;
		}
		return info.st_ctime;
	}

	std::time_t GetFileModifiedTime(const std::string& path)
	{
		struct stat info;
		if (stat(path.c_str(), &info) != 0)
		{
			return 0;
		}
		return info.st_mtime;
	}

	std::string GetFileExtensionName(const std::string& path)
	{
		size_t pos = path.find_last_of('.');
		if (pos == std::string::npos)
		{
			return "";
		}
		return path.substr(pos + 1);
	}

	std::string GetFileName(const std::string& path)
	{
		size_t pos = path.find_last_of("\\/");
		if (pos == std::string::npos)
		{
			return path;
		}
		return path.substr(pos + 1);
	}

	std::string GetFilePath(const std::string& path)
	{
		size_t pos = path.find_last_of("\\/");
		if (pos == std::string::npos)
		{
			return "";
		}
		return path.substr(0, pos);
	}

	size_t GetFileSize(const std::string& path)
	{
		if (!IsFile(path))
		{
			return 0;
		}

		std::ifstream in(path, std::ios::binary | std::ios::ate);
		if (!in.is_open())
		{
			return 0;
		}

		std::streampos size = in.tellg();
		in.close();
		return size;
	}

	std::vector<std::string> GetFiles(const std::string& path, const std::string& extension)
	{
		std::vector<std::string> files;
		if (!IsDirectory(path))
			return files;

		WIN32_FIND_DATA find_data;
		HANDLE handle = ::FindFirstFile((path + "\\*").c_str(), &find_data);
		if (handle == INVALID_HANDLE_VALUE)
			return files;

		do
		{
			if (!(find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				std::string filename(find_data.cFileName);
				if (extension.empty() || filename.rfind(extension) == filename.length() - extension.length())
				{
					files.push_back(path + "\\" + filename);
				}
			}
		}
		while (::FindNextFile(handle, &find_data));
		FindClose(handle);

		return files;
	}

	bool IsDirectory(const std::string& path)
	{
		std::string temp(path);
		while (!temp.empty() && (temp.back() == '\\' || temp.back() == '/'))
		{
			temp.pop_back();
		}
		if (temp.length() == 2 && temp[1] == ':')
		{
			return false;
		}

		struct stat info;
		if (stat(temp.c_str(), &info) != 0)
		{
			return false;
		}
		return (info.st_mode & S_IFMT) == S_IFDIR;
	}

	bool CreateDirectorys(const std::string& path)
	{
		if (Exists(path))
		{
			return IsDirectory(path);
		}

		// 递归创建目录
		size_t pos = 0;
		do
		{
			pos = path.find_first_of("\\/", pos + 1);
			std::string subdir = path.substr(0, pos);
			if (!Exists(subdir) && _mkdir(subdir.c_str()) != 0 && errno != EEXIST)
			{
				return false;
			}
		}
		while (pos != std::string::npos);

		return true;
	}

	bool DeleteDirectorys(const std::string& path)
	{
		if (!IsDirectory(path))
		{
			return false;
		}

		std::string temp(path);
		while (!temp.empty() && (temp.back() == '\\' || temp.back() == '/'))
		{
			temp.pop_back();
		}
		if (temp.length() == 2 && temp[1] == ':')
		{
			return false;
		}

		TCHAR dir[MAX_PATH + 1] = {0};
		strcpy_s(dir, MAX_PATH, temp.c_str());
		strcat_s(dir, MAX_PATH, "\\*");

		WIN32_FIND_DATA find_data;
		HANDLE handle = ::FindFirstFile(dir, &find_data);

		if (handle == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		do
		{
			if (strcmp(find_data.cFileName, ".") == 0 || strcmp(find_data.cFileName, "..") == 0)
			{
				continue;
			}
			TCHAR file_path[MAX_PATH] = {0};
			sprintf_s(file_path, MAX_PATH, "%s\\%s", temp.c_str(), find_data.cFileName);

			if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
			{
				if (!DeleteDirectorys(file_path))
				{
					FindClose(handle);
					return false;
				}
			}
			else
			{
				if (!DeleteFileX(file_path))
				{
					FindClose(handle);
					return false;
				}
			}
		}
		while (::FindNextFile(handle, &find_data) != 0);

		FindClose(handle);

		return ::RemoveDirectory(path.c_str()) != 0;
	}

	std::vector<std::string> GetDirectorys(const std::string& path)
	{
		std::vector<std::string> dirs;
		if (!IsDirectory(path))
		{
			return dirs;
		}

		WIN32_FIND_DATA find_data;
		HANDLE handle = ::FindFirstFile((path + "\\*").c_str(), &find_data);
		if (handle == INVALID_HANDLE_VALUE)
		{
			return dirs;
		}

		do
		{
			if ((find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && strcmp(find_data.cFileName, ".") != 0 &&
				strcmp(find_data.cFileName, "..") != 0)
			{
				dirs.push_back(find_data.cFileName);
			}
		}
		while (::FindNextFile(handle, &find_data) != 0);

		FindClose(handle);

		return dirs;
	}

	std::string GetCurrentWorkDirectory()
	{
		char buffer[MAX_PATH]{};
		::GetCurrentDirectory(MAX_PATH, buffer);
		return buffer;
	}

	bool SetCurrentWorkDirectory(const std::string& path)
	{
		return ::SetCurrentDirectory(path.c_str()) != 0;
	}

	std::string ReadAllText(const std::string& path)
	{
		if (!IsFile(path))
		{
			return "";
		}

		std::ifstream in(path);
		if (!in.is_open())
		{
			return "";
		}

		std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

		in.close();

		return content;
	}

	bool WriteAllText(const std::string& path, const std::string& text)
	{
		std::ofstream out(path);
		if (!out.is_open())
		{
			return false;
		}

		out << text;
		out.close();
		return true;
	}
}
#pragma endregion

#pragma region 文件编码检查
namespace file_encoding
{
	Encoding get(const std::string& file_path)
	{
		std::ifstream file(file_path, std::ios::binary);
		if (!file)
		{
			return Encoding::UNKNOWN;
		}

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();

		size_t len = content.size();
		auto data = (const uint8_t*)content.data();

		// BOM 判断
		if (len >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF)
			return Encoding::UTF8_BOM;
		if (len >= 2 && data[0] == 0xFF && data[1] == 0xFE)
			return Encoding::UTF16_LE;
		if (len >= 2 && data[0] == 0xFE && data[1] == 0xFF)
			return Encoding::UTF16_BE;

		// 判断是否是 UTF-8（能区分中文GBK 和 中文UTF8），如果是纯英文内容则判定为UTF-8
		bool is_utf8 = true;
		size_t i = 0;
		while (i < len)
		{
			// 单字节 0~127：英文，UTF8/GBK 通用
			if (data[i] <= 0x7F)
			{
				i++;
			}
			// 双字节 UTF8
			else if ((data[i] & 0xE0) == 0xC0)
			{
				if (i + 1 >= len || (data[i + 1] & 0xC0) != 0x80)
					is_utf8 = false;
				i += 2;
			}
			// 三字节 UTF8（中文主要在这里）
			else if ((data[i] & 0xF0) == 0xE0)
			{
				if (i + 2 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80)
					is_utf8 = false;
				i += 3;
			}
			// 四字节 UTF8
			else if ((data[i] & 0xF8) == 0xF0)
			{
				if (i + 3 >= len || (data[i + 1] & 0xC0) != 0x80 || (data[i + 2] & 0xC0) != 0x80 || (data[i + 3] & 0xC0)
					!= 0x80)
					is_utf8 = false;
				i += 4;
			}
			// 不符合 UTF8 规则 → 一定是 GBK
			else
			{
				is_utf8 = false;
				i++;
			}
		}

		if (is_utf8)
		{
			return Encoding::UTF8;
		}
		return Encoding::GBK;
	}

	void set(const Encoding& encoding)
	{
		// 内容转换
	}
}
#pragma endregion

//#pragma region WEB视图
//namespace web_viewer
//{
//	static std::string read_file_to_utf8(const std::string& file_path)
//	{
//		std::ifstream file(file_path, std::ios::binary);
//		if (!file.is_open())
//		{
//			return "";
//		}
//
//		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>()); // 支持小文件
//		file.close();
//
//		file_encoding::Encoding enc = file_encoding::get(file_path);
//
//		if (enc == file_encoding::Encoding::GBK)
//		{
//			return string_utils::G2U(content);
//		}
//		if (enc == file_encoding::Encoding::UTF8_BOM)
//		{
//			return content.substr(3);
//		}
//		return content;
//	}
//
//	static void show_in_web(const std::string& content, const std::string& web_title)
//	{
//		SYSTEMTIME st;
//		GetLocalTime(&st);
//
//		std::string date_time = string_utils::Format("%04d_%02d_%02d_%02d_%02d_%02d_%03d", st.wYear, st.wMonth, st.wDay,
//		                                             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
//
//		file_system::CreateDirectorys("d:\\log\\html\\");
//		std::string temp_file = std::string("d:\\log\\html\\") + std::string(web_title.size() ? web_title : date_time) +
//			".html";
//		//std::string temp_file = std::string("d:\\log\\html\\") + std::string(date_time) + ".html";
//
//		std::ofstream html_file(temp_file, std::ios::binary);
//		if (!html_file.is_open())
//		{
//			return;
//		}
//
//		std::string html_head;
//		html_head += R"(
//			<!DOCTYPE html>
//			<html lang="zh-CN">
//			<head>
//			<meta charset="UTF-8">)";
//		html_head += "<title>";
//		html_head += string_utils::G2U(web_title.size() ? web_title : "FAROAD 帮助");
//		html_head += "</title>";
//		html_head += R"(<style>
//				*{margin:0;padding:0;box-sizing:border-box;font-family:Microsoft YaHei,sans-serif}
//				body{padding:30px;background:#f5f7fa;font-size:16px;line-height:1.7}
//				.box{max-width:1200px;margin:0 auto;background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.05)}
//				h2{margin-bottom:20px;color:#333}
//				pre{white-space:pre-wrap;background:#f8f9fa;padding:15px;border-radius:6px;color:#222}
//				table{border-collapse:collapse;width:100%;margin-top:10px}
//				table,td,th{border:1px solid #ddd;padding:8px}
//				th{background:#f2f2f2}
//			</style>
//			</head>
//			<body>
//			<div class="box">
//			)";
//
//		html_file << html_head;
//		html_file << content;
//		html_file << "\n</div></body></html>";
//		html_file.close();
//
//		ShellExecuteA(nullptr, "open", temp_file.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
//
//		//Sleep(2000);
//		//std::remove(temp_file.c_str());
//	}
//
//	void show_web_by_str(const std::string& str, const std::string& web_title)
//	{
//		std::string content = "<pre>" + string_utils::G2U(str) + "</pre>";
//		show_in_web(content, web_title);
//	}
//
//	void show_web_by_txt(const std::string& file_path, const std::string& web_title)
//	{
//		std::string content = read_file_to_utf8(file_path);
//		std::string html = "<pre>" + content + "</pre>";
//		show_in_web(html, web_title);
//	}
//
//	void show_web_by_csv(const std::string& file_path, const std::string& web_title)
//	{
//		std::string content = read_file_to_utf8(file_path);
//		std::string html = "<table>";
//
//		size_t pos = 0;
//		std::string line;
//		std::string temp = content;
//
//		while ((pos = temp.find('\n')) != std::string::npos)
//		{
//			line = temp.substr(0, pos);
//			temp.erase(0, pos + 1);
//
//			html += "<tr>";
//			size_t p = 0;
//			std::string cell;
//			std::string line_copy = line;
//
//			while ((p = line_copy.find(',')) != std::string::npos)
//			{
//				cell = line_copy.substr(0, p);
//				html += "<td>" + cell + "</td>";
//				line_copy.erase(0, p + 1);
//			}
//
//			if (!line_copy.empty())
//			{
//				html += "<td>" + line_copy + "</td>";
//			}
//
//			html += "</tr>";
//		}
//
//		html += "</table>";
//		show_in_web(html, web_title);
//	}
//
//	void show_web_by_html(const std::string& file_path, const std::string& web_title)
//	{
//		std::string content = read_file_to_utf8(file_path);
//		show_in_web(content, web_title);
//	}
//}
//#pragma endregion

#pragma region 高精度时间戳
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

	int64_t COMMONTOOLS_API get_interval_time_us(int64_t start_us)
	{
		return max(0, get_current_time_us() - start_us);
	}

	int64_t COMMONTOOLS_API get_interval_time_ms(int64_t start_ms)
	{
		return max(0, get_current_time_ms() - start_ms);
	}

	int64_t COMMONTOOLS_API get_interval_time_ss(int64_t start_ss)
	{
		return max(0, get_current_time_ss() - start_ss);
	}
}
#pragma endregion

#pragma region STD标准库
namespace std
{
}
#pragma endregion

//#pragma region 图片管理类
//class ImageManager2
//{
//public:
//	static ImageManager2& GetInstance();
//
//	void AddEnqueue(const cv::Mat& image, const std::string& filename, const std::vector<int>& params = {});
//
//private:
//	ImageManager2();
//
//	~ImageManager2() = default;
//
//	ImageManager2(const ImageManager2&) = delete;
//
//	ImageManager2(const ImageManager2&&) = delete;
//
//	ImageManager2& operator=(const ImageManager2&) = delete;
//
//	ImageManager2& operator=(const ImageManager2&&) = delete;
//
//	static void SaveImageThread(std::shared_ptr<cv::Mat> image_ptr, const std::string& filename, const std::vector<int>& params);
//
//	ThreadPool thread_pool_;
//};
//
//
//class ImageManager
//{
//	ImageManager(int threadCount = -1);
//
//	~ImageManager();
//
//	std::vector<std::thread> workers;
//	std::queue<std::function<void()>> tasks;
//	std::mutex queueMutex;
//	std::condition_variable condition;
//	bool running;
//	std::atomic<int> completedTasks;
//	std::atomic<int> totalTasks;
//
//	int64_t start_time{};
//
//	void enqueue(std::function<void()> task);
//
//public:
//	ImageManager(const ImageManager&) = delete;
//
//	ImageManager& operator=(const ImageManager&) = delete;
//
//	static ImageManager& GetInstance(int threadCount = -1);
//
//	std::vector<int> getTiffCompressParams(int mode = 8);
//
//	void saveImage(const cv::Mat& image, const std::string& filename,
//		const std::vector<int>& compressionParams = {});
//
//	void saveImages(const std::vector<cv::Mat>& images, const std::vector<std::string>& filenames, const std::vector<int>& compressionParams = {});
//
//	int getCompletedTasks() const;
//
//	int getTotalTasks() const;
//};
//
//ImageManager2& ImageManager2::GetInstance()
//{
//	static ImageManager2 instance;
//	return instance;
//}
//
//ImageManager2::ImageManager2()
//	: thread_pool_(4) // 默认开4个
//{
//}
//
//void ImageManager2::SaveImageThread(std::shared_ptr<cv::Mat> image_ptr, const std::string& filename, const std::vector<int>& params)
//{
//	if (!image_ptr)
//		return;
//
//	if (!image_ptr->empty())
//		return;
//
//	bool ok = cv::imwrite(filename, *image_ptr, params);
//	if (ok)
//		LOG_MOUNT(LogLevel::Info, "filename(%s) save ok.", filename.c_str());
//	else
//		LOG_MOUNT(LogLevel::Info, "filename(%s) save ng.", filename.c_str());
//}
//
//void ImageManager2::AddEnqueue(const cv::Mat& image, const std::string& filename, const std::vector<int>& params /*= {} */)
//{
//	if (image.empty())
//		return;
//
//	//if (thread_pool_.pending_tasks() > 1000)
//	//{
//	//	return; // 限制
//	//}
//
//	auto image_ptr = std::make_shared<cv::Mat>(image.clone());
//	try
//	{
//		// 每张图片入队
//		thread_pool_.enqueue(&ImageManager2::SaveImageThread, image_ptr, filename, params);
//		// 不阻塞
//	}
//	catch (const std::exception&)
//	{
//	}
//}
//
//ImageManager::ImageManager(int threadCount)
//{
//	running = true;
//	completedTasks = 0;
//	totalTasks = 0;
//
//	start_time = timestamp::get_current_time_ms();
//
//	// 默认使用CPU核心数
//	if (threadCount <= 0)
//	{
//		threadCount = 2; // std::thread::hardware_concurrency() / 2;//使用一半
//	}
//
//	// 创建工作线程
//	for (int i = 0; i < threadCount; i++)
//	{
//		workers.emplace_back([this]
//			{
//				while (running)
//				{
//					std::function<void()> task;
//					{
//						std::unique_lock<std::mutex> lock(this->queueMutex);
//						this->condition.wait(lock, [this]
//							{
//								return !this->running || !this->tasks.empty();
//							});
//						if (!this->running && this->tasks.empty())
//							return;
//						if (this->tasks.empty())
//							continue;
//						task = std::move(this->tasks.front());
//						this->tasks.pop();
//					}
//					task();
//					++completedTasks;
//
//					if (completedTasks >= totalTasks && totalTasks > 0)
//					{
//						auto end_time = timestamp::get_current_time_ms();
//						LOG_MOUNT(LogLevel::InfoGreen, "非阻塞存图平均耗时: %d ms", (end_time - start_time) / totalTasks);
//					}
//				}
//			});
//	}
//
//	LOG_MOUNT(LogLevel::InfoGreen, "实时存图管理器已初始化，线程数: %d", threadCount);
//}
//
//ImageManager::~ImageManager()
//{
//	running = false;
//	condition.notify_all();
//	for (auto& worker : workers)
//	{
//		worker.join();
//	}
//}
//
//ImageManager& ImageManager::GetInstance(int threadCount)
//{
//	static ImageManager instance(threadCount);
//	return instance;
//}
//
//std::vector<int> ImageManager::getTiffCompressParams(int mode)
//{
//	// 1  = 无压缩
//	// 2  = LZW（通用平衡）
//	// 8  = DEFLATE（最小体积）
//	// 32773 = PackBits（【推荐】黑白相机/灰度图 最快最优）
//	return { cv::IMWRITE_TIFF_COMPRESSION, mode };
//}
//
//void ImageManager::saveImage(const cv::Mat& image, const std::string& filename, const std::vector<int>& compressionParams)
//{
//	// 复制图片数据，避免原始数据被修改
//	cv::Mat imageCopy = image.clone();
//	++totalTasks;
//
//	// 提交保存任务
//	enqueue([imageCopy, filename, compressionParams]()
//		{
//			bool success = cv::imwrite(filename, imageCopy, compressionParams);
//			if (success)
//			{
//				LOG_MOUNT(LogLevel::Info, "已保存:%s: ", filename.c_str());
//			}
//			else
//			{
//				LOG_MOUNT(LogLevel::Info, "保存失败:%s: ", filename.c_str());
//			}
//		});
//}
//
//void ImageManager::saveImages(const std::vector<cv::Mat>& images, const std::vector<std::string>& filenames, const std::vector<int>& compressionParams)
//{
//	for (size_t i = 0; i < images.size() && i < filenames.size(); i++)
//	{
//		saveImage(images[i], filenames[i], compressionParams);
//	}
//}
//
//int ImageManager::getCompletedTasks() const
//{
//	return completedTasks;
//}
//
//int ImageManager::getTotalTasks() const
//{
//	return totalTasks;
//}
//
//void ImageManager::enqueue(std::function<void()> task)
//{
//	{
//		std::unique_lock<std::mutex> lock(queueMutex);
//		tasks.push(std::move(task));
//	}
//	condition.notify_one();
//}
//#pragma endregion

