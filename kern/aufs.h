#ifndef __AUFS_H__
#define __AUFS_H__
#include <linux/buffer_head.h>
#include <linux/types.h>
#include <linux/fs.h>

#define AUFS_DDE_SIZE         32
//#define AUFS_DDE_MAX_NAME_LEN (AUFS_DDE_SIZE - sizeof(__be32))

static const unsigned long AUFS_MAGIC = 0x13131313;
static const unsigned long AUFS_DIR_SIZE = 64;
static const unsigned long AUFS_NAME_LEN = 60;


struct aufs_disk_super_block {
	__be32	dsb_magic;
	__be32	dsb_block_size;
	__be32	dsb_root_inode;
	__be32	dsb_inode_blocks;
	__be32 dsb_inode_map_blocks;
	__be32 dsb_zone_map_blocks;
	__be32 dsb_blocks_per_zone;
};

struct aufs_disk_inode {
	__be32	di_first;
	__be32	di_blocks;//todo: needs to update di_first and di_blocks once allocated and once write
	__be32	di_size;
	__be32	di_gid;
	__be32	di_uid;
	__be32	di_mode;
	__be64	di_ctime;
};

struct aufs_disk_dir_entry {
	__be32 dde_inode;
	char dde_name[0];//AUFS_DDE_MAX_NAME_LEN];
};

struct aufs_super_block {
	unsigned long asb_magic;
	unsigned long asb_inode_blocks;
	unsigned long asb_block_size;
	unsigned long asb_root_inode;
	unsigned long asb_inodes_in_block;
	unsigned long asb_inode_map_blocks;
	unsigned long asb_zone_map_blocks;
	unsigned long asb_blocks_per_zone;//todo: assert unsigned long is 32 bit
	struct buffer_head** s_zmap;
	struct buffer_head** s_imap;//vtodo: verify done: read bitmap to initialize s_imap
};

static inline struct aufs_super_block *AUFS_SB(struct super_block *sb)
{
	return (struct aufs_super_block *)sb->s_fs_info;
}

struct aufs_inode {
	struct inode ai_inode;
	unsigned long ai_first_block;
};

static inline struct aufs_inode *AUFS_INODE(struct inode *inode)
{
	return (struct aufs_inode *)inode;
}

extern const struct address_space_operations aufs_aops;
extern const struct inode_operations aufs_dir_inode_ops;
extern const struct file_operations aufs_file_ops;
extern const struct file_operations aufs_dir_ops;

struct inode *aufs_inode_get(struct super_block *sb, unsigned long no);
struct inode *aufs_inode_alloc(struct super_block *sb);
void aufs_inode_free(struct inode *inode);

extern int aufs_new_zone(struct inode * inode);

extern int minix_write_inode(struct inode *inode, struct writeback_control *wbc);
extern struct aufs_disk_inode * minix_V2_raw_inode(struct super_block *, ino_t, struct buffer_head **);
extern const struct inode_operations minix_file_inode_operations;
extern const struct inode_operations aufs_dir_inode_ops;
extern int minix_mknod(struct inode * dir, struct dentry *dentry, umode_t mode, dev_t rdev);
extern int minix_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl);
extern struct inode *aufs_new_inode(const struct inode *dir, umode_t mode, int *error);
extern int minix_set_inode(struct inode *inode, dev_t rdev);
extern int minix_prepare_chunk(struct page *page, loff_t pos, unsigned len);
extern int minix_add_link(struct dentry *dentry, struct inode *inode);

#endif /*__AUFS_H__*/
