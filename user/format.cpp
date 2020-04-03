#include "linux/byteorder/little_endian.h"
#include <algorithm>
#include <ctime>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include "bit_iterator.hpp"
#include "format.hpp"


namespace
{

// uint64_t ntohll(uint64_t n)
// {
// 	uint64_t test = 1ull;
// 	if (*(char *)&test == 1ull)
// 		return (static_cast<uint64_t>(htonl(n & 0xffffffff)) << 32u) | static_cast<uint64_t>(htonl(n >> 32u));
// 	else
// 		return n;
// }

} // namespace

Inode::Inode(BlocksCache &cache, uint32_t no)
	: m_inode(no), m_block(nullptr), m_raw(nullptr)
{
	FillInode(cache);
}

uint32_t Inode::InodeNo() const noexcept
{
	return m_inode;
}

uint32_t Inode::FirstBlock() const noexcept
{
	return __be32_to_cpu(AI_FIRST_BLOCK(m_raw));//todo: remove ntohl(AI_FIRST_BLOCK(m_raw));
}

void Inode::SetFirstBlock(uint32_t block) noexcept
{
	AI_FIRST_BLOCK(m_raw) = __cpu_to_be32(block);//todo: remove ntohl(block);
}

uint32_t Inode::BlocksCount() const noexcept
{
	return __be32_to_cpu(AI_BLOCKS(m_raw));//todo: remove ntohl(AI_BLOCKS(m_raw));
}

void Inode::SetBlocksCount(uint32_t count) noexcept
{
	AI_BLOCKS(m_raw) = __cpu_to_be32(count);//todo: remove ntohl(count);
}

uint32_t Inode::Size() const noexcept
{
	return __be32_to_cpu(AI_SIZE(m_raw));//todo: remove ntohl(AI_SIZE(m_raw));
}

void Inode::SetSize(uint32_t size) noexcept
{
	AI_SIZE(m_raw) = __cpu_to_be32(size);//todo: remove ntohl(size);
}

uint32_t Inode::Gid() const noexcept
{
	return __be32_to_cpu(AI_GID(m_raw));//todo: remove ntohl(AI_GID(m_raw));
}

void Inode::SetGid(uint32_t gid) noexcept
{
	AI_GID(m_raw) = __cpu_to_be32(gid);//todo: remove ntohl(gid);
}

uint32_t Inode::Uid() const noexcept
{
	return __be32_to_cpu(AI_UID(m_raw));//todo: remove ntohl(AI_UID(m_raw));
}

void Inode::SetUid(uint32_t uid) noexcept
{
	AI_UID(m_raw) = __cpu_to_be32(uid);//todo: remove ntohl(uid);
}

uint32_t Inode::Mode() const noexcept
{
	return __be32_to_cpu(AI_MODE(m_raw));//todo: remove ntohl(AI_MODE(m_raw));
}

void Inode::SetMode(uint32_t mode) noexcept
{
	AI_MODE(m_raw) = __cpu_to_be32(mode);//todo: remove ntohl(mode);
}

uint64_t Inode::CreateTime() const noexcept
{
	return __be64_to_cpu(AI_CTIME(m_raw));//todo: remove ntohll(AI_CTIME(m_raw));
}

void Inode::SetCreateTime(uint64_t ctime) noexcept
{
	AI_CTIME(m_raw) = __cpu_to_be64(ctime);//todo: remove ntohll(ctime);
}

void Inode::FillInode(BlocksCache &cache)
{
	ConfigurationConstPtr const config = cache.Config();
	size_t const in_block = config->BlockSize() / sizeof(struct aufs_inode);
	size_t const block = InodeNo() / in_block + 3; //todo: modify according to 1+zoneMap+blockMap+inodeMap
	size_t const index = InodeNo() % in_block;
	size_t const offset = index * sizeof(struct aufs_inode);

	m_block = cache.GetBlock(block);
	m_raw = reinterpret_cast<struct aufs_inode *>(m_block->Data() + offset);
	//AI_CTIME(m_raw) = ntohll(time(NULL));
	SetCreateTime(time(NULL));
}

SuperBlock::SuperBlock(BlocksCache &cache)
	: m_super_block(cache.GetBlock(0)), m_block_map(cache.GetBlock(1)), m_inode_map(cache.GetBlock(2))
{
	FillBlockMap(cache);
	FillInodeMap(cache);
	FillSuper(cache);
}

uint32_t SuperBlock::AllocateInode() noexcept
{
	BitIterator const e(m_inode_map->Data() + m_inode_map->Size() * 8, 0);
	BitIterator const b(m_inode_map->Data(), 0);

	BitIterator it = std::find(b, e, false);
	if (it != e)
	{
		*it = true;
		return static_cast<size_t>(it - b);
	}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wterminate"
	throw std::runtime_error("Cannot allocate inode");
#pragma GCC diagnostic pop
	return 0;
}

uint32_t SuperBlock::AllocateBlocks(size_t blocks) noexcept
{
	BitIterator const e(m_block_map->Data() + m_block_map->Size() * 8, 0);
	BitIterator const b(m_block_map->Data(), 0);

	BitIterator it = std::find(b, e, false);
	while (it != e)
	{
		BitIterator jt = std::find(it, e, true);
		if (static_cast<size_t>(jt - it) >= blocks)
		{
			std::fill(it, it + blocks, true);
			return it - b;
		}
		it = jt;
	}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wterminate"
	throw std::runtime_error("Cannot allocate blocks");
#pragma GCC diagnostic pop
	return 0;
}

void SuperBlock::SetRootInode(uint32_t root) noexcept
{
	struct aufs_super_block *sb =
		reinterpret_cast<struct aufs_super_block *>(
			m_super_block->Data());

	ASB_ROOT_INODE(sb) = htonl(root);
}

void SuperBlock::FillSuper(BlocksCache &cache) noexcept
{
	struct aufs_super_block *sb =
		reinterpret_cast<struct aufs_super_block *>(
			m_super_block->Data());

	ASB_MAGIC(sb) = htonl(AUFS_MAGIC);
	ASB_BLOCK_SIZE(sb) = htonl(cache.Config()->BlockSize());
	ASB_ROOT_INODE(sb) = htonl(-1);
	ASB_INODE_BLOCKS(sb) = htonl(cache.Config()->InodeBlocks());
	ASB_INODE_MAP_BLOCKS(sb) = htonl(1); //todo: support multiple-block inode maps
	ASB_BLOCK_MAP_BLOCKS(sb) = htonl(1); //todo: support multiple-block zone maps
	ASB_BLOCK_PER_ZONE(sb) = htonl(1);	 //todo: support non-1 block per zone
}

void SuperBlock::FillBlockMap(BlocksCache &cache) noexcept
{
	size_t const blocks = std::min(cache.Config()->Blocks(),
								   cache.Config()->BlockSize() * 8);
	//size_t const blocks= cache.Config()->Blocks();//todo: support multiple-block zone maps
	//size_t const inode_blocks = cache.Config()->InodeBlocks();

	BitIterator const it(m_block_map->Data(), 0);
	std::fill(it, it+1, true);	//std::fill(it, it + 3 + inode_blocks + 2, true);			  //todo: support multiple-block zone maps
	std::fill(it + 1, it + blocks, false); //std::fill(it + 3 + inode_blocks + 2, it + blocks, false); //2: 1 fore root inode and 1 for badblock
	std::fill(it + blocks, it + cache.Config()->BlockSize() * 8, true);
}

void SuperBlock::FillInodeMap(BlocksCache &cache) noexcept
{
	uint32_t const in_block =
		cache.Config()->BlockSize() / sizeof(struct aufs_inode);
	uint32_t const inode_blocks = cache.Config()->InodeBlocks();
	uint32_t const inodes = std::min(inode_blocks * in_block,
									 cache.Config()->BlockSize() * 8); //todo: support multiple-block zone maps

	BitIterator const it(m_inode_map->Data(), 0);
	std::fill(it, it + 1, true);
	std::fill(it + 1, it + inodes, false);
	std::fill(it + inodes, it + cache.Config()->BlockSize() * 8, true);
}

void Formatter::SetRootInode(Inode const &inode) noexcept
{
	m_super.SetRootInode(inode.InodeNo());
}

Inode Formatter::MkDir(uint32_t entries)
{
	uint32_t const bytes = entries * AUFS_DIR_SIZE;
	uint32_t const blocks = (bytes + m_config->BlockSize() - 1) /
							m_config->BlockSize();
	Inode inode(m_cache, m_super.AllocateInode());
	uint32_t block = m_super.AllocateBlocks(blocks);

	inode.SetFirstBlock(block);
	inode.SetBlocksCount(blocks);
	inode.SetSize(0);
	inode.SetUid(getuid());
	inode.SetGid(getgid());
	inode.SetMode(493 | S_IFDIR);

	return inode;
}
#include <iostream>
Inode Formatter::MkRootDir()
{
	uint32_t const bytes = 2 * AUFS_DIR_SIZE;
	uint32_t const blocks = (bytes + m_config->BlockSize() - 1) /
							m_config->BlockSize();
	Inode inode(m_cache, m_super.AllocateInode());
	uint32_t block = m_super.AllocateBlocks(blocks)+1+m_config->InodeBlocks()+1+1;//todo: support multiple inode maps blocks and zone maps blocks
	std::cout << "root inode no: " << inode.InodeNo() << "\n";
	std::cout << "root inode first block no: " << block << "\n";
	std::cout << "root inode block_size: " << m_config->BlockSize() << "\n";
	inode.SetFirstBlock(block);
	inode.SetBlocksCount(blocks);
	inode.SetSize(2 * AUFS_DIR_SIZE);
	inode.SetUid(getuid());
	inode.SetGid(getgid());
	//inode.SetMode(493 | S_IFDIR);
	inode.SetMode(S_IFDIR + 0755);
	inode.SetCreateTime(time(NULL));
	//BlockPtr rootContent = m_cache.GetBlock(3+m_config->InodeBlocks()+inode.FirstBlock());//todo: modify according to extent map block map inode map inodes
	BlockPtr rootContent = m_cache.GetBlock(block);
	BitIterator const it(rootContent->Data(), 0);
	//struct  aufs_disk_dir_entry this_parent_dirs[3]={{htonl(1),"."},{htonl(1),".."},{htonl(2),".badblocks"}};
	struct aufs_disk_dir_entry *raw_parent_dirs = reinterpret_cast<struct aufs_disk_dir_entry *>(
		rootContent->Data());
	raw_parent_dirs[0].dde_inode = htonl(1);
	strcpy((char *)(((uint8_t *)&raw_parent_dirs[0]) + sizeof(uint32_t)), ".");
	raw_parent_dirs[1].dde_inode = htonl(1);
	strcpy((char *)(((uint8_t *)&raw_parent_dirs[1]) + sizeof(uint32_t)), "..");
	raw_parent_dirs[2].dde_inode = htonl(2);
	strcpy((char *)(((uint8_t *)&raw_parent_dirs[2]) + sizeof(uint32_t)), ".badblocks");

	return inode;
}

Inode Formatter::MkFile(uint32_t size)
{
	uint32_t const blocks = (size + m_config->BlockSize() - 1) /
							m_config->BlockSize();
	Inode inode(m_cache, m_super.AllocateInode());
	uint32_t block = m_super.AllocateBlocks(blocks);

	inode.SetFirstBlock(block);
	inode.SetBlocksCount(blocks);
	inode.SetUid(getuid());
	inode.SetGid(getgid());
	inode.SetMode(493 | S_IFREG);

	return inode;
}

uint32_t Formatter::Write(Inode &inode, uint8_t const *data, uint32_t size)
{
	if (!(inode.Mode() & S_IFREG))
		throw std::logic_error("it is not file");

	uint32_t const left = inode.BlocksCount() * m_config->BlockSize() -
						  inode.Size();
	if (left < size)
		throw std::out_of_range("there is no enough space");

	uint32_t const block = inode.FirstBlock() + inode.Size() /
													m_config->BlockSize();
	uint32_t const offset = inode.Size() % m_config->BlockSize();
	uint32_t const towrite = std::min(size, m_config->BlockSize() - offset);

	BlockPtr bp = m_cache.GetBlock(block);
	std::copy_n(data, towrite, bp->Data() + offset);
	inode.SetSize(inode.Size() + towrite);

	return towrite;
}

void Formatter::AddChild(Inode &inode, char const *name, Inode const &ch)
{
	if (!(inode.Mode() & S_IFDIR))
		throw std::logic_error("it is not directory");

	uint32_t const inblock = m_config->BlockSize() / AUFS_DIR_SIZE;
	uint32_t const entries = inode.BlocksCount() * inblock;
	uint32_t const left = entries - inode.Size();

	if (!left)
		throw std::out_of_range("there is no enough space");

	uint32_t const block = inode.FirstBlock() + inode.Size() / inblock;
	uint32_t const offset = inode.Size() % inblock;

	BlockPtr bp = m_cache.GetBlock(block);
	struct aufs_disk_dir_entry *dp = reinterpret_cast<struct aufs_disk_dir_entry *>(
										 bp->Data()) +
									 offset;
	strncpy(dp->dde_name, name, AUFS_NAME_MAXLEN - 1);
	dp->dde_name[AUFS_NAME_MAXLEN - 1] = '\0';
	dp->dde_inode = htonl(ch.InodeNo());
	inode.SetSize(inode.Size() + 1);
}
