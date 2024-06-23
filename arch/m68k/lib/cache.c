// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2002
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <cpu_func.h>
#include <asm/immap.h>
#include <asm/cache.h>

volatile int *cf_icache_status = (int *)ICACHE_STATUS;
volatile int *cf_dcache_status = (int *)DCACHE_STATUS;

void flush_cache(ulong start_addr, ulong size)
{
	/* Must be implemented for all M68k processors with copy-back data cache */
}

int icache_status(void)
{
	return *cf_icache_status;
}

int dcache_status(void)
{
	return *cf_dcache_status;
}

void icache_enable(void)
{
	icache_invalid();

	*cf_icache_status = 1;

#if defined(CONFIG_CF_V2) || defined(CONFIG_CF_V3) || defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%acr0"::"r"(CFG_SYS_CACHE_ACR0));
	__asm__ __volatile__("movec %0, %%acr1"::"r"(CFG_SYS_CACHE_ACR1));
#endif

#if defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%acr2"::"r"(CFG_SYS_CACHE_ACR2));
	__asm__ __volatile__("movec %0, %%acr3"::"r"(CFG_SYS_CACHE_ACR3));
#endif

#if defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%acr6"::"r"(CFG_SYS_CACHE_ACR6));
	__asm__ __volatile__("movec %0, %%acr7"::"r"(CFG_SYS_CACHE_ACR7));
#endif

#if defined(CONFIG_CF_V2) || defined(CONFIG_CF_V3) || defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%cacr"::"r"(CFG_SYS_CACHE_ICACR));
#endif

#if defined(CONFIG_MC68030)
	__asm__ __volatile__("movec %0, %%cacr"::"r"(M68K_CACR_EI |(*cf_dcache_status ? M68K_CACR_ED :0)));
#endif
}

void icache_disable(void)
{
	*cf_icache_status = 0;
	icache_invalid();

#if defined(CONFIG_CF_V2) || defined(CONFIG_CF_V3) || defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	u32 temp = 0;

	__asm__ __volatile__("movec %0, %%acr0"::"r"(temp));
	__asm__ __volatile__("movec %0, %%acr1"::"r"(temp));
#endif

#if defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%acr2"::"r"(temp));
	__asm__ __volatile__("movec %0, %%acr3"::"r"(temp));
#endif

#if defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%acr6"::"r"(temp));
	__asm__ __volatile__("movec %0, %%acr7"::"r"(temp));
#endif

#if defined(CONFIG_MC68030)
	__asm__ __volatile__("movec %0, %%cacr"::"r"((*cf_dcache_status ? M68K_CACR_ED : 0)));
#endif
}

void icache_invalid(void)
{
#if defined(CONFIG_CF_V2) || defined(CONFIG_CF_V3) || defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	u32 temp;

	temp = CFG_SYS_ICACHE_INV;
	if (*cf_icache_status)
		temp |= CFG_SYS_CACHE_ICACR;

	__asm__ __volatile__("movec %0, %%cacr"::"r"(temp));
#endif

#if defined(CONFIG_MC68030)
	__asm__ __volatile__("movec %0, %%cacr"::"r"((*cf_dcache_status ? M68K_CACR_ED : 0)| (*cf_icache_status ? M68K_CACR_EI : 0)| M68K_CACR_CI));
#endif
}

/*
 * data cache only for 68030, 68040, 68060 and ColdFire V4 such as MCF5445x
 * the dcache will be dummy in ColdFire V2 and V3
 */
void dcache_enable(void)
{
	dcache_invalid();
	*cf_dcache_status = 1;

#if defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%acr0"::"r"(CFG_SYS_CACHE_ACR0));
	__asm__ __volatile__("movec %0, %%acr1"::"r"(CFG_SYS_CACHE_ACR1));
#endif

#if defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%acr4"::"r"(CFG_SYS_CACHE_ACR4));
	__asm__ __volatile__("movec %0, %%acr5"::"r"(CFG_SYS_CACHE_ACR5));
#endif

#if defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%cacr"::"r"(CFG_SYS_CACHE_DCACR));
#endif

#if defined(CONFIG_MC68030)
	__asm__ __volatile__("movec %0, %%cacr"::"r"((*cf_icache_status ? M68K_CACR_EI : 0) | M68K_CACR_ED));
#endif
}

void dcache_disable(void)
{
	*cf_dcache_status = 0;
	dcache_invalid();

#if defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	u32 temp = 0;

	__asm__ __volatile__("movec %0, %%cacr"::"r"(temp));
	__asm__ __volatile__("movec %0, %%acr0"::"r"(temp));
	__asm__ __volatile__("movec %0, %%acr1"::"r"(temp));
#endif

#if defined(CFG_CF_V4E)
	__asm__ __volatile__("movec %0, %%acr4"::"r"(temp));
	__asm__ __volatile__("movec %0, %%acr5"::"r"(temp));
#endif

#if defined(CONFIG_MC68030)
	__asm__ __volatile__("movec %0, %%cacr"::"r"((*cf_icache_status ? M68K_CACR_EI : 0)));
#endif
}

void dcache_invalid(void)
{
#if defined(CONFIG_CF_V4) || defined(CFG_CF_V4E)
	u32 temp;

	temp = CFG_SYS_DCACHE_INV;
	if (*cf_dcache_status)
		temp |= CFG_SYS_CACHE_DCACR;
	if (*cf_icache_status)
		temp |= CFG_SYS_CACHE_ICACR;

	__asm__ __volatile__("movec %0, %%cacr"::"r"(temp));
#endif

#if defined(CONFIG_MC68030)
	__asm__ __volatile__("movec %0, %%cacr"::"r"((*cf_dcache_status ? M68K_CACR_ED : 0)| (*cf_icache_status ? M68K_CACR_EI : 0)| M68K_CACR_CD));
#endif
}

__weak void invalidate_dcache_range(unsigned long start, unsigned long stop)
{
	/* An empty stub, real implementation should be in platform code */
}
__weak void flush_dcache_range(unsigned long start, unsigned long stop)
{
	/* An empty stub, real implementation should be in platform code */
}
