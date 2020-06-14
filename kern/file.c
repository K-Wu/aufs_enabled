#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/types.h>
#include <linux/magic.h>
#include "aufs.h"

const struct file_operations aufs_file_ops = {
	.llseek = generic_file_llseek,
	.read_iter = generic_file_read_iter,
	.mmap = generic_file_mmap,
	.splice_read = generic_file_splice_read,
	//unsupported ops in the original source file as follows
	.write_iter = generic_file_write_iter,
	.fsync = generic_file_fsync,
	.splice_write = iter_file_splice_write};

int minix_setattr(struct dentry *dentry, struct iattr *attr)
{
	struct inode *inode = d_inode(dentry);
	int error;

	error = setattr_prepare(dentry, attr);
	if (error)
		return error;

	if ((attr->ia_valid & ATTR_SIZE) &&
		attr->ia_size != i_size_read(inode))
	{
		error = inode_newsize_ok(inode, attr->ia_size);
		if (error)
			return error;

		truncate_setsize(inode, attr->ia_size);
		//minix_truncate(inode);//todo: implement truncate
	}

	setattr_copy(inode, attr);
	mark_inode_dirty(inode);
	return 0;
}

const struct inode_operations minix_file_inode_operations = {
	.setattr = minix_setattr,
	.getattr = minix_getattr,
};