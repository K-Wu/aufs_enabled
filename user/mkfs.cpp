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
	if (config->NumZones() <= 3)
		return false;

	return true;
}

bool VerifyDevice(ConfigurationConstPtr config)
{
	size_t const size = DeviceSize(config->Device());

	if (size < config->NumZones() * config->ZoneSize())
		return false;

	return true;
}

bool IsPowerOfTwo(uint32_t ZoneSize){
	while (ZoneSize>1){
		if (ZoneSize%2==1)
			return false;
		ZoneSize/=2;
	}
	return true;
}

bool VerifyZoneSize(ConfigurationConstPtr config)
{
	if (config->ZoneSize() < 512u || !IsPowerOfTwo(config->ZoneSize()))
		return false;

	if (config->BlockSize() * 8 < config->NumZones())
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
			  << "\tmkfs.aufs [(--zonesize | -s) SIZE] [(--nzones | -b) NUMZONES] DEVICE"
			  << std::endl
			  << std::endl
			  << "Where:" << std::endl
			  << "\tSIZE    - zone size. Default is 4096 bytes." << std::endl
			  << "\tBLOCKS  - number of zones would be used for aufs. By default is DEVICE size / SIZE." << std::endl
			  << "\tDEVICE  - device file." << std::endl;
}

ConfigurationConstPtr ParseArgs(int argc, char **argv)
{
	std::string device, dir;
	size_t zone_size = 16384u;
	size_t block_size;
	size_t num_zones = 0;

	while (argc--)
	{
		std::string const arg(*argv++);
		if ((arg == "--nzones" || arg == "-b") && argc)
		{
			num_zones = std::stoi(*argv++);
			--argc;
		}
		else if ((arg == "--zonesize" || arg == "-s") && argc)
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

	block_size=(zone_size>(4096u))?(4096u):(zone_size);

	if (device.empty())
		throw std::runtime_error("Device name expected");

	if (num_zones == 0)
		num_zones = std::min(DeviceSize(device) / zone_size, block_size * 8);

	ConfigurationConstPtr config = std::make_shared<Configuration>(
		device, dir, num_zones, zone_size);

	return VerifyConfiguration(config);
}


int main(int argc, char **argv)
{
	try
	{
		ConfigurationConstPtr config = ParseArgs(argc - 1, argv + 1);
		Formatter format(config);

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
