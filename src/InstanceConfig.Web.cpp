#include "Updater.h"
#include "InstanceConfig.hpp"


void models::InstanceConfig::SetCommonHeaders(RestClient::Connection* conn) const
{
	//
	// If a backend server is used, it can alter the response based on 
	// these header values, classic web servers will just ignore them
	// 

	conn->AppendHeader("X-Vicius-Manufacturer", manufacturer);
	conn->AppendHeader("X-Vicius-Product", product);
	conn->AppendHeader("X-Vicius-Version", appVersion.to_string());
}

int models::InstanceConfig::DownloadRelease(curl_progress_callback progressFn, const int releaseIndex)
{
	const auto conn = new RestClient::Connection("");

	conn->SetTimeout(5);
	conn->SetUserAgent(std::format("{}/{}", appFilename, appVersion.to_string()));
	conn->FollowRedirects(true);
	conn->FollowRedirects(true, 5);
	conn->SetFileProgressCallback(progressFn);

	SetCommonHeaders(conn);

	std::string tempPath(MAX_PATH, '\0');
	std::string tempFile(MAX_PATH, '\0');

	// TODO: error handling

	GetTempPathA(MAX_PATH, tempPath.data());
	GetTempFileNameA(tempPath.c_str(), "VICIUS", 0, tempFile.data());

	auto& release = remote.releases[releaseIndex];
	release.localTempFilePath = tempFile;

	static std::ofstream outStream(release.localTempFilePath.string(), std::ios::binary);

	// write to file as we download it
	auto writeCallback = [](void* data, size_t size, size_t nmemb, void* userdata) -> size_t
	{
		UNREFERENCED_PARAMETER(userdata);

		const auto bytes = size * nmemb;

		// TODO: error handling

		outStream.write(static_cast<char*>(data), bytes);

		return bytes;
	};

	conn->SetWriteFunction(writeCallback);

	auto [code, body, _] = conn->get(release.downloadUrl);

	outStream.close();

	// TODO: error handling

	return code;
}

[[nodiscard]] bool models::InstanceConfig::RequestUpdateInfo()
{
	const auto conn = new RestClient::Connection("");

	conn->SetTimeout(5);
	conn->SetUserAgent(std::format("{}/{}", appFilename, appVersion.to_string()));
	conn->FollowRedirects(true);
	conn->FollowRedirects(true, 5);

	RestClient::HeaderFields headers;
	headers["Accept"] = "application/json";
	conn->SetHeaders(headers);

	SetCommonHeaders(conn);

	auto [code, body, _] = conn->get(updateRequestUrl);

	if (code != 200)
	{
		// TODO: add retry logic or similar
		return false;
	}

	try
	{
		const json reply = json::parse(body);
		remote = reply.get<UpdateResponse>();

		// top release is always latest by version, even if the response wasn't the right order
		std::ranges::sort(remote.releases, [](const auto& lhs, const auto& rhs)
		{
			return lhs.GetSemVersion() > rhs.GetSemVersion();
		});

		// bail out now if we are not supposed to obey the server settings
		if (authority == Authority::Local || !reply.contains("shared"))
		{
			return true;
		}

		// TODO: implement merging remote shared config

		shared = remote.shared;

		return true;
	}
	catch (...)
	{
		return false;
	}
}
