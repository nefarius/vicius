#include "pch.h"
#define _CRT_SECURE_NO_WARNINGS
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
	const auto ua = std::format("{}/{}", appFilename, appVersion.to_string());
	spdlog::debug("Setting User Agent to {}", ua);
	conn->SetUserAgent(ua);
	conn->FollowRedirects(true);
	conn->FollowRedirects(true, 5);
	conn->SetFileProgressCallback(progressFn);

	SetCommonHeaders(conn);

	// pre-allocate buffers
	std::string tempPath(MAX_PATH, '\0');
	std::string tempFile(MAX_PATH, '\0');

	if (GetTempPathA(MAX_PATH, tempPath.data()) == 0)
	{
		spdlog::error("Failed to get path to temporary directory, error", GetLastError());
		return -1;
	}

	spdlog::debug("tempPath = {}", tempPath);

	if (GetTempFileNameA(tempPath.c_str(), "VICIUS", 0, tempFile.data()) == 0)
	{
		spdlog::error("Failed to get temporary file name, error", GetLastError());
		return -1;
	}

	spdlog::debug("tempFile = {}", tempFile);

	auto& release = GetSelectedRelease();
	release.localTempFilePath = tempFile;

	// this is ugly but only one download can run in parallel so we're fine :)
	static std::ofstream outStream;

	const std::ios_base::iostate exceptionMask = outStream.exceptions() | std::ios::failbit;
	outStream.exceptions(exceptionMask);

	try
	{
		outStream.open(release.localTempFilePath.string(), std::ios::binary);
	}
	catch (std::ios_base::failure& e)
	{
		spdlog::error("Failed to open file {}, error {}",
			release.localTempFilePath.string(), e.what());
		return -1;
	}

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

	// TODO: error handling, retry?
	if (code != 200)
	{
		spdlog::error("GET request failed with code {}", code);
	}

	return code;
}

[[nodiscard]] bool models::InstanceConfig::RequestUpdateInfo()
{
	const auto conn = new RestClient::Connection("");

	conn->SetTimeout(5);
	const auto ua = std::format("{}/{}", appFilename, appVersion.to_string());
	spdlog::debug("Setting User Agent to {}", ua);
	conn->SetUserAgent(ua);
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
		spdlog::error("GET request failed with code {}", code);
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
			spdlog::info("{} authority specified (or empty response), ignoring server parameters",
				magic_enum::enum_name(authority));
			return true;
		}

		// TODO: implement merging remote shared config

		spdlog::info("Processing remote shared configuration parameters");
		shared = remote.shared;

		return true;
	}
	catch (const json::exception& e)
	{
		spdlog::error("Failed to parse JSON, error {}", e.what());
		return false;
	}
	catch (...)
	{
		spdlog::error("Unexpected error during response parsing");
		return false;
	}
}
