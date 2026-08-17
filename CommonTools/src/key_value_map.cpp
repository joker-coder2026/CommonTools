#include "key_value_map.h"

#include "json/json.h"

#include <sstream>
#include <mutex>

namespace common_tools
{
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
}
