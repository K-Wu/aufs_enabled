#include "aufs.h"
#include "minix.h"
#include <linux/buffer_head.h>
#include <linux/bitops.h>
#include <linux/sched.h>

static DEFINE_SPINLOCK(bitmap_lock);

inline unsigned long integer_round_up_division(unsigned long dividend, unsigned long divisor)
{
	return (dividend+divisor-1)/divisor;
}

int aufs_new_zone(struct inode *inode) //return the block id of the new zone
{									   //todo: originally need to memset 0 after new block (see itree_common.c. alloc_branch()). move that to block free
	struct aufs_super_block *sbi = AUFS_SB(inode->i_sb);
	int bits_per_block = 8 * inode->i_sb->s_blocksize;
	int i;

	for (i = 0; i < sbi->asb_zone_map_blocks; i++)
	{
		struct buffer_head *bh = sbi->s_zmap[i];
		int j;

		spin_lock(&bitmap_lock);
		j = minix_find_first_zero_bit(bh->b_data, bits_per_block);
		if (j < bits_per_block)
		{
			minix_set_bit(j, bh->b_data);
			spin_unlock(&bitmap_lock);
			mark_buffer_dirty(bh);
			j += i * bits_per_block;//todo: propagate block size per zone map bit //todo: add bitmap to disk
			if (j < 0 /*|| j >= sbi->s_nzones*/) //todo: add s_nzones to super block //todo: add bitmap to disk
				break;

			printk("aufs_new_zone block successfully gained id %d. (1 + sbi->asb_inode_map_blocks + sbi->asb_zone_map_blocks + sbi->asb_inode_blocks) %ld\n", j, (1 + sbi->asb_inode_map_blocks + sbi->asb_zone_map_blocks + sbi->asb_inode_blocks));
			return j* (sbi->asb_blocks_per_zone)/*converting zone index to block index*/+(integer_round_up_division(1 + sbi->asb_inode_map_blocks + sbi->asb_zone_map_blocks + sbi->asb_inode_blocks + 1* (sbi->asb_blocks_per_zone),sbi->asb_alignment_num_blocks)*sbi->asb_alignment_num_blocks);//bitmap numbering starts from the data block region (root inode 1 returns 1+68, etc.)
		}
		spin_unlock(&bitmap_lock);
	}
	return 0;
}

struct inode *aufs_new_inode(const struct inode *dir, umode_t mode, int *error) //adapt from minix_new_inode
{
	struct super_block *sb = dir->i_sb;
	struct aufs_super_block *sbi = AUFS_SB(sb);
	struct inode *inode = new_inode(sb);
	struct buffer_head *bh;
	int bits_per_block = 8 * sb->s_blocksize;
	unsigned long j;
	int i;

	if (!inode)
	{
		*error = -ENOMEM;
		printk("aufs_new_inode fails !inode\n");
		return NULL;
	}
	j = bits_per_block;
	bh = NULL;
	*error = -ENOSPC;
	spin_lock(&bitmap_lock);
	for (i = 0; i < sbi->asb_inode_map_blocks; i++)
	{
		bh = sbi->s_imap[i];
		j = minix_find_first_zero_bit(bh->b_data, bits_per_block);
		if (j < bits_per_block)
		{
			break;
		}
	}
	if (!bh || j >= bits_per_block)
	{
		spin_unlock(&bitmap_lock);
		iput(inode);
		printk("aufs_new_inode fails !bh\n");
		return NULL;
	}
	if (minix_test_and_set_bit(j, bh->b_data))
	{ /* shouldn't happen */
		spin_unlock(&bitmap_lock);
		printk("minix_new_inode: bit already set\n");
		iput(inode);
		return NULL;
	}
	spin_unlock(&bitmap_lock);
	mark_buffer_dirty(bh);
	j += i * bits_per_block;
	if (!j /*|| j > sbi->s_ninodes*/)
	{ //todo: add s_ninodes to super block
		iput(inode);
		printk("aufs_new_inode fails !j\n");
		return NULL;
	}
	inode_init_owner(inode, dir, mode);
	inode->i_ino = j;
	inode->i_mtime = inode->i_atime = inode->i_ctime = current_time(inode);
	inode->i_blocks = 0;
	#ifdef MULTI_BLOCK_PTR_SCHEME
	memset(AUFS_INODE(inode)->ai_zone_ptr, 0, ZONE_PTR_IN_INODE_NUM*sizeof(AUFS_INODE(inode)->ai_zone_ptr[0]));
	#else
	memset(&(AUFS_INODE(inode)->ai_zone_ptr), 0, sizeof(AUFS_INODE(inode)->ai_zone_ptr));
	#endif
	insert_inode_hash(inode);
	mark_inode_dirty(inode);
	printk("aufs_new_inode %lu info:\n"
		   "\tsize   = %lu\n"
		   "\tblocks = %lu\n"
		   "\tuid    = %lu\n"
		   "\tgid    = %lu\n"
		   "\tmode   = %lo\n",
		   (unsigned long)inode->i_ino,
		   (unsigned long)inode->i_size,
		   (unsigned long)inode->i_blocks,
		   (unsigned long)i_uid_read(inode),
		   (unsigned long)i_gid_read(inode),
		   (unsigned long)inode->i_mode);
	*error = 0;
	return inode;
}

struct aufs_disk_inode *
minix_V2_raw_inode(struct super_block *sb, ino_t ino, struct buffer_head **bh)
{
	int block;
	struct aufs_super_block *sbi = AUFS_SB(sb);\
	int offset;
	*bh = NULL;
	if (!ino /*|| ino > sbi->s_ninodes*/)
	{ //todo: add s_ninodes to super block
		printk("Bad inode number on dev %s: %ld is out of range\n",
			   sb->s_id, (long)ino);
		return NULL;
	}
	block = aufs_inode_block(sbi, ino);
	offset = aufs_inode_offset(sbi, ino); //todo: propagate the offset formula to all the calculation macros and functions
	printk("minix_V2_raw_inode ino %lu block %d offset %d\n", ino, block, offset);
	*bh = sb_bread(sb, block);
	if (!*bh)
	{
		printk("Unable to read inode block\n");
		return NULL;
	}
	return (struct aufs_disk_inode *)((*bh)->b_data + offset);
	;
}