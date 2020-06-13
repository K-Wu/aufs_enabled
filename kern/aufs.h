#ifndef __AUFS_H__
#define __AUFS_H__
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/fs.h>
//macros defined here is visible in all source files under ./kern
#define MULTI_BLOCK_PTR_SCHEME
#ifdef MULTI_BLOCK_PTR_SCHEME
#define ZONE_PTR_IN_INODE_NUM 9
#else
#define ZONE_PTR_IN_INODE_NUM 1
#endif
#define AUFS_DDE_SIZE 32

static const unsigned long AUFS_MAGIC = 0x13131313;
static const unsigned long AUFS_DIR_SIZE = 64;

static const unsigned long AUFS_NAME_LEN = 60;

struct aufs_disk_super_block
{
	__be32 dsb_magic;
	__be32 dsb_block_size;
	__be32 dsb_root_inode;
	__be32 dsb_inode_blocks;
	__be32 dsb_inode_map_blocks;
	__be32 dsb_zone_map_blocks;
};

struct aufs_disk_inode
{
#ifdef MULTI_BLOCK_PTR_SCHEME
	__be32 di_zone_ptr[ZONE_PTR_IN_INODE_NUM];
#else
	__be32 di_zone_ptr;
#endif
	__be32 di_blocks; //inode->i_blocks does not use the same block granularity 4KiB as in block map or pagecache. it is 512B hard coded in the kernel.
	__be32 di_size;
	__be32 di_gid;
	__be32 di_uid;
	__be32 di_mode;
	__be64 di_ctime;
};

struct aufs_disk_dir_entry
{
	__be32 dde_inode;
	char dde_name[0]; //AUFS_DDE_MAX_NAME_LEN];
};

struct aufs_super_block
{
	unsigned long asb_magic;
	unsigned long asb_inode_blocks; //number of blocks inode entries occupy
	unsigned long asb_zone_size;
	unsigned long asb_root_inode;
	unsigned long asb_inodes_in_block;
	unsigned long asb_inode_map_blocks;
	unsigned long asb_zone_map_blocks;
	unsigned long asb_blocks_per_zone; //todo: assert unsigned long is 32 bit
	struct buffer_head **s_zmap;
	struct buffer_head **s_imap;
	struct buffer_head *s_sbh;
};

static inline struct aufs_super_block *AUFS_SB(struct super_block *sb)
{
	return (struct aufs_super_block *)sb->s_fs_info;
}

struct aufs_inode
{
#ifdef MULTI_BLOCK_PTR_SCHEME
	unsigned long ai_zone_ptr[ZONE_PTR_IN_INODE_NUM];
#else
	unsigned long ai_zone_ptr;
#endif
	struct inode ai_inode;
};

static inline struct aufs_inode *AUFS_INODE(struct inode *inode)
{
	//return (struct aufs_inode *)inode;
	return container_of(inode, struct aufs_inode, ai_inode);
}

static inline sector_t aufs_inode_block(struct aufs_super_block const *asb,
										ino_t inode_no)
{
	return (sector_t)(1 + asb->asb_inode_map_blocks + asb->asb_zone_map_blocks + inode_no / asb->asb_inodes_in_block); 
}

static inline size_t aufs_inode_offset(struct aufs_super_block const *asb,
									   ino_t inode_no)
{
	return sizeof(struct aufs_disk_inode) *
		   (inode_no % asb->asb_inodes_in_block); //todo: support undivided inode size if it is undivided
}

extern const struct address_space_operations aufs_aops;
extern const struct inode_operations aufs_dir_inode_ops;
extern const struct file_operations aufs_file_ops;
extern const struct file_operations aufs_dir_ops;

struct inode *aufs_inode_get(struct super_block *sb, unsigned long no);
struct inode *aufs_inode_alloc(struct super_block *sb);
void aufs_inode_free(struct inode *inode);

extern int aufs_new_zone(struct inode *inode);
extern int minix_write_inode(struct inode *inode, struct writeback_control *wbc);
extern struct aufs_disk_inode *minix_V2_raw_inode(struct super_block *, ino_t, struct buffer_head **);
extern const struct inode_operations minix_file_inode_operations;
extern const struct inode_operations aufs_dir_inode_ops;
extern int minix_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t rdev);
extern int minix_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
extern struct inode *aufs_new_inode(const struct inode *dir, umode_t mode, int *error);
extern int minix_set_inode(struct inode *inode, dev_t rdev);
extern int minix_prepare_chunk(struct page *page, loff_t pos, unsigned len);
extern int minix_add_link(struct dentry *dentry, struct inode *inode);
extern unsigned V2_minix_blocks(loff_t size, struct super_block *sb);
extern int minix_getattr(const struct path *path, struct kstat *stat, u32 request_mask, unsigned int flags);
extern int minix_make_empty(struct inode *inode, struct inode *dir);
extern int minix_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode);
extern struct aufs_disk_dir_entry *minix_find_entry(struct dentry *dentry, struct page **res_page);
extern ino_t minix_inode_by_name(struct dentry *dentry);
extern struct dentry *minix_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags);
extern int minix_tmpfile(struct inode *dir, struct dentry *dentry, umode_t mode);
#endif /*__AUFS_H__*/
