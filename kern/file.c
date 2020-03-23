#include <linux/fs.h>

ssize_t kernel_read_wrapper(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return kernel_read(file, (void*) buf, count, pos);
}

const struct file_operations aufs_file_ops = {
	.llseek = generic_file_llseek,
	//.read = new_sync_read,
	.read = kernel_read_wrapper,
	.read_iter = generic_file_read_iter,
	.mmap = generic_file_mmap,
	.splice_read = generic_file_splice_read,
	//unsupported ops in the original source file as follows
	.write_iter	= generic_file_write_iter,
	.fsync		= generic_file_fsync,
	.splice_write = iter_file_splice_write
};

const struct inode_operations minix_file_inode_operations = {
	//.setattr	= minix_setattr,
	//.getattr	= minix_getattr,
};