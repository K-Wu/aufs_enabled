#include <linux/aio.h>
#include <linux/buffer_head.h>
#include <linux/mpage.h>
#include <linux/slab.h>
#include "minix.h"
#include "aufs.h"
#include <linux/writeback.h>

static int aufs_get_block(struct inode *inode, sector_t iblock,
			struct buffer_head *bh_result, int create)//todo: add size limit detection //todo: check whether need to maintain bitmap as required by generic VFS operations
{
	if (create){
		AUFS_INODE(inode)->ai_first_block = aufs_new_zone(inode);
	}
	map_bh(bh_result, inode->i_sb, iblock + AUFS_INODE(inode)->ai_first_block);
	return 0;
}


static void aufs_inode_fill(struct aufs_inode *ai,
			struct aufs_disk_inode const *di)//todo: aufs_inode_put
{
	ai->ai_first_block = be32_to_cpu(di->di_first);
	ai->ai_inode.i_mode = be32_to_cpu(di->di_mode);
	ai->ai_inode.i_size = be32_to_cpu(di->di_size);
	ai->ai_inode.i_blocks = be32_to_cpu(di->di_blocks);
	ai->ai_inode.i_ctime.tv_sec = be64_to_cpu(di->di_ctime);
	ai->ai_inode.i_mtime.tv_sec = ai->ai_inode.i_atime.tv_sec =
				ai->ai_inode.i_ctime.tv_sec;
	ai->ai_inode.i_mtime.tv_nsec = ai->ai_inode.i_atime.tv_nsec =
				ai->ai_inode.i_ctime.tv_nsec = 0;
	i_uid_write(&ai->ai_inode, (uid_t)be32_to_cpu(di->di_uid));
	i_gid_write(&ai->ai_inode, (gid_t)be32_to_cpu(di->di_gid));
}

static inline sector_t aufs_inode_block(struct aufs_super_block const *asb,
			ino_t inode_no)
{
	return (sector_t)(1+asb->asb_inode_map_blocks+asb->asb_zone_map_blocks + inode_no / asb->asb_inodes_in_block);
}

static size_t aufs_inode_offset(struct aufs_super_block const *asb,
			ino_t inode_no)
{
	return sizeof(struct aufs_disk_inode) *
				(inode_no % asb->asb_inodes_in_block);
}




/*
 * The minix V2 function to synchronize an inode.
 */
static struct buffer_head * V2_minix_update_inode(struct inode * inode)//vtodo: verify done: assimilate this
{
	struct buffer_head * bh;
	struct aufs_disk_inode * raw_inode;
	struct aufs_inode *aufs_inode = AUFS_INODE(inode);
	//int i;

	raw_inode = minix_V2_raw_inode(inode->i_sb, inode->i_ino, &bh);
	if (!raw_inode)
		return NULL;
	raw_inode->di_mode = inode->i_mode;
	raw_inode->di_uid = fs_high2lowuid(i_uid_read(inode));
	raw_inode->di_gid = fs_high2lowgid(i_gid_read(inode));
	//raw_inode->i_nlinks = inode->i_nlink; //todo: add aufs_disk_inode field support
	raw_inode->di_size = inode->i_size;
	raw_inode->di_blocks = (raw_inode->di_size+inode->i_sb->s_blocksize-1)/(inode->i_sb->s_blocksize);
	//raw_inode->i_mtime = inode->i_mtime.tv_sec; //todo: add aufs_disk_inode field support
	//raw_inode->i_atime = inode->i_atime.tv_sec; //todo: add aufs_disk_inode field support
	raw_inode->di_ctime = inode->i_ctime.tv_sec;
	raw_inode->di_first = aufs_inode->ai_first_block;
	//todo: need to update the first block pointer
	// if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
	// 	raw_inode->i_zone[0] = old_encode_dev(inode->i_rdev);
	// else for (i = 0; i < 10; i++)
	// 	raw_inode->i_zone[i] = minix_inode->u.i2_data[i];
	mark_buffer_dirty(bh);
	return bh;
}

int minix_write_inode(struct inode *inode, struct writeback_control *wbc)//vtodo: verify done: assimilate this
{
	int err = 0;
	struct buffer_head *bh;

	bh = V2_minix_update_inode(inode);
	if (!bh)
		return -EIO;
	if (wbc->sync_mode == WB_SYNC_ALL && buffer_dirty(bh)) {
		sync_dirty_buffer(bh);
		if (buffer_req(bh) && !buffer_uptodate(bh)) {
			printk("IO error syncing minix inode [%s:%08lx]\n",
				inode->i_sb->s_id, inode->i_ino);
			err = -EIO;
		}
	}
	brelse (bh);
	return err;
}


struct inode *aufs_inode_get(struct super_block *sb, ino_t no)
{
	struct aufs_super_block *asb = AUFS_SB(sb);
	struct buffer_head *bh;
	struct aufs_disk_inode *di;
	struct aufs_inode *ai;
	struct inode *inode;
	size_t block, offset;

	inode = iget_locked(sb, no);
	if (!inode)
		return ERR_PTR(-ENOMEM);

	if (!(inode->i_state & I_NEW))
		return inode;

	ai = AUFS_INODE(inode);
	block = aufs_inode_block(asb, no);
	offset = aufs_inode_offset(asb, no);

	pr_debug("aufs reads inode %lu from %lu block with offset %lu\n",
		(unsigned long)no, (unsigned long)block, (unsigned long)offset);

	bh = sb_bread(sb, block);
	if (!bh) {
		pr_err("cannot read block %lu\n", (unsigned long)block);
		goto read_error;
	}

	di = (struct aufs_disk_inode *)(bh->b_data + offset);
	aufs_inode_fill(ai, di);
	brelse(bh);

	inode->i_mapping->a_ops = &aufs_aops;
	if (S_ISREG(inode->i_mode)) {//vtodo: verifydone: mimic minix_set_inode
		inode->i_op = &minix_file_inode_operations;
		inode->i_fop = &aufs_file_ops;
		inode->i_mapping->a_ops = &aufs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &aufs_dir_inode_ops;
		inode->i_fop = &aufs_dir_ops;
		inode->i_mapping->a_ops = &aufs_aops;
	}
	else
		goto read_error;

	pr_debug("aufs inode %lu info:\n"
		"\tsize   = %lu\n"
		"\tfirst_block  = %lu\n"
		"\tblocks = %lu\n"
		"\tuid    = %lu\n"
		"\tgid    = %lu\n"
		"\tmode   = %lo\n",
				(unsigned long)inode->i_ino,
				(unsigned long)inode->i_size,
				(unsigned long)ai->ai_first_block,
				(unsigned long)inode->i_blocks,
				(unsigned long)i_uid_read(inode),
				(unsigned long)i_gid_read(inode),
				(unsigned long)inode->i_mode);

	unlock_new_inode(inode);

	return inode;

read_error:
	pr_err("aufs cannot read inode %lu\n", (unsigned long)no);
	iget_failed(inode);

	return ERR_PTR(-EIO);
}


int minix_prepare_chunk(struct page *page, loff_t pos, unsigned len)
{
	return __block_write_begin(page, pos, len, aufs_get_block);
}

int minix_set_inode(struct inode *inode, dev_t rdev)
{
	if (S_ISREG(inode->i_mode)) {
		inode->i_op = &minix_file_inode_operations;
		inode->i_fop = &aufs_file_ops;
		inode->i_mapping->a_ops = &aufs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &aufs_dir_inode_ops;
		inode->i_fop = &aufs_dir_ops;
		inode->i_mapping->a_ops = &aufs_aops;
	} else if (S_ISLNK(inode->i_mode)) {
		return -EINVAL;
		//inode->i_op = &minix_symlink_inode_operations;
		//inode_nohighmem(inode);
		//inode->i_mapping->a_ops = &aufs_aops;
	} else
		return -EINVAL;
		//init_special_inode(inode, inode->i_mode, rdev);
	return 0;
}



static int aufs_readpage(struct file *file, struct page *page)
{
	return block_read_full_page(page, aufs_get_block);
}

static int aufs_writepage(/*struct file *file, */struct page *page, struct writeback_control *wbc)
{
	return block_write_full_page(page, aufs_get_block, wbc);
}

static void minix_write_failed(struct address_space *mapping, loff_t to)
{
	struct inode *inode = mapping->host;

	if (to > inode->i_size) {
		truncate_pagecache(inode, inode->i_size);
		//minix_truncate(inode);
		//truncate(inode);//todo: implement truncate
	}
}

static int minix_write_begin(struct file *file, struct address_space *mapping,
			loff_t pos, unsigned len, unsigned flags,
			struct page **pagep, void **fsdata)
{
	int ret;

	ret = block_write_begin(mapping, pos, len, flags, pagep,
				aufs_get_block);
	if (unlikely(ret))
		minix_write_failed(mapping, pos + len);

	return ret;
}

// static int aufs_readpages(struct file *file, struct address_space *mapping,
// 			struct list_head *pages, unsigned nr_pages)
// {
// 	return mpage_readpages(mapping, pages, nr_pages, aufs_get_block);
// }

// static ssize_t aufs_direct_io(/*int rw,*/ struct kiocb *iocb,
// 			struct iov_iter *iter, loff_t off)
// {
// 	struct inode *inode = file_inode(iocb->ki_filp);

// 	return blockdev_direct_IO(rw, iocb, inode, iter, off, aufs_get_block);
// }

const struct address_space_operations aufs_aops = {
	.readpage = aufs_readpage,
	.writepage = aufs_writepage,
	.write_begin = minix_write_begin,
	.write_end = generic_write_end
	//.readpages = aufs_readpages,
	//.direct_IO = aufs_direct_io
};
