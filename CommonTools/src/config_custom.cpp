#include "config_custom.h"

#include "string_utils.h"
#include "file_system.h"

#include "json/json.h"

#include <fstream>
#include <mutex>
#include <sstream>
#include <cstdlib>
#include <cctype>
#include <cstdio>
#include <regex>
#include <windows.h>

namespace common_tools
{
	class ConfigImpl
	{
	public:
		Json::Value json_root_; // JSON数据根节点
		std::string base_path_; // 配置文件根目录
		std::string last_error_; // 最后错误信息
		mutable std::mutex data_mtx_; // 临界区对象
		mutable std::mutex load_file_mtx_; // 临界区对象(仅读文件时用)
		mutable std::mutex save_file_mtx_; // 临界区对象(仅写文件时用)

		ConfigImpl()
			: base_path_("d:/param/custom_settings/")
		{
			last_error_ = "";

			file_system::create_directories(base_path_);

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
			if (string_utils::trim(file_name).empty() || string_utils::trim(section).empty() || string_utils::trim(key).
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
					writer_builder["sortKeys"] = false;

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
					writer_builder["sortKeys"] = false;

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
					item.file_name = string_utils::UTF8ToGBK(file_name);
					item.section = string_utils::UTF8ToGBK(section);
					item.key = string_utils::UTF8ToGBK(key);
					item.description = key_obj.isMember("description")
						                   ? string_utils::UTF8ToGBK(key_obj["description"].asString())
						                   : string_utils::UTF8ToGBK("");

					const Json::Value& val = key_obj["value"];

					ConfigDataType type = JudgeStringType(val.asString());

					if (val.isDouble() && ConfigDataType::Double == type) // 浮点数默认保留3位小数
					{
						std::ostringstream ss;
						ss.precision(3);
						ss << std::fixed << val.asDouble();
						item.value = string_utils::UTF8ToGBK(ss.str());
					}
					else
					{
						item.value = string_utils::UTF8ToGBK(val.asString());
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
				item.file_name = string_utils::GBKToUTF8(member.file_name);
				item.section = string_utils::GBKToUTF8(member.section);
				item.key = string_utils::GBKToUTF8(member.key);
				item.description = string_utils::GBKToUTF8(member.description);
				item.value = string_utils::GBKToUTF8(member.value);

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
			std::vector<std::string> objects = string_utils::split(object_path, '/', 3);
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

		std::vector<std::string> GetSections(const std::string& file_name)
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			std::vector<std::string> sections;

			if (file_name.empty())
			{
				last_error_ = "文件名为空";
				return sections;
			}

			if (!json_root_.isMember(file_name))
			{
				last_error_ = "文件不存在: " + file_name;
				return sections;
			}

			const Json::Value& file_data = json_root_[file_name];
			if (!file_data.isObject())
			{
				last_error_ = "文件数据不是对象: " + file_name;
				return sections;
			}

			Json::Value::Members members = file_data.getMemberNames();
			for (const auto& member : members)
			{
				sections.push_back(string_utils::UTF8ToGBK(member));
			}

			last_error_ = "获取节点列表成功: " + file_name;
			return sections;
		}

		std::vector<ConfigCustom::Members> GetSectionKeyValuePairs(const std::string& file_name, const std::string& section)
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			std::vector<ConfigCustom::Members> key_value_pairs;

			if (file_name.empty())
			{
				last_error_ = "文件名为空";
				return key_value_pairs;
			}

			if (section.empty())
			{
				last_error_ = "节点名为空";
				return key_value_pairs;
			}

			if (!json_root_.isMember(file_name))
			{
				last_error_ = "文件不存在: " + file_name;
				return key_value_pairs;
			}

			const Json::Value& file_data = json_root_[file_name];
			if (!file_data.isObject())
			{
				last_error_ = "文件数据不是对象: " + file_name;
				return key_value_pairs;
			}

			if (!file_data.isMember(section))
			{
				last_error_ = "节点不存在: " + section;
				return key_value_pairs;
			}

			const Json::Value& sec_obj = file_data[section];
			if (!sec_obj.isObject())
			{
				last_error_ = "节点数据不是对象: " + section;
				return key_value_pairs;
			}

			Json::Value::Members keys = sec_obj.getMemberNames();
			for (const auto& key : keys)
			{
				const Json::Value& key_obj = sec_obj[key];
				if (!key_obj.isObject())
					continue;

				ConfigCustom::Members item;
				item.file_name = string_utils::UTF8ToGBK(file_name);
				item.section = string_utils::UTF8ToGBK(section);
				item.key = string_utils::UTF8ToGBK(key);
				item.description = key_obj.isMember("description")
					                   ? string_utils::UTF8ToGBK(key_obj["description"].asString())
					                   : string_utils::UTF8ToGBK("");

				const Json::Value& val = key_obj["value"];

				ConfigDataType type = JudgeStringType(val.asString());

				if (val.isDouble() && ConfigDataType::Double == type)
				{
					std::ostringstream ss;
					ss.precision(3);
					ss << std::fixed << val.asDouble();
					item.value = string_utils::UTF8ToGBK(ss.str());
				}
				else
				{
					item.value = string_utils::UTF8ToGBK(val.asString());
				}

				key_value_pairs.push_back(item);
			}

			last_error_ = "获取节点键值对成功: " + file_name + "/" + section;
			return key_value_pairs;
		}

		bool GetAllSectionsKeyValuePairs(const std::string& file_name, std::map<std::string, std::map<std::string, std::string>>& result)
		{
			std::lock_guard<std::mutex> lock(data_mtx_);

			last_error_ = "";
			result.clear();

			if (file_name.empty())
			{
				last_error_ = "文件名为空";
				return false;
			}

			if (!json_root_.isMember(file_name))
			{
				last_error_ = "文件不存在: " + file_name;
				return false;
			}

			const Json::Value& file_data = json_root_[file_name];
			if (!file_data.isObject())
			{
				last_error_ = "文件数据不是对象: " + file_name;
				return false;
			}

			Json::Value::Members sections = file_data.getMemberNames();
			for (const auto& section : sections)
			{
				const Json::Value& sec_obj = file_data[section];
				if (!sec_obj.isObject())
					continue;

				std::map<std::string, std::string> key_value_map;
				Json::Value::Members keys = sec_obj.getMemberNames();
				for (const auto& key : keys)
				{
					const Json::Value& key_obj = sec_obj[key];
					if (!key_obj.isObject())
						continue;

					const Json::Value& val = key_obj["value"];
					ConfigDataType type = JudgeStringType(val.asString());

					std::string value_str;
					if (val.isDouble() && ConfigDataType::Double == type)
					{
						std::ostringstream ss;
						ss.precision(3);
						ss << std::fixed << val.asDouble();
						value_str = string_utils::UTF8ToGBK(ss.str());
					}
					else
					{
						value_str = string_utils::UTF8ToGBK(val.asString());
					}

					key_value_map[string_utils::UTF8ToGBK(key)] = value_str;
				}

				result[string_utils::UTF8ToGBK(section)] = key_value_map;
			}

			last_error_ = "获取所有节点键值对成功: " + file_name;
			return true;
		}
	};

	// -------------------------- ConfigKey 实现 --------------------------
	ConfigKey::ConfigKey(ConfigImpl* impl, const std::string& file_name, const std::string& section,
	                     const std::string& key)
		: impl_(impl),
		  file_name_(string_utils::to_lower(file_name)),
		  section_(string_utils::to_lower(section)),
		  key_(string_utils::to_lower(key))
	{
		impl_->EnsureNodeExists(file_name_, section_, key_);
	}

	ConfigKey::ConfigKey(ConfigImpl* impl, const std::string& file_name, const std::string& section)
		: impl_(impl),
		  file_name_(string_utils::to_lower(file_name)),
		  section_(string_utils::to_lower(section)),
		  key_("")
	{
	}

	ConfigKey ConfigKey::operator[](const std::string& key)
	{
		key_ = string_utils::to_lower(key);
		impl_->EnsureNodeExists(file_name_, section_, key_); //不能移除，当key非空时自动创建节点
		return *this;
	}

	ConfigKey& ConfigKey::SetDescription(const std::string& value)
	{
		//std::lock_guard<std::mutex> lock(impl_->data_mtx_); // by ht 20260528
		//if (impl_->json_root_.isMember(file_name_)
		//	&& impl_->json_root_[file_name_].isMember(section_)
		//	&& impl_->json_root_[file_name_][section_].isMember(key_)
		//	&& impl_->json_root_[file_name_][section_][key_].isMember("value")
		//	&& impl_->json_root_[file_name_][section_][key_]["value"].type() != Json::nullValue
		//	)
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			impl_->json_root_[file_name_][section_][key_]["description"] = string_utils::GBKToUTF8(value);
		}
		return *this;
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
					value = string_utils::UTF8ToGBK(temp);
				else
					impl_->json_root_[file_name_][section_][key_]["description"] = string_utils::GBKToUTF8(default_value);
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
					break;
				case Json::booleanValue:
				case Json::intValue:
				case Json::uintValue:
					type = ConfigDataType::Int;
					break;
				case Json::realValue:
					type = ConfigDataType::Double;
					break;
				case Json::stringValue:
					type = ConfigDataType::String;
					break;
				}
			}
		}
		return type;
	}

	template<>
	COMMONTOOLS_API int ConfigKey::Get<int>(const int& default_value, const std::string& description) const
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
		if (!description.empty())
			const_cast<ConfigKey*>(this)->SetDescription(description);
		return value;
	}

	template<>
	COMMONTOOLS_API double ConfigKey::Get<double>(const double& default_value, const std::string& description) const
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
		if (!description.empty())
			const_cast<ConfigKey*>(this)->SetDescription(description);
		return value;
	}

	template<>
	COMMONTOOLS_API std::string ConfigKey::Get<std::string>(const std::string& default_value, const std::string& description) const
	{
		auto value = default_value;
		{
			std::lock_guard<std::mutex> lock(impl_->data_mtx_);

			if (impl_->EnsureNodeExists(file_name_, section_, key_))
			{
				std::string temp = impl_->json_root_[file_name_][section_][key_].get("value", "").asString();
				if (impl_->json_root_[file_name_][section_][key_]["value"].type() != Json::nullValue && !temp.empty())
					value = string_utils::UTF8ToGBK(temp);
				else
					impl_->json_root_[file_name_][section_][key_]["value"] = string_utils::GBKToUTF8(default_value);
			}
		}
		if (!description.empty())
			const_cast<ConfigKey*>(this)->SetDescription(description);
		return value;
	}

	template<>
	COMMONTOOLS_API ConfigKey& ConfigKey::Set<int>(const int& value, const std::string& description)
	{
		std::lock_guard<std::mutex> lock(impl_->data_mtx_);
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			impl_->json_root_[file_name_][section_][key_]["value"] = value;
		}
		if (!description.empty())
			SetDescription(description);
		return *this;
	}

	template<>
	COMMONTOOLS_API ConfigKey& ConfigKey::Set<double>(const double& value, const std::string& description)
	{
		std::lock_guard<std::mutex> lock(impl_->data_mtx_);
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			impl_->json_root_[file_name_][section_][key_]["value"] = value;
		}
		if (!description.empty())
			SetDescription(description);
		return *this;
	}

	template<>
	COMMONTOOLS_API ConfigKey& ConfigKey::Set<std::string>(const std::string& value, const std::string& description)
	{
		std::lock_guard<std::mutex> lock(impl_->data_mtx_);
		if (impl_->EnsureNodeExists(file_name_, section_, key_))
		{
			impl_->json_root_[file_name_][section_][key_]["value"] = string_utils::GBKToUTF8(value);
		}
		if (!description.empty())
			SetDescription(description);
		return *this;
	}

	// -------------------------- ConfigSection 实现 --------------------------

	ConfigSection::ConfigSection(ConfigImpl* impl, const std::string& file_name)
		: impl_(impl),
		  file_name_(string_utils::to_lower(file_name))
	{
	}

	ConfigKey ConfigSection::operator[](const std::string& section)
	{
		return ConfigKey(impl_, file_name_, string_utils::to_lower(section));
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
		return ConfigSection(impl_, string_utils::to_lower(file_name));
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

	std::vector<std::string> ConfigCustom::GetSections(const std::string& file_name)
	{
		return impl_->GetSections(string_utils::to_lower(file_name));
	}

	std::vector<ConfigCustom::Members> ConfigCustom::GetSectionKeyValuePairs(const std::string& file_name, const std::string& section)
	{
		return impl_->GetSectionKeyValuePairs(string_utils::to_lower(file_name), string_utils::to_lower(section));
	}

	bool ConfigCustom::GetAllSectionsKeyValuePairs(const std::string& file_name, std::map<std::string, std::map<std::string, std::string>>& result)
	{
		return impl_->GetAllSectionsKeyValuePairs(string_utils::to_lower(file_name), result);
	}
}
