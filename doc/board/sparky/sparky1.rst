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

* CPU: Motorola MC68030 at 16 MHz
* DUART: Exar XR68C681 at 0x02000000, clocked at 3.6864 MHz (serial console,
  counter/timer, GPIO)
* SDRAM: 8 MB at 0x00000000, two banks of 4 MB selected by A22
* NVSRAM: Dallas DS1250Y, 512 KB at 0x01000000 — holds the vector table and
  U-Boot itself
* CompactFlash: True IDE mode, 8 registers at 0x03000000

An address decoder splits each 64 MB into four 16 MB quadrants from A25/A24,
one chip select each. A26-A31 are not decoded, so the map repeats every
64 MB through the 4 GB space:

.. list-table::
   :header-rows: 1

   * - Region
     - Base
     - Size
     - Chip select
   * - SDRAM
     - 0x00000000
     - 8 MB
     - /RAMCS
   * - NVSRAM (boot image)
     - 0x01000000
     - 512 KB
     - /ROMCS
   * - XR68C681 DUART
     - 0x02000000
     - 16 B
     - /UARTCS
   * - CompactFlash
     - 0x03000000
     - 8 B
     - /DISKCS

The whole ``A25 = 1`` half of each 64 MB — the DUART and the CompactFlash —
is marked cache-inhibited by the decoder's ``/CIIN``. The NVSRAM decodes only
A18–A0, so it repeats every 512 KB through its quadrant.

RAM is at 0 because that is where Linux wants it, but the 68030 fetches its
reset SSP and PC from 0x00000000 and 0x00000004. A boot override handles that:
a small PLD asserts ``/BOOT`` through ``/RESET`` and for the first 8 bus cycles
after it — the two vectors, read a byte at a time from the 8-bit NVSRAM — and
while it is asserted the decoder sends the bottom quadrant to the NVSRAM
instead of the SDRAM. The reset PC, ``0x01000400``, points into the NVSRAM's
own quadrant, so execution continues from there once ``/BOOT`` releases.

The override counts *completed* bus cycles, so it relies on ``/AS`` being held
negated while the CPU three-states it during reset. **The board needs a pull-up
on /AS** (4.7 kΩ). Without one ``/AS`` floats low through a power-on reset, the
counter sees a cycle end as the CPU starts driving it, and the last byte of
the reset PC comes from uninitialised SDRAM.

Building U-Boot
---------------

A classic m68k cross-compiler is required. ColdFire-only toolchains will not
work as they do not support the full 68030 instruction set.

.. code-block:: bash

    export CROSS_COMPILE=m68k-linux-gnu-
    make sparky1_defconfig
    make

The build produces ``u-boot.bin``, linked at ``CONFIG_TEXT_BASE`` =
0x01000000 and written to the start of the DS1250Y NVSRAM. The image begins
with the exception vector table; ``CONFIG_SYS_MONITOR_BASE`` = 0x01000400 is
where U-Boot's code starts inside it. Programming is done in-circuit; see the
nvram-programmer project, whose firmware must address the NVSRAM at
0x01000000 (A24 high).

Hardware Support
----------------

The following peripherals are supported in U-Boot:

* XR68C681 DUART channel A (serial console, 115200 8N1)
* XR68C681 counter/timer (100 Hz system timer)
* XR68C681 output port (GPIO)
* CompactFlash in True IDE mode (block device, DOS partitions, FAT)

CompactFlash / IDE
------------------

The CompactFlash card is driven by U-Boot's generic IDE driver
(``drivers/block/ide.c``) with a 16-bit data port. The card is wired as a True IDE
device with only the command block decoded:

.. list-table::
   :header-rows: 1

   * - Property
     - Value
     - Kconfig
   * - Base address
     - 0x03000000
     - ``CONFIG_SYS_ATA_BASE_ADDR``
   * - Registers
     - 8, byte-wide, selected by A2–A0
     - ``CONFIG_SYS_ATA_STRIDE=1``
   * - Data port
     - offset 0, 16 bits wide on D31–D16, word accesses only
     - ``CONFIG_SYS_ATA_DATA_8BIT`` not set
   * - Devices
     - one, master only
     - ``CONFIG_SYS_IDE_MAXDEVICE=1``

Three consequences of that wiring are worth knowing:

* **16-bit data port, 8-bit command registers.** ``cfmanager`` revision 04
  wires IDE D0–D7 to D31–D24 and IDE D8–D15 to D23–D16. The data register at
  offset 0 takes only word accesses — a byte access there loses every other
  byte — while the seven command block registers stay byte-wide on D31–D24.
  ``CONFIG_SYS_ATA_DATA_8BIT`` is therefore off, and the driver neither sends
  SET FEATURES 0x01 nor moves sector data a byte at a time.

  The upstream 16-bit paths need no change. The CPU sees each ATA word with
  its bytes exchanged (the ATA low byte on D31–D24), and on big-endian m68k
  ``inw()`` and ``outw()`` exchange them back. So sector data streams into
  memory in file order, and IDENTIFY DEVICE data is stored as true ATA words,
  which leaves the model, serial and firmware strings readable in place.
  Sector buffers must be 2-byte aligned.

* **CS1 is not decoded.** Only A2–A0 reach the card, so the alternate status
  and device control registers at CS1 are unreachable. This is harmless
  because ``ATA_DEV_CTL`` has exactly one user in the driver,
  ``atapi_wait_mask()``, which is compiled out unless ``CONFIG_ATAPI`` is set.
  **Do not enable CONFIG_ATAPI on this board.** Nothing enforces this any
  more: the Kconfig dependency that used to prevent it belongs to
  ``CONFIG_SYS_ATA_DATA_8BIT``, which is now off.

* **RESET– is not software-controlled**, so ``CONFIG_IDE_RESET`` stays off and
  the board provides no ``ide_set_reset()``.

Bus timing — the ATA ``/IORD`` and ``/IOWR`` strobes, the wait states and
``/DSACK0`` — is generated by the ``cfmanager`` GAL, which stretches every CF
cycle to about 437 ns to meet ATA PIO mode 0. The 400 ns the ATA spec requires
between the last data word and the following status read is therefore free.

Throughput is limited by the bus: every sector is 256 word-wide port reads
or writes. It used to be limited by the 100 Hz system timer instead, because
``udelay()`` could not wait for less than a 10 ms tick, and the driver's
``udelay(50)`` after each sector's command cost a whole tick — capping reads
at about 50 KB/s. The board now overrides ``__udelay()`` to count the
XR68C681's counter directly, at 230400 Hz, leaving ``get_timer()`` and the
100 Hz tick untouched. Measured with ``ide read`` and ``ide write`` of 1024
sectors (512 KB):

.. list-table::
   :header-rows: 1

   * - Card
     - Data port
     - Read
     - Write
   * - SanDisk SDCFX-008G
     - 8-bit
     - 1.07 s (about 480 KB/s)
     - 2.9–3.2 s (about 160–170 KB/s)
   * - SDCFXPS-064G
     - 8-bit
     - 0.78 s (about 660 KB/s)
     - 1.1–2.1 s
   * - SDCFXPS-064G
     - 16-bit
     - 0.54–0.55 s (about 940 KB/s)
     - 0.53 s (about 970 KB/s)

The 8-bit figures are from before ``cfmanager`` revision 04, when only IDE
D0–D7 was wired and ``CONFIG_SYS_ATA_DATA_8BIT`` was set.

Using the card::

    ide info                 # controller is probed at boot; this re-reports
    part list ide 0          # partition table
    ls ide 0:1 /             # FAT directory listing
    load ide 0:1 ${loadaddr} /boot/image

Exception Vector Table
----------------------

The 68030 VBR (Vector Base Register) is initialised in two stages:

1. **Before relocation** — VBR is 0 after reset, and 0 is uninitialised SDRAM,
   so the first thing ``start.S`` does is point VBR at the vector table at the
   front of the boot image, ``0x01000000``. That table handles any exception
   during ``board_init_f``.

2. **After relocation** — ``arch_initr_trap()`` (called from ``board_init_r``
   via ``initcall_run_r``) calls ``trap_init(CFG_SYS_SDRAM_BASE)``, which
   writes a fresh vector table directly into RAM at ``0x00000000`` (the base of
   SDRAM). All 254 vectors are filled with the relocated RAM addresses of
   ``_exc_handler`` (vectors 2–24, 32–63) or ``_int_handler`` (vectors 25–31,
   64–255). ``setvbr(0x00000000)`` then points VBR at the new table.

The flash vector table is never copied to RAM; the RAM table is built from
scratch by ``trap_init()`` in ``arch/m68k/lib/traps.c``.

Vectors 0 and 1 of the RAM table are left unused by ``trap_init()``.
``do_reset()`` fills them with the boot image's reset SSP and PC before it
pulses ``/RESET``. The ``RESET`` instruction itself does not reset a 68030,
but the processor is reset if ``/RESET`` stays asserted at least eight clocks
past the instruction's own pulse, and on this board it did while ``/RESET``
rose slowly through the supervisor's 5 kΩ pull-up — about 6 clocks to reach
2.0 V. The CPU then fetched its reset vectors, and the boot override took the
last byte of the reset PC from RAM at 0 rather than from the NVSRAM. A 1 kΩ
pull-up on ``/RESET`` (2.0 V in about a clock) stopped it. The copy is kept as
a safeguard: with the same values in both places, the vector fetch is right
either way.

Reset
-----

``reset`` restarts the board through its reset vectors, as a power-on would.
``do_reset()`` masks interrupts, points VBR back at the boot image's table,
turns the caches off and jumps to ``reset_from_rom()`` in ``start.S`` at its
link-time address in the NVSRAM — not the relocated copy in SDRAM — which
executes ``RESET`` and jumps to ``_start``. Running it from the NVSRAM matters
because ``/RESET`` re-arms the boot override, which then sends the next 8 bus
cycles in the bottom quadrant — where U-Boot runs from SDRAM — to the
NVSRAM.
