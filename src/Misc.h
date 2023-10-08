#pragma once

#define NAMEOF(variable) ((decltype(&variable))nullptr, #variable)

template <class T>
inline T get_value(const nlohmann::json& json, const std::string& path, T value = T(), size_t max_deep = 25)
{
	// discard if empty or not an object,
	if (json.is_null() || json.empty() || !json.is_object()) return value;

	std::stringstream test(path);
	std::string key_name;
	auto current = std::ref(json);
	auto index = 0;

	while (std::getline(test, key_name, '.') && index < max_deep)
	{
		auto it = current.get().find(key_name);
		if (it != current.get().end())
		{
			current = it.value();
			++index;
		}
		else
		{
			return value;
		}
	}

	try
	{
		T returnValue = current.get();
		return returnValue;
	}
	catch (const std::exception& e)
	{
		return value;
	}
	catch (...)
	{
		return value;
	}
}

