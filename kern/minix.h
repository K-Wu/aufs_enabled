#ifndef FS_MINIX_H
#define FS_MINIX_H
#if defined(CONFIG_MINIX_FS_NATIVE_ENDIAN) && \
	defined(CONFIG_MINIX_FS_BIG_ENDIAN_16BIT_INDEXED)

#error Minix file system byte order broken

#elif defined(CONFIG_MINIX_FS_NATIVE_ENDIAN)

/*
 * big-endian 32 or 64 bit indexed bitmaps on big-endian system or
 * little-endian bitmaps on little-endian system
 */

#define minix_test_and_set_bit(nr, addr)	\
	__test_and_set_bit((nr), (unsigned long *)(addr))
#define minix_set_bit(nr, addr)		\
	__set_bit((nr), (unsigned long *)(addr))
#define minix_test_and_clear_bit(nr, addr) \
	__test_and_clear_bit((nr), (unsigned long *)(addr))
#define minix_test_bit(nr, addr)		\
	test_bit((nr), (unsigned long *)(addr))
#define minix_find_first_zero_bit(addr, size) \
	find_first_zero_bit((unsigned long *)(addr), (size))

#elif defined(CONFIG_MINIX_FS_BIG_ENDIAN_16BIT_INDEXED)

/*
 * big-endian 16bit indexed bitmaps
 */

static inline int minix_find_first_zero_bit(const void *vaddr, unsigned size)
{
	const unsigned short *p = vaddr, *addr = vaddr;
	unsigned short num;

	if (!size)
		return 0;

	size >>= 4;
	while (*p++ == 0xffff) {
		if (--size == 0)
			return (p - addr) << 4;
	}

	num = *--p;
	return ((p - addr) << 4) + ffz(num);
}

#define minix_test_and_set_bit(nr, addr)	\
	__test_and_set_bit((nr) ^ 16, (unsigned long *)(addr))
#define minix_set_bit(nr, addr)	\
	__set_bit((nr) ^ 16, (unsigned long *)(addr))
#define minix_test_and_clear_bit(nr, addr)	\
	__test_and_clear_bit((nr) ^ 16, (unsigned long *)(addr))

static inline int minix_test_bit(int nr, const void *vaddr)
{
	const unsigned short *p = vaddr;
	return (p[nr >> 4] & (1U << (nr & 15))) != 0;
}

#else

/*
 * little-endian bitmaps
 */

#define minix_test_and_set_bit	__test_and_set_bit_le
#define minix_set_bit		__set_bit_le
#define minix_test_and_clear_bit	__test_and_clear_bit_le
#define minix_test_bit	test_bit_le
#define minix_find_first_zero_bit	find_first_zero_bit_le

#endif

#endif /* FS_MINIX_H */