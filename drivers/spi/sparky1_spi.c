// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2021 Graeme Harker (graeme.harker@gmail.com)
 */

#include <common.h>
#include <clock_legacy.h>
#include <spi.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>
#include <asm/types.h>

#define SPARKY_SPI_CNTL         1
#define SPARKY_SPI_SS_ENABLE    1
#define SPARKY_SPI_SS_DISABLE   0

struct sparky1_spi_regs {
	u8 data;
    u8 cntrl;
};

static void sparky1_spi_ss_enable(struct udevice *dev)
{
	struct sparky1_spi_regs *spi = (struct sparky1_spi_regs *) dev_read_addr(dev_get_parent(dev));
    debug("%s: spi=%p\n", __func__, spi);
    
    while (readb(&spi->cntrl) & BIT(7))
        ;
	
    writeb(SPARKY_SPI_SS_ENABLE, &spi->cntrl);
}

static void sparky1_spi_ss_disable(struct udevice *dev)
{
	struct sparky1_spi_regs *spi = (struct sparky1_spi_regs *) dev_read_addr(dev_get_parent(dev));
    debug("%s: spi=%p\n", __func__, spi);

    while (readb(&spi->cntrl) & BIT(7))
        ;
	
	writeb(SPARKY_SPI_SS_DISABLE, &spi->cntrl);
}

static int sparky1_spi_xfer(struct udevice *dev, unsigned int bitlen,
		const void *dout, void *din, unsigned long flags)
{
	struct sparky1_spi_regs *spi = (struct sparky1_spi_regs *) dev_read_addr(dev_get_parent(dev));
    u8 *out = (u8 *)dout;
    u8 *in = (u8 *)din;

    debug("%s: spi=%p, bitlen=%d, %s, %s, data=", __func__, spi, bitlen,
        ((flags & SPI_XFER_BEGIN) ? "SPI_XFER_BEGIN" : ""), 
        ((flags & SPI_XFER_END) ? "SPI_XFER_END" : ""));

	if (flags & SPI_XFER_BEGIN)
        sparky1_spi_ss_enable(dev);

    for (uint i = bitlen / 8 ; i > 0 ; i--)
    {
	    writeb(*out, &spi->data);

        debug("%x", *out);

        while (readb(&spi->cntrl) & BIT(7))
            ;

        *in = readb(&spi->data);

        out++;
        in++;
    }

	if (flags & SPI_XFER_END)
		sparky1_spi_ss_disable(dev);

    debug("\n");

	return 0;
}


static int sparky1_spi_set_speed(struct udevice *bus, uint speed)
{
	struct sparky1_spi_regs *spi = (struct sparky1_spi_regs *) dev_read_addr(bus);
    debug("%s: spi=%p, speed=%d\n", __func__, spi, speed);
	return 0;
}

static int sparky1_spi_set_mode(struct udevice *bus, uint mode)
{
    struct sparky1_spi_regs *spi = (struct sparky1_spi_regs *) dev_read_addr(bus);
    debug("%s: spi=%p, mode=%d\n", __func__, spi, mode);
	return 0;
}

static int sparky1_spi_probe(struct udevice *bus)
{
	/* Init SPI Hardware, disable remap, set clock */

	struct sparky1_spi_regs *spi = (struct sparky1_spi_regs *) dev_read_addr(bus);
    debug("%s: spi=%p\n", __func__, spi);
	return 0;
}

static int sparky1_cs_info(struct udevice *bus, uint cs,
			   struct spi_cs_info *info)
{
    struct sparky1_spi_regs *spi = (struct sparky1_spi_regs *) dev_read_addr(bus);
    debug("%s: spi=%p, cs=%d\n", __func__, spi, cs);

	/* Always allow activity on unit 0 */

	if (cs > 0)
		return -EINVAL;

	return 0;
}

static const struct dm_spi_ops sparky1_spi_ops = {
	.xfer       = sparky1_spi_xfer,
	.set_speed  = sparky1_spi_set_speed,
	.set_mode   = sparky1_spi_set_mode,
	.cs_info    = sparky1_cs_info,
};

static const struct udevice_id sparky1_spi_ids[] = {
	{ .compatible = "sparky,spi" },
	{}
};

U_BOOT_DRIVER(sparky1_spi) = {
	.name   = "sparky1_spi",
	.id = UCLASS_SPI,
	.of_match = sparky1_spi_ids,
	.ops    = &sparky1_spi_ops,
	.probe  = sparky1_spi_probe,
};
