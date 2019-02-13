#include <iostream>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#define STORMLIB_NO_AUTO_LINK
#include "stormlib.h"

using namespace std::literals;

void replace_all(std::string& input, std::string_view search, std::string_view replace)
{
	auto pos = input.find(search);

	while (pos != std::string::npos)
	{
		input.replace(pos, search.size(), replace);
		pos = input.find(search, pos + replace.size());
	}
}

int main(int argc, char **argv)
{
	std::vector<std::string> args(argv, argv + argc);
	std::filesystem::path bnet_install_dir(L"C:\\Program Files (x86)\\Battle.net");

	if (args.size() > 1)
		bnet_install_dir = args.at(1);

	try
	{
		const std::wregex bnet_version_regex(L"Battle\\.net\\.(\\d+)");

		std::filesystem::path bnet_latest_path;
		int bnet_latest_version = 0;

		for (auto& entry : std::filesystem::directory_iterator(bnet_install_dir))
		{
			if (!entry.is_directory())
				continue;

			const auto path = entry.path().filename().wstring();
			std::wsmatch bnet_version_match;

			if (std::regex_match(path, bnet_version_match, bnet_version_regex))
			{
				int version = std::stoi(bnet_version_match[1].str());

				if (version > bnet_latest_version)
				{
					bnet_latest_path = entry;
					bnet_latest_version = version;
				}
			}
		}

		if (bnet_latest_version == 0)
			throw std::runtime_error("Latest Battle.net version not found");

		if (std::filesystem::exists(bnet_latest_path / L"resources\\play\\GameTabList.qml"))
		{
			std::cout << "Nothing to do, " << bnet_latest_path << " is already fixed" << std::endl;
			std::exit(0);
		}

		std::cout << "Fixing " << bnet_latest_path << std::endl;

		HANDLE mpq_handle, qml_handle;

		if (!SFileOpenArchive((bnet_latest_path / L"Battle.net.mpq").c_str(), 0,
			STREAM_PROVIDER_FLAT | BASE_PROVIDER_FILE | STREAM_FLAG_READ_ONLY, &mpq_handle))
			throw std::runtime_error("Could not open archive");

		if (!SFileOpenFileEx(mpq_handle, "resources\\play\\GameTabList.qml", SFILE_OPEN_FROM_MPQ,
			&qml_handle))
			throw std::runtime_error("Could not open file in archive");

		DWORD qml_size;

		if (!SFileGetFileInfo(qml_handle, SFileInfoFileSize, &qml_size, sizeof(DWORD), nullptr))
			throw std::runtime_error("Could not get file information");

		std::vector<char> qml_data(qml_size);

		if (!SFileReadFile(qml_handle, qml_data.data(), qml_size, nullptr, nullptr))
			throw std::runtime_error("Could not read file");

		if (!SFileCloseFile(qml_handle))
			throw std::runtime_error("Could not close file");

		std::string qml_file(qml_data.data(), qml_data.size());
		replace_all(qml_file, "return gameController.activisionProductsModel.count>0;",
			"return false;");

		const std::filesystem::path output_dir(bnet_latest_path / L"resources\\play");
		
		std::filesystem::create_directories(output_dir);
		std::ofstream qml_replacement(output_dir / L"GameTabList.qml", std::ios::binary);

		if (!qml_replacement.good())
			throw std::runtime_error("Could not open replacement file for writing");

		qml_replacement << qml_file;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error: " << e.what() << std::endl;
		std::exit(1);
	}

	return 0;
}
