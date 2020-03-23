#include "aufs.h"
static int add_nondir(struct dentry *dentry, struct inode *inode)
{
	int err = minix_add_link(dentry, inode);
	if (!err) {
		d_instantiate(dentry, inode);
		return 0;
	}
	inode_dec_link_count(inode);
	iput(inode);
	return err;
}
int minix_mknod(struct inode * dir, struct dentry *dentry, umode_t mode, dev_t rdev)
{
	int error;
	struct inode *inode;

	if (!old_valid_dev(rdev))
		return -EINVAL;

	//inode = minix_new_inode(dir, mode, &error);
	inode=aufs_new_inode(dir, mode, &error);//vtodo: verify done: find empty inode

	if (inode) {
		int set_inode_ret=minix_set_inode(inode, rdev);
		if (set_inode_ret){
			return set_inode_ret;
		}
		mark_inode_dirty(inode);
		error = add_nondir(dentry, inode);
	}
	return error;
}

//todo: add mkdir

int minix_create(struct inode *dir, struct dentry *dentry, umode_t mode,
		bool excl)
{
	return minix_mknod(dir, dentry, mode, 0);
}