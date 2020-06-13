#include "aufs.h"
static int add_nondir(struct dentry *dentry, struct inode *inode)
{
	int err = minix_add_link(dentry, inode);
	if (!err)
	{
		d_instantiate(dentry, inode);
		return 0;
	}
	inode_dec_link_count(inode);
	iput(inode);
	return err;
}

int minix_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) //todo: assimilate in terms of func name
{
	printk("minix_mkdir \n");
	struct inode *inode;
	int err;

	inode_inc_link_count(dir);

	inode = aufs_new_inode(dir, S_IFDIR | mode, &err);
	if (!inode)
		goto out_dir;

	minix_set_inode(inode, 0);

	inode_inc_link_count(inode);

	err = minix_make_empty(inode, dir);
	if (err)
		goto out_fail;

	err = minix_add_link(dentry, inode);
	if (err)
		goto out_fail;

	d_instantiate(dentry, inode);
out:
	return err;

out_fail:
	inode_dec_link_count(inode);
	inode_dec_link_count(inode);
	iput(inode);
out_dir:
	inode_dec_link_count(dir);
	goto out;
}

int minix_tmpfile(struct inode *dir, struct dentry *dentry, umode_t mode)
{
	int error;
	struct inode *inode = aufs_new_inode(dir, mode, &error);
	if (inode)
	{
		minix_set_inode(inode, 0);
		mark_inode_dirty(inode);
		d_tmpfile(dentry, inode);
	}
	return error;
}

struct dentry *minix_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags)
{
	struct inode *inode = NULL;
	ino_t ino;

	if (dentry->d_name.len > AUFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	ino = minix_inode_by_name(dentry);
	if (ino)
		inode = aufs_inode_get(dir->i_sb, ino);
	return d_splice_alias(inode, dentry);
}

int minix_mknod(struct inode *dir, struct dentry *dentry, umode_t mode, dev_t rdev)
{
	int error;
	struct inode *inode;

	if (!old_valid_dev(rdev))
		return -EINVAL;

	inode = aufs_new_inode(dir, mode, &error);
	printk("minix_mknod inode %lu\n", inode->i_ino);
	if (inode)
	{
		int set_inode_ret = minix_set_inode(inode, rdev);
		if (set_inode_ret)
		{
			return set_inode_ret;
		}
		mark_inode_dirty(inode);
		error = add_nondir(dentry, inode);
	}
	return error;
}


int minix_create(struct inode *dir, struct dentry *dentry, umode_t mode,
				 bool excl)
{
	return minix_mknod(dir, dentry, mode, 0);
}