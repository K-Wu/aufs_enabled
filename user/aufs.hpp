#ifndef __AUFS_HPP__
#define __AUFS_HPP__

#include <cassert>
#include <cstring>
#include <cstdint>
#define ZONE_PTR_IN_INODE_NUM 9
static uint32_t const AUFS_MAGIC = 0x13131313;
static uint32_t const AUFS_NAME_MAXLEN = 60;

struct aufs_super_block
{
	uint32_t asb_magic;
	uint32_t asb_zone_size;
	uint32_t asb_root_inode;
	uint32_t asb_inode_blocks;
	uint32_t asb_inode_map_blocks;
	uint32_t asb_zone_map_blocks;
};

static inline uint32_t &ASB_MAGIC(struct aufs_super_block *asb)
{
	return asb->asb_magic;
}

static inline uint32_t &ASB_ZONE_SIZE(struct aufs_super_block *asb)
{
	return asb->asb_zone_size;
}

static inline uint32_t &ASB_ROOT_INODE(struct aufs_super_block *asb)
{
	return asb->asb_root_inode;
}

static inline uint32_t &ASB_INODE_BLOCKS(struct aufs_super_block *asb)
{
	return asb->asb_inode_blocks;
}

static inline uint32_t &ASB_INODE_MAP_BLOCKS(struct aufs_super_block *asb)
{
	return asb->asb_inode_map_blocks;
}

static inline uint32_t &ASB_BLOCK_MAP_BLOCKS(struct aufs_super_block *asb)
{
	return asb->asb_zone_map_blocks;
}

struct aufs_inode
{
	uint32_t ai_zone_ptr[ZONE_PTR_IN_INODE_NUM];
	uint32_t ai_blocks;
	uint32_t ai_size;
	uint32_t ai_gid;
	uint32_t ai_uid;
	uint32_t ai_mode;
	uint64_t ai_ctime;
};

static inline uint32_t &AI_ZONE_PTR_FIRST(struct aufs_inode *ai)
{
	return ai->ai_zone_ptr[0];
}

static inline uint32_t* AI_ZONE_PTR(struct aufs_inode *ai)
{
	return ai->ai_zone_ptr;
}


static inline uint32_t &AI_BLOCKS(struct aufs_inode *ai)
{
	return ai->ai_blocks;
}

static inline uint32_t &AI_SIZE(struct aufs_inode *ai)
{
	return ai->ai_size;
}

static inline uint32_t &AI_GID(struct aufs_inode *ai)
{
	return ai->ai_gid;
}

static inline uint32_t &AI_UID(struct aufs_inode *ai)
{
	return ai->ai_uid;
}

static inline uint32_t &AI_MODE(struct aufs_inode *ai)
{
	return ai->ai_mode;
}

static inline uint64_t &AI_CTIME(struct aufs_inode *ai)
{
	return ai->ai_ctime;
}

static const unsigned long AUFS_DIR_SIZE = 64;

struct aufs_disk_dir_entry
{
	uint32_t dde_inode;
	char dde_name[AUFS_DIR_SIZE - sizeof(uint32_t)];
};

static inline uint32_t &ADE_INODE(struct aufs_disk_dir_entry *ade)
{
	return ade->dde_inode;
}

static inline char const *ADE_NAME(struct aufs_disk_dir_entry const *ade)
{
	return ade->dde_name;
}

static inline char *ADE_NAME(struct aufs_disk_dir_entry *ade)
{
	return ade->dde_name;
}

#endif /*__AUFS_HPP__*/
