#ifndef __BLOCK_HPP__
#define __BLOCK_HPP__

#include <cstddef>
#include <cstdint>
#include <istream>
#include <map>
#include <memory>
#include <ostream>
#include <string>
#include <vector>
#include <iostream>
#include "aufs.hpp"

class Configuration
{
public:
	explicit Configuration(std::string device,
						   std::string dir,
						   uint32_t n_zones,
						   uint32_t zone_size,
						   uint32_t num_block_inode_map,
						   uint32_t num_block_zone_map,
						   uint32_t num_block_alignment) noexcept
		: m_device(device), m_dir(dir), m_zone_size(zone_size), m_inode_map_blocks(num_block_inode_map),m_zone_map_blocks(num_block_zone_map), m_num_block_alignment(num_block_alignment)
	{
		//body is executed after initializer list.
		//A copy of stnadard can be found at https://stackoverflow.com/questions/4037219/order-of-execution-in-constructor-initialization-list.
		std::cout << "num_block_zone_map*block_size*8("<<num_block_zone_map*BlockSize()*8<<") n_zones("<<n_zones<<")"<<std::endl;
		m_device_zones = std::min(num_block_zone_map*BlockSize()*8,n_zones);
		m_inode_blocks=CountInodeEntryBlocks();
	}

	std::string const &Device() const noexcept
	{
		return m_device;
	}

	std::string const &SourceDir() const noexcept
	{
		return m_dir;
	}

	uint32_t NumZones() const noexcept
	{
		return m_device_zones;
	}

	uint32_t NumInodeEntryBlocks() const noexcept
	{
		std::cout << "inode blocks: " << m_inode_blocks << std::endl;
		return m_inode_blocks;
	}

	uint32_t ZoneSize() const noexcept
	{
		return m_zone_size;
	}

	uint32_t BlockSize() const noexcept
	{
		return (m_zone_size > 4096) ? 4096 : m_zone_size;
	}

	uint32_t NumInodeMapBlocks() const noexcept
	{
		return m_inode_map_blocks;
	}
	
	uint32_t NumZoneMapBlocks() const noexcept
	{
		return m_zone_map_blocks;
	}

	uint32_t CalculateAlignedNumBlocks(uint32_t num_blocks) const noexcept
	{

		return (integer_round_up_division(num_blocks,m_num_block_alignment)*m_num_block_alignment);
	}

	uint32_t NumBlockAlignment() const noexcept
	{
		return m_num_block_alignment;
	}

private:
	uint32_t integer_round_up_division(uint32_t dividend, uint32_t divisor) const noexcept
	{
		return (dividend+divisor-1)/divisor;
	}

	uint32_t CountInodeEntryBlocks() const noexcept
	{
		std::cout << "NumZones() " << NumZones() << "\n";
		uint32_t const inodes = std::min(NumZones(), NumInodeMapBlocks() * BlockSize() * 8);
		std::cout << "BLOCK SIZE " << BlockSize() << " , ZoneSize(): " << ZoneSize() << " in CountInodeBLocks: \n";
		uint32_t const in_block = BlockSize() /
								  sizeof(struct aufs_inode);

		return (inodes + in_block - 1) / in_block;
	}

	std::string m_device;
	std::string m_dir;
	uint32_t m_device_zones;
	uint32_t m_zone_size;
	uint32_t m_inode_blocks;
	uint32_t m_inode_map_blocks;
	uint32_t m_zone_map_blocks;
	uint32_t m_num_block_alignment;
};

using ConfigurationPtr = std::shared_ptr<Configuration>;
using ConfigurationConstPtr = std::shared_ptr<Configuration const>;

class Block
{
public:
	explicit Block(ConfigurationConstPtr config, size_t no)
		: m_config(config), m_number(no), m_data(m_config->BlockSize(), 0)
	{
	}

	Block(Block const &) = delete;
	Block &operator=(Block const &) = delete;

	Block(Block &&) = delete;
	Block &operator=(Block &&) = delete;

	size_t BlockNo() const noexcept
	{
		return m_number;
	}

	uint8_t *Data() noexcept
	{
		return m_data.data();
	}

	uint8_t const *Data() const noexcept
	{
		return m_data.data();
	}

	size_t Size() const noexcept
	{
		return m_config->BlockSize();
	}

	std::ostream &Dump(std::ostream &out) const
	{
		return out.write(reinterpret_cast<char const *>(Data()), Size());
	}

	std::istream &Read(std::istream &in)
	{
		return in.read(reinterpret_cast<char *>(Data()), Size());
	}

private:
	ConfigurationConstPtr m_config;
	size_t m_number;
	std::vector<uint8_t> m_data;
};

using BlockPtr = std::shared_ptr<Block>;
using BlockConstPtr = std::shared_ptr<Block const>;

class BlocksCache
{
public:
	explicit BlocksCache(ConfigurationConstPtr config);
	~BlocksCache();

	ConfigurationConstPtr Config() const noexcept;
	BlockPtr GetBlock(size_t no);
	void Sync();

	BlocksCache(BlocksCache &&) = delete;
	BlocksCache &operator=(BlocksCache &&) = delete;

	BlocksCache(BlocksCache const &) = delete;
	BlocksCache &operator=(BlocksCache const &) = delete;

private:
	BlockPtr ReadBlock(std::istream &in, size_t no);
	void WriteBlock(std::ostream &out, BlockPtr block);

	ConfigurationConstPtr m_config;
	std::map<size_t, BlockPtr> m_cache;
};

#endif /*__BLOCK_HPP__*/
