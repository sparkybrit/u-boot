.. SPDX-License-Identifier: GPL-2.0+
.. Copyright (C) 2025, Graeme Harker <graeme.harker@gmail.com>

Sparky1 SBC
===========

The Sparky1 is a custom single-board computer built around the Motorola MC68030
CPU. Unlike the ColdFire (MCF) targets already supported in U-Boot's m68k
architecture, the Sparky1 uses the classic full 68030 ISA with an external
68030 MMU and separate instruction and data caches.

Hardware
--------

* CPU: Motorola MC68030 at 3.6864 MHz
* DUART: Exar XR68C681 at 0x80000000 (serial console, counter/timer, GPIO)
* Flash: mapped at 0x000000 (reset vector base)
* SDRAM: mapped at 0x100000

Building U-Boot
---------------

A classic m68k cross-compiler is required. ColdFire-only toolchains will not
work as they do not support the full 68030 instruction set.

.. code-block:: bash

    export CROSS_COMPILE=m68k-linux-gnu-
    make sparky1_defconfig
    make

The build produces ``u-boot.bin`` for programming to flash at address 0x000400.

Hardware Support
----------------

The following peripherals are supported in U-Boot:

* XR68C681 DUART channel A (serial console, 115200 8N1)
* XR68C681 counter/timer (100 Hz system timer)
* XR68C681 output port (GPIO)
