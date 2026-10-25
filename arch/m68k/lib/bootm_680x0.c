// SPDX-License-Identifier: GPL-2.0+
/*
 * Booting Linux on classic (non-ColdFire) m68k
 *
 * A classic m68k kernel takes no arguments. It finds a chain of struct
 * bi_record, the bootinfo, immediately after its own image at _end, and
 * arch/m68k/kernel/head.S reads the machine, CPU, FPU and MMU types and
 * the memory layout from it before anything else.
 *
 * vmlinux-std.lds puts the BSS before the init sections, so a binary
 * made with objcopy -O binary holds everything up to the end of the init
 * data, BSS included as zeros. Only the page alignment that ends the last
 * section is missing from the file, so _end is the end of the image bootm
 * has just loaded, rounded up to a page. A binary already padded to _end
 * gives the same answer.
 */

#include <bootm.h>
#include <bootstage.h>
#include <command.h>
#include <cpu_func.h>
#include <env.h>
#include <image.h>
#include <stdio.h>
#include <asm/bootinfo.h>
#include <asm/global_data.h>
#include <linux/kernel.h>
#include <linux/string.h>

DECLARE_GLOBAL_DATA_PTR;

/* The kernel's COMMAND_LINE_SIZE */
#define M68K_CL_SIZE	256

/* The kernel's PAGE_SIZE on everything but Sun-3 */
#define M68K_PAGE_SIZE	4096

__weak void board_bootinfo_types(struct m68k_bootinfo_types *t)
{
	t->machtype = 0;
	t->cputype = CPU_68030;
	t->fputype = 0;		/* no internal FPU */
	t->mmutype = MMU_68030;
}

static void *bi_add(void *p, u16 tag, const void *data, u16 len)
{
	struct bi_record *r = p;
	u16 size = ALIGN(sizeof(*r) + len, 4);

	r->tag = tag;
	r->size = size;
	if (len)
		memcpy(r->data, data, len);
	memset((char *)r->data + len, 0, size - sizeof(*r) - len);

	return p + size;
}

static void *bi_add_ulong(void *p, u16 tag, ulong val)
{
	return bi_add(p, tag, &val, sizeof(val));
}

static int bootinfo_setup(struct bootm_headers *images)
{
	struct m68k_bootinfo_types t;
	const char *cmdline;
	ulong start, end;
	void *bi;
	int i;

	if (*(u32 *)(images->os.load + 2) != BOOTINFOV_MAGIC) {
		printf("No bootinfo version table at 0x%08lx: not a classic m68k kernel\n",
		       images->os.load + 2);
		return -EINVAL;
	}

	board_bootinfo_types(&t);
	if (!t.machtype) {
		puts("No m68k machine type for the bootinfo\n");
		return -EINVAL;
	}

	start = ALIGN(images->os.load + images->os.image_len, M68K_PAGE_SIZE);
	bi = (void *)start;

	bi = bi_add_ulong(bi, BI_MACHTYPE, t.machtype);
	bi = bi_add_ulong(bi, BI_CPUTYPE, t.cputype);
	bi = bi_add_ulong(bi, BI_FPUTYPE, t.fputype);
	bi = bi_add_ulong(bi, BI_MMUTYPE, t.mmutype);

	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		struct mem_info m = {
			.addr = gd->dram[i].start,
			.size = gd->dram[i].size,
		};

		if (m.size)
			bi = bi_add(bi, BI_MEMCHUNK, &m, sizeof(m));
	}

	if (images->rd_start) {
		struct mem_info rd = {
			.addr = images->rd_start,
			.size = images->rd_end - images->rd_start,
		};

		bi = bi_add(bi, BI_RAMDISK, &rd, sizeof(rd));
	}

	cmdline = env_get("bootargs");
	if (cmdline) {
		if (strlen(cmdline) >= M68K_CL_SIZE)
			printf("bootargs longer than %d characters: the kernel will truncate them\n",
			       M68K_CL_SIZE - 1);
		bi = bi_add(bi, BI_COMMAND_LINE, cmdline, strlen(cmdline) + 1);
	}

	bi = bi_add(bi, BI_LAST, NULL, 0);
	end = (ulong)bi;

	if (images->rd_start && start < images->rd_end && end > images->rd_start) {
		printf("Bootinfo 0x%08lx-0x%08lx overlaps the ramdisk\n", start, end);
		return -EINVAL;
	}

	flush_cache(start, end - start);
	printf("   Bootinfo at %08lx, %lu bytes\n", start, end - start);

	return 0;
}

int do_bootm_linux(int flag, struct bootm_info *bmi)
{
	struct bootm_headers *images = bmi->images;
	void (*kernel)(void);

	/*
	 * Build the bootinfo while a failure can still return to the
	 * prompt: by OS_GO, arch_preboot_os() has masked interrupts and
	 * turned the caches off.
	 */
	if (flag & BOOTM_STATE_OS_PREP)
		return bootinfo_setup(images) ? 1 : 0;

	if (flag != 0 && flag != BOOTM_STATE_OS_GO)
		return 1;

	/* flag 0 asks for everything at once */
	if (!flag && bootinfo_setup(images))
		return 1;

	kernel = (void (*)(void))images->ep;

	debug("## Transferring control to Linux (at address %08lx) ...\n",
	      (ulong)kernel);

	bootstage_mark(BOOTSTAGE_ID_RUN_OS);

	bootm_final(0);

	/* head.S sets up its own stack and takes no arguments */
	kernel();
	/* does not return */

	return 1;
}
