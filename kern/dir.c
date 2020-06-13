#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/highmem.h>
#include <linux/swap.h>

#include "aufs.h"

static size_t aufs_dir_offset(size_t idx)
{
	return idx * sizeof(struct aufs_disk_dir_entry);
}

static size_t aufs_dir_pages(struct inode *inode)
{
	size_t size = aufs_dir_offset(inode->i_size);

	return (size + PAGE_SIZE - 1) >> PAGE_SHIFT;
}

static size_t aufs_dir_entry_page(size_t idx)
{
	return aufs_dir_offset(idx) >> PAGE_SHIFT;
}

static inline size_t aufs_dir_entry_offset(size_t idx)
{
	return aufs_dir_offset(idx) -
		   (aufs_dir_entry_page(idx) << PAGE_SHIFT);
}

static struct page *aufs_get_page(struct inode *inode, size_t n)
{
	struct address_space *mapping = inode->i_mapping;
	struct page *page = read_mapping_page(mapping, n, NULL);

	if (!IS_ERR(page))
		kmap(page);
	return page;
}

static void aufs_put_page(struct page *page)
{
	kunmap(page);
	put_page(page);
}

static int aufs_dir_emit(struct dir_context *ctx,
						 struct aufs_disk_dir_entry *de)
{
	unsigned type = DT_UNKNOWN;
	unsigned len = strlen(de->dde_name);
	size_t ino = be32_to_cpu(de->dde_inode);

	return dir_emit(ctx, de->dde_name, len, ino, type);
}

static inline void *minix_next_entry(void *de) //todo: assimilate in terms of func name
{
	return (void *)((char *)de + AUFS_DIR_SIZE);
}

static int dir_commit_chunk(struct page *page, loff_t pos, unsigned len) //todo: assimilate in terms of func name
{
	struct address_space *mapping = page->mapping;
	struct inode *dir = mapping->host;
	int err = 0;
	block_write_end(NULL, mapping, pos, len, len, page, NULL);

	if (pos + len > dir->i_size)
	{
		i_size_write(dir, pos + len);
		mark_inode_dirty(dir);
	}
	if (IS_DIRSYNC(dir))
		err = write_one_page(page);
	else
		unlock_page(page);
	return err;
}

/*
 * Return the offset into page `page_nr' of the last valid
 * byte in that page, plus one.
 */
static unsigned
minix_last_byte(struct inode *inode, unsigned long page_nr) //todo: assimilate in terms of func name
{
	unsigned last_byte = PAGE_SIZE;

	if (page_nr == (inode->i_size >> PAGE_SHIFT))
		last_byte = inode->i_size & (PAGE_SIZE - 1);
	return last_byte;
}

static inline int namecompare(int len, int maxlen,
							  const char *name, const char *buffer)
{
	if (len < maxlen && buffer[len])
		return 0;
	return !memcmp(name, buffer, len);
}

static struct page *dir_get_page(struct inode *dir, unsigned long n) //todo: assimilate in terms of func name
{
	struct address_space *mapping = dir->i_mapping;
	struct page *page = read_mapping_page(mapping, n, NULL);
	if (!IS_ERR(page))
		kmap(page);
	return page;
}

/*
 *	minix_find_entry()
 *
 * finds an entry in the specified directory with the wanted name. It
 * returns the cache buffer in which the entry was found, and the entry
 * itself (as a parameter - res_dir). It does NOT read the inode of the
 * entry - you'll have to do that yourself if you want to.
 */
struct aufs_disk_dir_entry *minix_find_entry(struct dentry *dentry, struct page **res_page)
{
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct inode *dir = d_inode(dentry->d_parent);
	struct super_block *sb = dir->i_sb;
	struct aufs_super_block *sbi = AUFS_SB(sb);
	unsigned long n;
	unsigned long npages = dir_pages(dir);
	struct page *page = NULL;
	char *p;
	struct aufs_disk_dir_entry *de3;
	char *namx;
	__u32 inumber;
	*res_page = NULL;

	for (n = 0; n < npages; n++)
	{
		char *kaddr, *limit;

		page = dir_get_page(dir, n);
		if (IS_ERR(page))
			continue;

		kaddr = (char *)page_address(page);
		limit = kaddr + minix_last_byte(dir, n) - AUFS_DIR_SIZE;
		for (p = kaddr; p <= limit; p = minix_next_entry(p))
		{

			de3 = (struct aufs_disk_dir_entry *)p;
			namx = de3->dde_name;
			inumber = de3->dde_inode;
			if (!inumber)
				continue;
			if (namecompare(namelen, AUFS_NAME_LEN, name, namx))
				goto found;
		}
		aufs_put_page(page);
	}
	return NULL;

found:
	*res_page = page;
	return (struct aufs_disk_dir_entry *)p;
}

int minix_add_link(struct dentry *dentry, struct inode *inode) //todo: assimilate in terms of func name
{
	struct inode *dir = d_inode(dentry->d_parent);
	const char *name = dentry->d_name.name;
	int namelen = dentry->d_name.len;
	struct page *page = NULL;
	unsigned long npages = dir_pages(dir);
	unsigned long n;
	char *kaddr, *p;
	struct aufs_disk_dir_entry *de;
	loff_t pos;
	int err;
	char *namx = NULL;
	__u32 inumber;

	/*
	 * We take care of directory expansion in the same loop
	 * This code plays outside i_size, so it locks the page
	 * to protect that region.
	 */
	for (n = 0; n <= npages; n++)
	{
		char *limit, *dir_end;

		page = dir_get_page(dir, n);
		err = PTR_ERR(page);
		if (IS_ERR(page))
			goto out;
		lock_page(page);
		kaddr = (char *)page_address(page);
		dir_end = kaddr + minix_last_byte(dir, n);
		limit = kaddr + PAGE_SIZE - AUFS_DIR_SIZE;
		for (p = kaddr; p <= limit; p = minix_next_entry(p))
		{
			de = (struct aufs_disk_dir_entry *)p;
			namx = de->dde_name;
			inumber = de->dde_inode;
			if (p == dir_end)
			{
				/* We hit i_size */
				de->dde_inode = 0;
				goto got_it;
			}
			if (!inumber)
				goto got_it;
			err = -EEXIST;
			if (namecompare(namelen, AUFS_NAME_LEN, name, namx))
				goto out_unlock;
		}
		unlock_page(page);
		aufs_put_page(page);
	}
	BUG();
	return -EINVAL;

got_it:
	pos = page_offset(page) + p - (char *)page_address(page);
	err = minix_prepare_chunk(page, pos, AUFS_DIR_SIZE);
	if (err)
		goto out_unlock;
	memcpy(namx, name, namelen);
	memset(namx + namelen, 0, AUFS_DIR_SIZE - namelen - 4);
	de->dde_inode = inode->i_ino;
	err = dir_commit_chunk(page, pos, AUFS_DIR_SIZE);
	dir->i_mtime = dir->i_ctime = current_time(dir);
	mark_inode_dirty(dir);
out_put:
	aufs_put_page(page);
out:
	return err;
out_unlock:
	unlock_page(page);
	goto out_put;
}

int minix_make_empty(struct inode *inode, struct inode *dir) //todo: assimilate in terms of func name
{
	struct page *page = grab_cache_page(inode->i_mapping, 0);
	char *kaddr;
	int err;
	struct aufs_disk_dir_entry *de3;

	if (!page)
		return -ENOMEM;
	err = minix_prepare_chunk(page, 0, 2 * AUFS_DIR_SIZE);
	if (err)
	{
		unlock_page(page);
		goto fail;
	}

	kaddr = kmap_atomic(page);
	memset(kaddr, 0, PAGE_SIZE);

	de3 = (struct aufs_disk_dir_entry *)kaddr;

	de3->dde_inode = inode->i_ino;
	strcpy(de3->dde_name, ".");
	de3 = minix_next_entry(de3);
	de3->dde_inode = dir->i_ino;
	strcpy(de3->dde_name, "..");

	kunmap_atomic(kaddr);

	err = dir_commit_chunk(page, 0, 2 * AUFS_DIR_SIZE);
fail:
	put_page(page);
	return err;
}

static int aufs_iterate(struct inode *inode, struct dir_context *ctx)
{
	size_t pages = aufs_dir_pages(inode);
	size_t pidx = aufs_dir_entry_page(ctx->pos);
	size_t off = aufs_dir_entry_offset(ctx->pos);

	for (; pidx < pages; ++pidx, off = 0)
	{
		struct page *page = aufs_get_page(inode, pidx);
		struct aufs_disk_dir_entry *de;
		char *kaddr;

		if (IS_ERR(page))
		{
			pr_err("cannot access page %lu in %lu", pidx,
				   (unsigned long)inode->i_ino);
			return PTR_ERR(page);
		}

		kaddr = page_address(page);
		de = (struct aufs_disk_dir_entry *)(kaddr + off);
		while (off < PAGE_SIZE && ctx->pos < inode->i_size)
		{
			if (!aufs_dir_emit(ctx, de))
			{
				aufs_put_page(page);
				return 0;
			}
			++ctx->pos;
			++de;
		}
		aufs_put_page(page);
	}
	return 0;
}

static int minix_readdir(struct file *file, struct dir_context *ctx)
{
	struct inode *inode = file_inode(file);
	struct super_block *sb = inode->i_sb;
	unsigned long npages = dir_pages(inode);
	unsigned long pos = ctx->pos;
	unsigned offset;
	unsigned long n;
	struct aufs_disk_dir_entry *de3;
	ctx->pos = pos = ALIGN(pos, AUFS_DIR_SIZE);
	if (pos >= inode->i_size)
		return 0;

	offset = pos & ~PAGE_MASK;
	n = pos >> PAGE_SHIFT;

	for (; n < npages; n++, offset = 0)
	{
		char *p, *kaddr, *limit;
		struct page *page = dir_get_page(inode, n);

		if (IS_ERR(page))
			continue;
		kaddr = (char *)page_address(page);
		p = kaddr + offset;
		limit = kaddr + minix_last_byte(inode, n) - AUFS_DIR_SIZE;
		for (; p <= limit; p = minix_next_entry(p))
		{
			const char *name;
			__u32 inumber;
			de3 = (struct aufs_disk_dir_entry *)p;
			name = de3->dde_name;
			inumber = de3->dde_inode;

			if (inumber)
			{
				unsigned l = strnlen(name, AUFS_NAME_LEN);
				if (!dir_emit(ctx, name, l,
							  inumber, DT_UNKNOWN))
				{
					aufs_put_page(page);
					return 0;
				}
			}
			ctx->pos += AUFS_DIR_SIZE;
		}
		aufs_put_page(page);
	}
	return 0;
}

static int aufs_readdir(struct file *file, struct dir_context *ctx)
{
	return aufs_iterate(file_inode(file), ctx);
}

const struct file_operations aufs_dir_ops = {
	.llseek = generic_file_llseek,
	.read = generic_read_dir,
	.iterate_shared = minix_readdir,
	.fsync = generic_file_fsync,
};

struct aufs_filename_match
{
	struct dir_context ctx;
	ino_t ino;
	const char *name;
	int len;
};

ino_t minix_inode_by_name(struct dentry *dentry)
{
	struct page *page;
	struct aufs_disk_dir_entry *de = minix_find_entry(dentry, &page);
	ino_t res = 0;

	if (de)
	{
		struct address_space *mapping = page->mapping;
		struct inode *inode = mapping->host;

		res = ((struct aufs_disk_dir_entry *)de)->dde_inode; //todo: propagate: covert according to endian

		aufs_put_page(page);
	}
	return res;
}

static int aufs_match(struct dir_context *ctx, const char *name, int len,
					  loff_t off, u64 ino, unsigned type)
{
	struct aufs_filename_match *match = (struct aufs_filename_match *)ctx;

	if (len != match->len)
		return 0;

	if (memcmp(match->name, name, len) == 0)
	{
		match->ino = ino;
		return 1;
	}
	return 0;
}

static ino_t aufs_inode_by_name(struct inode *dir, struct qstr *child)
{
	struct aufs_filename_match match = {
		{&aufs_match, 0}, 0, child->name, child->len};

	int err = aufs_iterate(dir, &match.ctx);

	if (err)
		pr_err("Cannot find dir entry, error = %d", err);
	return match.ino;
}

static struct dentry *aufs_lookup(struct inode *dir, struct dentry *dentry,
								  unsigned flags) //todo: assimilate in terms of func name minix_lookup
{
	struct inode *inode = NULL;
	ino_t ino;

	if (dentry->d_name.len >= AUFS_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);

	ino = aufs_inode_by_name(dir, &dentry->d_name);
	if (ino)
	{
		inode = aufs_inode_get(dir->i_sb, ino);
		if (IS_ERR(inode))
		{
			pr_err("Cannot read inode %lu", (unsigned long)ino);
			return ERR_PTR(PTR_ERR(inode));
		}
		d_add(dentry, inode);
	}
	return NULL;
}

const struct inode_operations aufs_dir_inode_ops = {
	.lookup = minix_lookup,
	.create = minix_create,
	.mknod = minix_mknod,
	.getattr = minix_getattr,
	.mkdir = minix_mkdir,
	.tmpfile = minix_tmpfile,
};

//tutorial: clear inode: see minix_clear_inode. Manipulate the region pointed by buffer_head->b_data and mark that buffer_head as dirty. One may use ordinary pointer dereference to modify.