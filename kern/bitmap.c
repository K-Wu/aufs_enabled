#include "aufs.h"
#include "minix.h"
#include <linux/buffer_head.h>
#include <linux/bitops.h>
#include <linux/sched.h>

static DEFINE_SPINLOCK(bitmap_lock);

int aufs_new_zone(struct inode * inode)//return the block id of the new zone
{//vtodo: verify done: assimilate this //todo: verify
//todo: originally need to memset 0 after new block (see itree_common.c. alloc_branch()). move that to block free
	struct aufs_super_block *sbi = AUFS_SB(inode->i_sb);
	int bits_per_block = 8 * inode->i_sb->s_blocksize;
	int i;
	
	for (i = 0; i < sbi->asb_zone_map_blocks ; i++) {
		struct buffer_head *bh = sbi->s_zmap[i];
		int j;

		spin_lock(&bitmap_lock);
		j = minix_find_first_zero_bit(bh->b_data, bits_per_block);
		if (j < bits_per_block) {
			minix_set_bit(j, bh->b_data);
			spin_unlock(&bitmap_lock);
			mark_buffer_dirty(bh);
			j += i * bits_per_block * (sbi->asb_blocks_per_zone) + 1+sbi->asb_inode_map_blocks+sbi->asb_zone_map_blocks;//todo: propagate block size per zone map bit
			if (j < (1+sbi->asb_inode_map_blocks+sbi->asb_zone_map_blocks) /*|| j >= sbi->s_nzones*/)//todo: add s_nzones to super block
				break;
			return j;
		}
		spin_unlock(&bitmap_lock);
	}
	return 0;
}


struct inode *aufs_new_inode(const struct inode *dir, umode_t mode, int *error)//adapt from minix_new_inode
{//vtodo: verify done: assimilate this
//vtodo: verify done: aufs new inode: mimic minix_new_inode
	struct super_block *sb = dir->i_sb;
	struct aufs_super_block *sbi = AUFS_SB(sb);
	struct inode *inode = new_inode(sb);
	struct buffer_head * bh;
	int bits_per_block = 8 * sb->s_blocksize;
	unsigned long j;
	int i;

	if (!inode) {
		*error = -ENOMEM;
		return NULL;
	}
	j = bits_per_block;
	bh = NULL;
	*error = -ENOSPC;
	spin_lock(&bitmap_lock);
	for (i = 0; i < sbi->asb_inode_map_blocks; i++) {
		bh = sbi->s_imap[i];
		j = minix_find_first_zero_bit(bh->b_data, bits_per_block);
		if (j < bits_per_block)
			break;
	}
	if (!bh || j >= bits_per_block) {
		spin_unlock(&bitmap_lock);
		iput(inode);
		return NULL;
	}
	if (minix_test_and_set_bit(j, bh->b_data)) {	/* shouldn't happen */
		spin_unlock(&bitmap_lock);
		printk("minix_new_inode: bit already set\n");
		iput(inode);
		return NULL;
	}
	spin_unlock(&bitmap_lock);
	mark_buffer_dirty(bh);
	j += i * bits_per_block;
	if (!j /*|| j > sbi->s_ninodes*/) {//todo: add s_ninodes to super block
		iput(inode);
		return NULL;
	}
	inode_init_owner(inode, dir, mode);
	inode->i_ino = j;
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	inode->i_blocks = 0;
	//memset(&minix_i(inode)->u, 0, sizeof(minix_i(inode)->u));
	insert_inode_hash(inode);
	mark_inode_dirty(inode);

	*error = 0;
	return inode;
}


struct aufs_disk_inode *
minix_V2_raw_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh)//vtodo: verify done: assimilate this
{
	int block;
	struct aufs_super_block *sbi = AUFS_SB(sb);
	struct aufs_disk_inode *p;
	int aufs_disk_inodes_per_block = sb->s_blocksize / sizeof(struct aufs_inode);

	*bh = NULL;
	if (!ino /*|| ino > sbi->s_ninodes*/) {//todo: add s_ninodes to super block
		printk("Bad inode number on dev %s: %ld is out of range\n",
		       sb->s_id, (long)ino);
		return NULL;
	}
	ino--;
	block = 1 + sbi->asb_inode_map_blocks + sbi->asb_zone_map_blocks +
		 ino / aufs_disk_inodes_per_block;//todo: propagate the offset formula to all the calculation macros and functions
	*bh = sb_bread(sb, block);
	if (!*bh) {
		printk("Unable to read inode block\n");
		return NULL;
	}
	p = (void *)(*bh)->b_data;
	return p + ino % aufs_disk_inodes_per_block;
}