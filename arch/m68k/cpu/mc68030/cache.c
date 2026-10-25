// SPDX-License-Identifier: GPL-2.0+
/*
 * MC68030 on-chip cache control
 *
 * The 68030 has 256-byte instruction and data caches, both controlled
 * through CACR alone. The data cache is write-through, so there is
 * never anything to write back.
 */

#include <cpu_func.h>
#include <asm/cache.h>
#include <linux/errno.h>

static u32 get_cacr(void)
{
	u32 cacr;

	__asm__ __volatile__("movec %%cacr, %0" : "=r"(cacr));
	return cacr;
}

static void set_cacr(u32 cacr)
{
	__asm__ __volatile__("movec %0, %%cacr" :: "r"(cacr));
}

void flush_cache(ulong start_addr, ulong size)
{
	/* write-through data cache: nothing to flush */
}

int icache_status(void)
{
	return (get_cacr() & M68K_CACR_EI) ? 1 : 0;
}

int dcache_status(void)
{
	return (get_cacr() & M68K_CACR_ED) ? 1 : 0;
}

void icache_enable(void)
{
	invalidate_icache_all();
	set_cacr(get_cacr() | M68K_CACR_EI);
}

void icache_disable(void)
{
	invalidate_icache_all();
	set_cacr(get_cacr() & ~M68K_CACR_EI);
}

void invalidate_icache_all(void)
{
	set_cacr(get_cacr() | M68K_CACR_CI);
}

void dcache_enable(void)
{
	dcache_invalid();
	set_cacr(get_cacr() | M68K_CACR_ED);
}

void dcache_disable(void)
{
	dcache_invalid();
	set_cacr(get_cacr() & ~M68K_CACR_ED);
}

void dcache_invalid(void)
{
	set_cacr(get_cacr() | M68K_CACR_CD);
}

__weak void flush_dcache_all(void)
{
	flush_dcache_range(0, ~0);
}

__weak void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
}

__weak void flush_dcache_range(unsigned long start, unsigned long stop)
{
}

int __weak pgprot_set_attrs(phys_addr_t addr, size_t size, enum pgprot_attrs perm)
{
	return -ENOSYS;
}
