#include <algorithm>
#include <iostream>
#include <iterator>
#include <fstream>
#include <vector>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include "format.hpp"

size_t DeviceSize(std::string const &device)
{
	std::ifstream in(device.c_str(),
					 std::ios::in | std::ios::binary | std::ios::ate);
	std::ifstream::streampos const size = in.tellg();

	return static_cast<size_t>(size);
}

bool VerifyZones(ConfigurationConstPtr config)
{
	if (config->Zones() <= 3)
		return false;

	return true;
}

bool VerifyDevice(ConfigurationConstPtr config)
{
	size_t const size = DeviceSize(config->Device());

	if (size < config->Zones() * config->ZoneSize())
		return false;

	return true;
}

bool VerifyZoneSize(ConfigurationConstPtr config)
{
	if (config->ZoneSize() != 512u && config->ZoneSize() != 1024u &&
		config->ZoneSize() != 2048u && config->ZoneSize() != 4096u && config->ZoneSize() != 16384u)
		return false;

	if (config->BlockSize() * 8 < config->Zones())
		std::cout << "WARNING: With zone size = "
				  << config->ZoneSize() << " zones number should be "
				  << "less or equal to " << config->BlockSize() * 8
				  << std::endl;

	return true;
}

ConfigurationConstPtr VerifyConfiguration(ConfigurationConstPtr config)
{
	if (!VerifyZoneSize(config))
		throw std::runtime_error("Unsupported zone size");

	if (!VerifyZones(config))
		throw std::runtime_error("Wrong number of zones");

	if (!VerifyDevice(config))
		throw std::runtime_error("Device is too small");

	return config;
}

void PrintHelp()
{
	std::cout << "Usage:" << std::endl
			  << "\tmkfs.aufs [(--zone_size | -s) SIZE] [(--blocks | -b) BLOCKS] DEVICE"
			  << std::endl
			  << std::endl
			  << "Where:" << std::endl
			  << "\tSIZE    - block size. Default is 4096 bytes." << std::endl
			  << "\tBLOCKS  - number of blocks would be used for aufs. By default is DEVICE size / SIZE." << std::endl
			  << "\tDEVICE  - device file." << std::endl;
}

ConfigurationConstPtr ParseArgs(int argc, char **argv)
{
	std::string device, dir;
	size_t zone_size = 16384u;
	size_t block_size = 4096u;
	size_t blocks = 0;

	while (argc--)
	{
		std::string const arg(*argv++);
		if ((arg == "--blocks" || arg == "-b") && argc)
		{
			blocks = std::stoi(*argv++);
			--argc;
		}
		else if ((arg == "--zone_size" || arg == "-s") && argc)
		{
			zone_size = std::stoi(*argv++);
			--argc;
		}
		else if ((arg == "--dir" || arg == "-d") && argc)
		{
			dir = *argv++;
			--argc;
		}
		else if (arg == "--help" || arg == "-h")
		{
			PrintHelp();
		}
		else
		{
			device = arg;
		}
	}

	if (device.empty())
		throw std::runtime_error("Device name expected");

	if (blocks == 0)
		blocks = std::min(DeviceSize(device) / zone_size, block_size * 8);

	ConfigurationConstPtr config = std::make_shared<Configuration>(
		device, dir, blocks, zone_size);

	return VerifyConfiguration(config);
}


int main(int argc, char **argv)
{
	try
	{
		ConfigurationConstPtr config = ParseArgs(argc - 1, argv + 1);
		Formatter format(config);

		//if (!config->SourceDir().empty())
		//		format.SetRootInode(CopyDir(format,
		//				config->SourceDir()));
		//else
		format.SetRootInode(format.MkRootDir());

		return 0;
	}
	catch (std::exception const &e)
	{
		std::cout << "ERROR: " << e.what() << std::endl;
		PrintHelp();
	}

	return 1;
}
