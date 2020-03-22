#include <linux/fs.h>

ssize_t kernel_read_wrapper(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	return kernel_read(file, (void*) buf, count, pos);
}

const struct file_operations aufs_file_ops = {
	.llseek = generic_file_llseek,
	.read = kernel_read_wrapper,
	//.read	= generic_file_read,
	.read_iter = generic_file_read_iter,
	.mmap = generic_file_mmap,
	.splice_read = generic_file_splice_read,
	//unsupported ops in the original source file as follows,
    .write_iter	= generic_file_write_iter,
    .splice_read	= generic_file_splice_read,
    .splice_write	= iter_file_splice_write,
};
