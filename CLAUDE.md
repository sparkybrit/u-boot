# U-Boot for Sparky1 SBC

This is a fork of U-Boot (currently rebased to v2026.10) adding support for the **sparky1** — a custom 68030-based single-board computer. The upstream project supports the m68k architecture only for ColdFire (MCF) CPUs; this fork extends it with classic 68030 CPU support and drivers for the XR68C681 DUART.

## Branch layout

All custom work is on the `sparky` branch — currently ~60 commits on top of an
upstream release. `master` is a pristine mirror of upstream
(`https://github.com/u-boot/u-boot.git`) with none of your changes on it, kept
only as a local reference. **Merging work into `master` would break
`git merge upstream/master`; `sparky` is the branch to merge feature branches
into.**

### Rebasing onto a new upstream release

Rebase onto **tags**, not onto `master`. A tag is a fixed, reproducible base;
`master`'s tip moves mid-cycle. Updating `master` is optional housekeeping and
is *not* a prerequisite — what matters is having the tags:

```bash
git fetch upstream --tags     # --tags is required; a plain fetch may not bring them
git checkout sparky
git rebase v2027.01           # one release at a time, never an -rc tag
```

`git describe --tags --abbrev=0 sparky` reports the release you are currently
based on. If it names something implausibly old, the tags are missing rather
than the branch being stale.

U-Boot releases **quarterly** — `vYYYY.01`, `vYYYY.04`, `vYYYY.07`, `vYYYY.10`
— each preceded by `rc1`..`rc5`. So a rebase is due about every three months.
Never rebase onto an `-rc` tag.

### Sizing a hop before starting it

Only files that *both* sides touched can conflict, and that set is usually
tiny. Compute it first:

```bash
git diff --name-only v2026.04 sparky   | sort > /tmp/yours
git diff --name-only v2026.04 v2026.07 | sort > /tmp/upstream
comm -12 /tmp/yours /tmp/upstream
```

For the v2026.04 → v2026.07 hop that is 9 files out of 5808 upstream changed,
and every one is a registration-list collision — upstream added an entry, you
added an entry, keep both:

`arch/m68k/Kconfig`, `arch/m68k/lib/Makefile`, `drivers/block/Kconfig`,
`drivers/serial/{Kconfig,Makefile}`, `drivers/gpio/{Kconfig,Makefile}`,
`drivers/timer/Kconfig`, `doc/board/index.rst`.

For the v2026.07 → v2026.10 hop (2026-10-11) it was 11 of 3647. Two were the
MMC leftover described below, fixed before rebasing; the remaining nine were
the registration lists, `drivers/block/ide.c` and `serial_mcf.c`. **All 59
commits applied with no conflicts at all**, and the result was checked by
comparing the fork's own changes before and after
(`git diff v2026.07 sparky` against `git diff v2026.10 sparky-v2026.10`, sorted
`+`/`-` lines): identical, 51 files, +3971/−33.

If a file shows up in that intersection and has nothing to do with sparky1,
suspect a leftover rather than a conflict to resolve — but restore it to the
tag `sparky` is **actually based on**, not the one it came from. The two MMC
files are the cautionary case. `cmd/mmc.c` and `drivers/mmc/mmc.c` carried
two changes that survived an incomplete revert, and on 2026-08-27 commit
`58007926e80` "restored them to upstream" — to their v2026.04 content, on a
branch already rebased onto v2026.07. Upstream had made exactly those two
changes in v2026.07 (`67fc5052285`, eMMC 5.1B; `6f7d976317c`, a trailing
space), so the cleanup silently reverted them, and the two files showed up
again when sizing the v2026.10 hop. Its stated reason was wrong too:
`MMC_VERSION_5_1B` "not defined anywhere" was true of v2026.04 only;
v2026.07's `include/mmc.h` defines it. Fixed 2026-10-11 by restoring both
files from v2026.07 (`git checkout v2026.07 -- <files>`). Sparky1 does not
build MMC, so the board was never affected. Check with
`git diff <base-tag> sparky -- <file>`, which must be empty for a file sparky1
does not touch.

`drivers/block/ide.c` is a modified upstream file and *will* conflict on some
future hop — see the rebase note in the CompactFlash section for exactly what
is in it.

### Afterwards

A rebase that compiles is not a rebase that works. Rebuild, reflash and
confirm the board boots to a prompt and still probes the CompactFlash before
trusting it.

## What this fork adds (sparky branch summary)

### New CPU support — `arch/m68k/cpu/mc68030/`
- `start.S` — reset vector, early 68030 init, cache setup, jump to `board_init_f`
- `cpu.c` — `cpu_init()`, `watchdog_reset()`
- `cpu_init.c` — `cpu_init_f()` enables both caches; `arch_preboot_os()`
  masks interrupts and turns both caches off before `bootm` enters an OS —
  see [Phase 3](#phase-3--handing-over-to-an-os-bootm)
- `interrupts.c` — interrupt controller init
- `speed.c` — clock/speed reporting

### Sparky1 board — `board/sparky/sparky1/`
- `Kconfig`, `Makefile`, `sparky1.c` — board init, `checkboard()`, `dram_init()`
- `configs/sparky1_defconfig` — full build configuration
- `include/configs/sparky1.h` — legacy config header
- `arch/m68k/dts/sparky1.dts` — device tree: XR68C681 DUART at 0x02000000 (UART, timer, GPIO)

### XR68C681 DUART drivers
- `drivers/serial/xr68c681_serial.c` — serial driver (UCLASS_SERIAL)
- `drivers/timer/xr68c681_timer.c` — 100 Hz counter/timer driver (UCLASS_TIMER); its
  remove method masks the timer interrupt before `bootm` enters an OS
- `drivers/gpio/mc68681_gpio.c` — output port GPIO driver (UCLASS_GPIO)
- `include/xr68c681.h` — register map (`xr68c681_t`) and all bit macros for the discrete chip
- `include/dm/platform_data/mc68681.h` — shared DM platform data struct (`mc68681_plat`)
- Compatible strings: `exar,xr68c681` (serial), `exar,xr68c681_timer` (timer), `motorola,mc68681_gpio` (GPIO)

### CompactFlash / IDE
- `arch/m68k/dts/sparky1.dts` — `ide@3000000` node, `compatible = "u-boot,ide"`
- `board/sparky/sparky1/sparky1.c` — `board_late_init()` probes the controller
- Uses upstream `drivers/block/ide.c` with a 16-bit data port (since
  `cfmanager` revision 04, 2026-10-25), plus `ide_wait_drq()` so a
  late DRQ does not abort a transfer; see
  [CompactFlash / IDE](#compactflash--ide--key-implementation-notes)

### Modified upstream files
- `arch/m68k/include/asm/uart.h` → moved to `arch/m68k/include/asm/coldfire/uart.h` (restored to upstream ColdFire content; `serial_mcf.c` updated to match)
- `arch/m68k/lib/cache.c`, `interrupts.c` — extended for 68030 (non-ColdFire) builds
- `arch/m68k/lib/Makefile` — split ARCH_COLDFIRE bundle; common files now `obj-y`, ColdFire-only (`bdinfo.o`) stays conditional
- `arch/m68k/Kconfig` — added `MC680x0`, `MC68030`, `TARGET_SPARKY1` alongside upstream's QEMU m68k additions
- `lib/time.c`, `drivers/timer/timer-uclass.c` — minor fixes
- `drivers/block/ide.c`, `drivers/block/Kconfig` — added `CONFIG_SYS_ATA_DATA_8BIT`
  (8-bit data port, no longer used by sparky1), a `u-boot,ide` devicetree binding, and a floating-bus
  no-device check. **This file conflicts on rebase** — see below.

## XR68C681 DUART — key implementation notes

The XR68C681 is Exar's version of the MC68C681 Dual UART with extended features. All three drivers (serial, timer, GPIO) include `<xr68c681.h>` and use the `xr68c681_t` register struct. Platform data (`mc68681_plat`) is shared via `<dm/platform_data/mc68681.h>`.

### Baud rate generation (XR68C681-specific)

The XR68C681 has a per-channel **BRG extend bit (X bit)** that selects an extended column in the BRG rate table. It is set/cleared via CR Miscellaneous Commands (Table 3 of the datasheet), which live in the **upper nibble CR[7:4]**; the lower nibble CR[3:0] must be zero to avoid TX/RX side-effects:

| Command | Value | Effect |
|---------|-------|--------|
| `UART_UCR_SET_RX_EXTEND` | `0x80` | Set Rx X=1 (extended rates) |
| `UART_UCR_CLR_RX_EXTEND` | `0x90` | Clear Rx X=0 |
| `UART_UCR_SET_TX_EXTEND` | `0xA0` | Set Tx X=1 |
| `UART_UCR_CLR_TX_EXTEND` | `0xB0` | Clear Tx X=0 |

115200 baud requires CSR=`0x88` **plus** X=1 on both channels. CSR=`0xBB` alone gives 9600, not 115200.

The BRG table in `xr68c681_serial.c` encodes both the CSR value and the extend flag for each supported rate.

### ACR register

ACR (Auxiliary Control Register) at offset 0x04 is **write-only** — reads at that address return IPCR (Input Port Change Register), not ACR. Never use read-modify-write on ACR; always write the full value directly.

`UART_UACR_CLK` (`0xF0`) = BRG Set 2 (bit 7) + Timer mode X1/CLK÷16 (bits 6:4 = 111). This value is shared by both the serial init and the timer probe.

### Timer mode

The counter/timer runs at 100 Hz: N=0x0480 (1152), f = (3.6864 MHz / 16) / (2 × 1152) = 100 Hz. The interrupt is cleared by reading `uopc` (0x0F). In timer mode, the "stop counter" command does **not** actually halt the counter — it only clears ISR[3]. The `clock-frequency` DTS property must be set to `100`.

Before `bootm` enters an OS, the driver's remove method (`DM_FLAG_OS_PREPARE`)
writes `uimr = 0` and reads `uopc`, masking the interrupt at the DUART and
clearing any pending tick. The counter keeps running, so `__udelay()` still
works up to the jump. See [Phase 3](#phase-3--handing-over-to-an-os-bootm).

## CompactFlash / IDE — key implementation notes

A CompactFlash card in True IDE mode hangs off `/DISKCS` at `0x03000000`. It is
driven by upstream's generic `drivers/block/ide.c`, which needed four changes
to cope with the wiring.

### The wiring

| Property | Value | Kconfig |
|----------|-------|---------|
| Base address | `0x03000000` | `CONFIG_SYS_ATA_BASE_ADDR` |
| Registers | 8, byte-wide on D31–D24, selected by A2–A0 | `CONFIG_SYS_ATA_STRIDE=0x1` |
| Control block | alternate status / device control at `+0xE` (`-CS1` = !A3, `cfmanager` revision 05) | `CONFIG_SYS_ATA_ALT_OFFSET=0x8` |
| Data port | offset 0, **16 bits** on D31–D16, word accesses only | `CONFIG_SYS_ATA_DATA_8BIT` **not** set |
| Devices | one, master only | `CONFIG_SYS_IDE_MAXDEVICE=0x1` |

Timing (`/IORD`, `/IOWR`, wait states, `/DSACK0`) comes from the `cfmanager`
GAL in the sparky1 hardware project, which stretches every CF cycle to ~437 ns
for ATA PIO mode 0. The 400 ns ATA requires between the last data word and the
next status read is therefore free — no extra delay in the driver.

### 16-bit data port — since `cfmanager` revision 04

**Changed 2026-10-25**, branch `cf-16bit`, commit `b4eb3ff0c9b`. Revision 04
wires CF D0–D7 to D31–D24 (as before) and **CF D8–D15 to D23–D16**. The data
port at offset 0 takes **word accesses only** — a byte access there loses every
other byte — while the seven command registers are still byte-wide on D31–D24.

The only change was dropping `CONFIG_SYS_ATA_DATA_8BIT=y` from the defconfig.
That also removes `ide_set_8bit_mode()`, so no `SET FEATURES 0x01` is sent.
**No driver change was needed**, and the reason is worth knowing:

- The CPU sees each ATA word **byte-swapped** — the ATA low byte arrives on
  D31–D24, the high half of a big-endian word.
- On big-endian m68k, `inw()`/`outw()` are `in_le16()`/`out_le16()`, which
  swap it back. So upstream's `le16_to_cpu(inw())` for sector data cancels
  out to a raw word move, keeping the bytes in stream order, and
  `be16_to_cpu(inw())` for IDENTIFY leaves one swap and gives true ATA words.
  The disassembly shows exactly that: plain `movew` in `ide_read()`/`ide_write()`,
  `movew` + `rorw #8` in the IDENTIFY loop.
- Sector buffers must now be **2-byte aligned**.

**Tested on the board 2026-10-25** (card `SDCFXPS-064G`): IDENTIFY correct
(model, `HDX16.01`, serial, 125059072 sectors via LBA48 — every other string
character crosses D8–D15, so that alone proves the upper lane and the order);
probe clean on 3/3 `reset`s; LBA 0 still reads `55 aa` at `0x1FE` (written
before the change, so a check against data this image did not write);
1-, 8- and 2 × 1024-sector write/read/`cmp` round trips with varied data and a
poisoned destination all matched; trial 1's data re-read after trial 2 matched.
**About 2.5 MB written — not yet soak tested.** The 40 M-sector soak below was
all on the 8-bit port. D8–D15 have the same 33R series termination as
D0–D7 (confirmed 2026-10-25), but their signal integrity is unproven at
volume.

### The 8-bit data port — still in the driver, no longer used

Before revision 04, only the low byte of the ATA data bus was wired, so `CONFIG_SYS_ATA_DATA_8BIT`
switches `ide_input_data()`, `ide_output_data()` and `ide_input_swap_data()` to
byte transfers, and makes `ide_ident()` call `ide_set_8bit_mode()` before
IDENTIFY — which issues `INITIALIZE DEVICE PARAMETERS` (0x91, tolerated if it
fails; it only affects CHS translation, never used here) and then
`SET FEATURES` (0xEF) with subcommand `0x01`, "enable 8-bit PIO data transfer".
That last one matters: without it the card also drives D8–D15.

`ide_set_8bit_mode()` sits in `ide_ident()` rather than `ide_init_one()`
because 8-bit mode is a **per-device** setting, and `ide_init_one()` runs once
per bus.

### Byte ordering in 8-bit mode — the part that is easy to get wrong

The two data paths need *different* handling, and the reason is not symmetric:

- **Sector data** (`ide_input_data`/`ide_output_data`) — the card streams the
  sector a byte at a time in its natural order, so the buffer is filled
  byte-for-byte with no swapping on either endianness. It also need not be
  2-byte aligned, unlike the 16-bit path.

- **IDENTIFY data** (`ide_input_swap_data`) — stored as **native-endian 16-bit
  words**, i.e. `word = (hi << 8) | lo`. ATA puts the first character of each
  string in bits 15–8, so on big-endian m68k a native word store is exactly
  what leaves `iop.model` / `serial_no` / `fw_rev` readable in place, and
  leaves `iop.config`, `lba_capacity[]` and `command_set_2` holding true ATA
  word values (`be16_to_cpu()` in `ide_ident()` is then a no-op).

This matches the reference implementation in the hardware project's
`tests/cf_test/cf_test.c` (`put_ata_str()`, `word_at()`, `dword_at()`), and the
`hd_driveid_t` field offsets line up with the ATA word numbers it uses:
`config` @ 0, `serial_no` @ 20, `fw_rev` @ 46, `model` @ 54, `lba_capacity`
@ 120, `command_set_2` @ 166.

### The control block — reachable since `cfmanager` revision 05

**Since 2026-10-25** the card's `-CS0` is A3 and `-CS1` is !A3 (`cfmanager`
revision 05, pin 23), so the control block's one register — alternate status
on read, device control on write — sits at `0x0300000E`. `ATA_DEV_CTL` is
`CONFIG_SYS_ATA_ALT_OFFSET + 6 × stride`, hence `ALT_OFFSET = 0x8`. Offsets
`+8`–`+D` are not registers; never touch them. Before revision 05 `-CS0` was
tied low and `-CS1` high, so the block was unreachable.

`ATA_DEV_CTL` has exactly one user in the driver, `atapi_wait_mask()`, which
is compiled out unless `CONFIG_ATAPI` is set, so the offset does not change
the sparky1 image today. It is there for anything that does use it, and it
matches the control resource Linux's `pata_platform` will want.

**Checked on the board 2026-10-25** (card `SDCFXPS-064G`): `md.b 0x0300000e 1`
reads `50`, the same as status at `+7`; with sector count and LBA low set to
`a5`/`5a`, writing `0x06` then `0x02` to device control (SRST with nIEN, then
nIEN alone) left the reset signature `01 01 00 00` in sector count/LBA, and
the device register read `a0`. `ide reset` then probed the card normally and
LBA 0 still read `55aa`.

`RESET–` is still not software-controlled — it is on the board's `/RESET` —
so `CONFIG_IDE_RESET` stays off and there is no `ide_set_reset()`. Instead
**`CONFIG_IDE_SRST`** (added on branch `ide-srst`, 2026-10-25) makes
`ide_reset()` issue SRST through device control on each bus: nIEN + SRST,
`udelay(5)`, nIEN alone, `mdelay(2)`, then `ide_init_one()`'s existing BSY
wait. It runs on every probe — at boot and on each `ide reset`. The option
`depends on !IDE_RESET`, and its help warns that `SYS_ATA_ALT_OFFSET` must
point at a real control block: at the default 0 the write lands on the
device/head register instead.

**Checked on the board 2026-10-25** (card `SDCFXPS-064G`), first with a
throwaway build printing alternate status around the reset: `d0` (BSY) while
SRST is held; after 2 ms, with `a5`/`5a` planted in the task file beforehand,
`01 01 00 00 a0` — the reset signature — and status `50`. At boot the card is
still BSY (`80`) at 2 ms, just after the power-on hard reset, and the BSY wait
covers it. Then on the clean build: 3/3 `reset`, 5/5 `ide reset`, a
1024-sector read / rewrite-in-place / poisoned read-back / `cmp` matching, and
LBA 0 still `55aa`. Do not judge SRST from the task file after `ide reset`:
the probe's IDENTIFY overwrites it.

### Throughput

**16-bit port, 2026-10-25, `SDCFXPS-064G`:** `ide read` 1024 sectors in
0.54–0.55 s (~940 KB/s), `ide write` 1024 sectors in 0.53 s (~970 KB/s),
against 0.78 s and 1.1–2.1 s for the same card on the 8-bit port. The rest of
this section is the 8-bit history.

Was limited by the 100 Hz system timer, not the bus, until `__udelay()` got a
finer time source (see below). Measured on a SanDisk SDCFX-008G:

| | 100 Hz tick floor | counter-based `udelay()` |
|---|---|---|
| `ide read` 128 sectors (64 KB) | 1.300 s | 0.140–0.150 s |
| `ide read` 1024 sectors (512 KB) | 10.250 s | 1.070 s |
| per sector | 10.01 ms | 1.045 ms |
| bulk read rate | **50.0 KB/s** | **478 KB/s** |
| `load` of a 4 KB file | ~1.4 s | 0.150 s |
| `ide write` 1024 sectors (512 KB) | — | 3.160 s (**162 KB/s**) |
| `ide reset` (full probe) | 0.360 s | 0.090 s |

The cost was **not** `ide_wait()` — its `udelay(100)` only runs when the card is
actually BUSY. It was the **unconditional `udelay(50)` inside `ide_read()`'s
per-block loop**, right after the PIO read command. Every `udelay()` on this
board used to cost a full 10 ms tick whatever it asked for, because
`usec_to_tick(50)` is 0 at 100 Hz and `__udelay()`'s loop is
`while (get_ticks() < tmp+1)` — the `+1` forces a wait for the next tick edge.
One tick per sector capped reads at ~100 sectors/s ≈ 51 KB/s, and the measured
10.01 ms per sector says that was the whole of it.

At 1.045 ms per sector the read is now bounded by the transfer itself — 512
single-byte port reads — rather than by any delay, so the next gain would have
to come from the bus — which is what `cfmanager` revision 04's 16-bit port
then did (figures above).

Writing gained about as much: 512 KB in **2.87–3.01 s** (~170 KB/s), against an
old filesystem-level figure of ~18 KB/s (2 MB of metadata in 112 s). Long writes
now run to completion — the `no IRQ` stall that used to abort them is fixed —
and no corruption has been seen in 40 million sectors since; see below.

### A real time source for `udelay()`

**Done — 9.6× on bulk CF reads, measured.** Also improves every timeout in
U-Boot; nothing depended on the old behaviour. Lives on branch
`sparky-udelay`, three commits, **not yet merged into `sparky`**.

**Why it is safe to change.** The 100 Hz timer interrupt does exactly one thing:

```c
static void timer_interrupt_handler(void *arg)
{
	readb(&base->uopc);   /* clear the interrupt */
	counter++;            /* and that is all */
}
```

`counter` has exactly two references — incremented there, read by
`get_count()`. It is the only IRQ handler installed (the other
`irq_install_handler()` call lives in `arch/m68k/lib/time.c`, which is
`obj-$(CONFIG_MCFTMR)` and not built for sparky1), and it is the only DUART
interrupt source enabled: the serial driver writes `uimr = 0`, the timer then
writes `uimr = 0b1000`. Nothing else uses the interrupt.

**What is NOT safe to change casually.** The timer *as a time source* backs
`get_timer()`, `get_ticks()` and every timeout in U-Boot, and the timer uclass
converts counts to time using `priv->clock_rate`, which comes from
`clock-frequency = <100>` in `arch/m68k/dts/sparky1.dts`. Redefining what
`counter` means ripples through all of that.

**The contained fix: override `__udelay()` at board level.** It is declared
`__weak` in `lib/time.c`, and two trees already override it
(`board/armltd/integrator/timer.c`, and the ColdFire `arch/m68k/lib/time.c`).
The sparky1 `__udelay()` in `board/sparky/sparky1/sparky1.c` spins on the live
counter and leaves `counter`, `clock-frequency` and `get_timer()` untouched.
The XR68C681 counter decrements at 3.6864 MHz / 16 = **230400 Hz**, one count
per **4.34 µs** — about 2300× finer than the 10 ms tick.

**How it finds the hardware.** From `gd->timer`, which `dm_timer_init()` sets
only *after* `device_probe()` returns — so a non-NULL `gd->timer` is itself the
proof that `xr68c681_timer_probe()` has run and started the counter. Its
`mc68681_plat` supplies the register base and its `timer_dev_priv.clock_rate`
supplies the tick rate, from which the reload value N is recovered as
`230400 / (2 × clock_rate)` rather than duplicating the driver's `0x0480`.
A `gd->timer->driver != DM_DRIVER_REF(xr68c681_timer)` check guards the plat
cast; testing `gd->timer` directly is also what keeps this off the
`get_ticks()` path, which would otherwise call `dm_timer_init()` and panic.

**The wrinkles, and what they cost:**

1. In timer mode the counter reaches zero **twice** per interrupt period
   (`f = clk / 2N`, N = 1152), so its absolute phase is ambiguous. Only
   *elapsed* counts are used, so that does not matter here — but it is why this
   cannot naively be reused to make `get_count()` fine-grained as well.
2. The counter is not running until `xr68c681_timer_probe()` starts it, so a
   `udelay()` from `board_init_f` falls back to a NOP loop scaled from
   `CFG_SYS_CLK`. Accurate to no better than a factor of two, which is all
   early code needs. (Nothing currently calls it that early; before this
   change such a call would have panicked.)
3. CTU and CTL are two byte reads of a running counter, so the upper half is
   read twice and the pair retried if it moved. It changes once every 256
   counts (~1.1 ms), so this never spins more than once.
4. Whether the reload consumes a clock — making the cycle N+1 counts rather
   than N — is not worth pinning down. The two differ by 0.09%; the code
   counts the longer, which errs towards delaying slightly too long.

**Measured result.** Per-sector cost dropped from 10.01 ms to 1.045 ms, so
reads are now bounded by the transfer itself (512 byte-reads) exactly as
predicted — the estimate of ~0.5–1 ms/sector was right. Bulk reads went from
50.0 KB/s to 478 KB/s. See the throughput table above for the full set.

Correctness was re-checked at the same time, because every timeout in the IDE
driver just had its resolution changed and a card being carried by the old
10 ms floor would now fail:

- the CF probes on the first attempt at boot, with model, firmware and serial
  all intact;
- two independent 64 KB reads of the same LBA are byte-identical;
- LBA 0 still reads `0x55aa` at offset `0x1FE`, so the sector byte order is
  right against data the board did not write;
- `part list` parses the FAT32 partition, and a PC-written text file reads back
  as correct ASCII.

The write path was tested too, at LBA `0x100000` in unallocated space — on the
current reference card the documented LBA `0x1000` lands inside FAT2. It is
about 9x faster as well, but it **also turned out to corrupt sectors**, on the
old image just as much as the new one. That corruption was later traced to
signal integrity on the CF data lines and largely cured by termination
resistors; making `udelay()` honest also exposed a latent driver bug in the
write handshake, since fixed. See the next section for both.

### The write path — corruption fixed, stall fixed

**Worked 2026-08-30, confirmed by soak testing 2026-08-31.** Two independent
faults were stacked on top of one another, and neither was quite where the
2026-08-29 notes suspected. Both are now fixed — one in hardware, one in this
driver.

**1. Byte-duplication corruption — a hardware fix.** ~5–8% of written sectors
used to come back with one byte duplicated and the rest of the sector shifted a
byte later. Fitting **33R series termination resistors on the data lines at the
IDE connector** cured it. Nothing in this fork changed for it. The old suspicion
— `cfmanager`'s `/IOWR` timing — was the right neighbourhood in that it is
signal integrity on the CF bus, but termination rather than a PLD change is what
fixed it.

**Exactly one corrupt sector has ever been seen since**, on 2026-08-30, in the
first few thousand sectors written after the resistors went on. That single
event was briefly recorded here as a rate of "1 in 46,736" — **that was wrong**,
and the figure is retracted. It was one event with enormous error bars, and
soak testing has since contradicted it decisively:

| card | sectors written, read back and compared | corrupt |
|---|---|---|
| SanDisk SDCFX-008G (4 runs) | 17,059,840 | **0** |
| `SDCFXPS-032G` (12 h) | 11,395,072 | **0** |
| `SDCFXPS-064G` (12 h) | 11,369,472 | **0** |
| **total** | **39,824,384** (20.4 GB) | **0** |

Across all three cards that bounds the rate below **7.5e-08** per sector at 95%
confidence; on the reference SanDisk alone, below 1.8e-07 — over 100x below the
retracted figure. At the retracted rate those runs would have produced more than
850 corrupt sectors.

> **Treat writes as sound, but note the one unexplained event.** It was real —
> verified on the card by two independent read-backs, with the classic
> duplicate-and-shift signature — so something produced it. It has not
> reproduced in 40 million sectors. If it ever returns, the signature below is
> how to recognise it. A brownout was briefly a candidate, on the strength of
> a rail reading that turned out to be a miscalibrated meter — see the reset
> section below. Nothing currently explains it.

**2. `Error (no IRQ) ... status 0x50` on long writes — a driver fix.** With the
corruption out of the way, streaming writes turned out to abort after 50–700 blocks,
about one stall per 170 blocks. `ide_write()` issues `ATA_CMD_PIO_WRITE` per
block, waits `udelay(50)`, then requires DRQ — but `ide_wait()` only spins
*while BSY is set*, so a card that has not asserted BSY yet, being still busy
committing the previous sector, reads back as "not busy, no DRQ" and the write
is abandoned. Status `0x50` is `DRDY|DSC` with BSY, DRQ **and** ERR all clear,
which is precisely that signature: `ide_wait()` returned normally rather than
timing out.

The fix is `ide_wait_drq()` in `drivers/block/ide.c` — poll until the device is
both out of BSY *and* asserting DRQ, break early on ERR, timeout unchanged. It
replaces the DRQ wait in **both** `ide_write()` and `ide_read()`; the read path
never stalls in practice (after a read data phase the card has no flash to
program) but carries the identical latent bug, and fixing only one made the
asymmetry look arbitrary for upstream.

Only the **DRQ waits** were changed. Every BSY-only `ide_wait()` call is left
alone, which matters: the wait after `ATA_CMD_CHK_POWER` in `ide_read()` follows
a *non-data* command where DRQ is never asserted, so routing it through
`ide_wait_drq()` would spin out the full 2 s timeout on every single read.

The read path was measured before and after to prove no regression — 20 x 1024
blocks each way, every read paired with a second read of the same LBA and
compared:

| | reads OK | paired reads differed | 1024-block time |
|---|---|---|---|
| before | 20/20 | 0 | 0.785 s mean |
| after | 20/20 | 0 | 0.775 s mean |

Unchanged, as expected: when a device asserts BSY then DRQ normally, both
helpers return at the same moment. They differ only in the case that was
breaking writes.

**The evidence that isolated it to the driver**, all taken before the fix:

| | result |
|---|---|
| `ide read` 1024 blocks ×5 | 5/5 complete, 5120 blocks, no stall |
| `ide write` 1024 blocks ×10 | **0/10** complete, stalling at 54–704 blocks |
| isolated single-block writes | 150/150 clean |
| single-block retry at the stalling LBA | succeeds 5/6, immediately |
| read timings against the table above | unchanged (0.130 s / 1.050 s) |

Reads issue the same LBA and command register writes over the same data bus, so
a bus fault would have stalled them too; and the card was demonstrably healthy
at the exact LBA it had just refused. That left the driver's handshake.

**After the fix, same hardware, only `ide.c` changed:**

| | before | after |
|---|---|---|
| `ide write` 1024 blocks | 0/10 complete | **50/50 complete** |
| longest run before a stall | 704 blocks | **no stall in 39,824,384 blocks** |
| sectors written + verified | 4592 (harvested from partial writes) | **39,824,384** |

The soak figure is the strong one: **zero `no IRQ` stalls in 40 million blocks**
across three cards. At the pre-fix rate of one per 170 blocks those runs would
have hit roughly 234,000 aborts.

The 8-sector reproduce case passes either way — 8 blocks is short enough to
clear a ~1-in-170 stall most times, which is exactly why the stall only showed
up once writes were tested at length.

Why it did not bite before the resistors is not fully settled — the 3.160 s
figure in the throughput table is a complete 1024-block write on this same
firmware. The likeliest reading is that it was always marginal: the race needs
the card to be slow to assert BSY on some particular sector, which depends on
its internal state, and several MB have been written to it since.

### Open — the board reboots under sustained CF writes, rarely

**Confirmed 2026-09-01, and rare.** This is not corruption; every sector
written in these runs still verified.

A soak instrumented to check every reply for U-Boot's banner caught one
outright. The write command echoes, U-Boot prints its transfer line, and then
the banner appears part-way through:

```
ide write 0x40100000 0x2a6c00 0x400
ide write: device 0 block # 2780160, count 1024 ...
U-Boot 2026.07-00042-g8ca11aba965b-dirty (Sep 01 2026 - 07:40:12 +0100)
CPU:   Motorola MC68030
...
```

**Rate: 1 reboot in 7,395,328 sectors** over a 12 h run — the only run so far
with detection in it.

> **That is one event, and one event is not a rate.** The Poisson interval on
> a single observation spans roughly 1-in-400,000 to 1-in-90,000,000 sectors.
> What is established is that the reboot is real and rare, not how rare. This
> is the same trap that produced the retracted corruption figure above.

**Swapping cards does not settle it either.** A 12 h instrumented run on a
third card, `SDCFXPS-064G`, produced 11,369,472 sectors with **zero** events of
any kind — no reboots, corruption, stalls, errors or slow writes.

That is *not* evidence the card is better. At the SanDisk's measured rate the
expected count over that run is **1.52**, so seeing zero anyway has probability
**0.22** — the two cards' plausible ranges overlap almost entirely. Comparing
cards needs several events, which means either far longer runs or a workload
built to provoke resets rather than one that merely might.

**What the cards genuinely differ in is latency, not resets:**

| card | typical 1024-block write | worst seen | slow writes >10 s |
|---|---|---|---|
| SanDisk SDCFX-008G | 3.1 s | **90.7 s** | 3 in 7.2 M sectors |
| `SDCFXPS-064G` | 1.1–2.1 s | 2.1 s | **0 in 11.4 M** |

That difference is cleanly measured, unlike the reset rate. If a long write
latency matters to something built on this board, the card choice is real.

**An earlier claim here was wrong.** This section previously reported "24 such
events in ~9.7 M sectors" on the SanDisk, inferred by assuming every
`unparsable write reply` in the uninstrumented runs was a reset. Proper
detection puts it about **18x rarer** than that, so most of those replies were
something else. Do not infer a reboot from a truncated reply; test for the
banner.

**Run 3 and run 4 differ in a way nothing explains.** Same card, same wiring,
same rail, same duration, nothing changed physically:

| | run 3 (overnight) | run 4 (daytime) |
|---|---|---|
| sectors | 7,217,152 | 7,395,328 |
| `unparsable`/other errors | 13 | **0** |
| slow writes >10 s | 3 (max 90.7 s) | **0** (max 6.2 s) |
| confirmed reboots | not instrumented | 1 |

**This matters for testing any fix:** a single clean run proves nothing,
because run 4 was already clean without any change. A power-path fix has to be
measured against run 3's conditions, not run 4's, and until what varies between
them is understood that is hard to arrange deliberately.

**The power-supply explanation is currently unsupported.** A reading of
4.75 V at the IDE adapter on 2026-09-01 — exactly the supervisor's trip point —
briefly made this look solved. **That reading was wrong**: the in-circuit panel
meter was miscalibrated and reading low. Checked against a multimeter and
retuned on 2026-09-02, the real picture is healthy:

| | |
|---|---|
| six rails across five breadboards | 4.90–4.94 V |
| spread end to end | **40 mV** (4.94 V nearest the supply, 4.90 V furthest) |
| margin to the 4.75 V trip | **150–190 mV**, about 3% |
| average draw | 450 mA on a 1 A regulator — **45% load** |
| regulator temperature | warm, not hot |

40 mV across five chained breadboards at 450 mA implies about **89 mΩ** of
total path resistance, so even a 200 mA transient step costs only ~18 mV. The
distribution is not the problem at DC, the regulator is neither current-limited
nor thermally folding back, and **a larger regulator is not indicated** — it
would raise a ceiling the board is nowhere near.

None of that *disproves* a brownout: a meter shows an average and cannot see a
microsecond dip, and reaching 4.75 V from 4.91 V during a write burst is not a
big excursion. But there is no longer any evidence *for* it, and the one
confirmed reset is unexplained.

**To make progress this needs a transient capture, not more meter readings** —
an analog probe on the rail *at the CF adapter* plus a digital channel on
`/RESET`, triggered on the reset edge. Failing that, bulk plus ceramic
decoupling at the CF socket is the cheap speculative fix, since local energy
storage is what actually supplies a burst while the regulator's loop catches up.

> **Three claims in this investigation went into these notes as measured fact
> and were later retracted**, each one a single unvalidated number treated as
> settled: corruption at "1 in 46,736" (bounded 100x lower over 28 M sectors),
> "24 reboots per ~9.7 M sectors" (1 in 7.4 M once instrumented), and this
> 4.75 V rail (meter miscalibrated). The caveat about panel-meter accuracy was
> even written down here before being ignored. State what a measurement can
> support, and validate the instrument before building on it.

**Why it is not confirmed:** the harness logged only the last 220 bytes of each
reply, so U-Boot's banner — which would settle it outright — was truncated away.
Grepping the logs for `U-Boot 2026` finds nothing, and that proves nothing.
A rerun must keep the whole transcript and test for the banner explicitly.

**To settle it:** analog probe on the 5 V rail *at the CF socket* plus a digital
channel on `/RESET`, triggered on `/RESET` falling. A voltmeter reading 4.85 V
cannot see a microsecond transient. Cheapest first move is to unplug the Teensy
and rerun — it drops rail load and bus loading together — after confirming the
4k7 pull-ups on `/BR` and `/BGACK` (see the CPU sheet in the hardware project).

### Open — a slow region on the reference card

Writes that normally take 3.1 s for 1024 blocks occasionally take far longer,
and the slow LBAs repeat across independent runs:

| run | LBA | duration |
|---|---|---|
| 2 | `0x1a7c00` | 47 s |
| 3 | `0x1a8c00` | 12.6 s |
| 3 | `0x1ac400` | 10.2 s |
| 3 | `0x1ae800` | — |
| 3 | `0x1e4c00` | 90.7 s |

`0x1a7c00`–`0x1ae800` recurring across runs looks like a genuinely weak region
rather than routine garbage collection. Note no U-Boot timeout can cover a 90 s
write — `IDE_TIME_OUT` is 2 s — so a card that disappears for that long will
fail whatever the driver does.

A card browning out mid-program and retrying internally would produce the same
signature, and the repeatability by LBA could simply be where the workload puts
its heaviest bursts — but the rail reading that made that plausible was a
miscalibrated meter, so treat it as an open possibility rather than a lead.

Note also that run 4 saw **no slow writes at all** (slowest 6.2 s) over 12 hours
on the same card, where run 3 saw three including one of 90.7 s. Whatever drives
this is not simply a bad area of flash, or it would show every pass.

### Rebase note

`drivers/block/ide.c` is now a modified upstream file and will conflict when
rebasing through release tags. The changes are three `#if
IS_ENABLED(CONFIG_SYS_ATA_DATA_8BIT)` blocks around the data-path functions,
the `ide_set_8bit_mode()` helper and its call in `ide_ident()`, the `0xFF`
no-device check in `ide_init_one()`, `.of_match` on `U_BOOT_DRIVER(ide)`, the
`ide_wait_drq()` helper and its use in `ide_write()` and `ide_read()` (not
board-specific — the existing code aborts a transfer whenever the device has not
asserted BSY yet, so this is an upstream bug too and is queued for the list),
the `CONFIG_IDE_SRST` branch in `ide_reset()` (which also moved below
`ide_outb()`/`ide_inb()` to use them; a candidate for upstream too), and
the `strlcpy()` size fix in `ide_probe()` (upstream passes `BLK_*_SIZE` where
the buffers are `BLK_*_SIZE + 1`, which truncates a maximum-length string by
one character - it printed `HDX 5.08` as `HDX 5.0`). That last one is an
upstream bug, not a sparky1 change, and **is fixed upstream in `next`**
(`0b4c421190a`), so the first release with it is **v2027.01**. It is *not* in
v2026.10. Keep the local copy until `sparky` is rebased onto v2027.01; before
that, dropping it brings the truncation back. On that hop the `ide.c` part of
the local commit is identical to upstream's and should merge to nothing —
check with `git diff v2027.01 sparky -- drivers/block/ide.c | grep strlcpy`,
which should then print nothing.

To check whether a fix is in a release, ask git directly —
`git merge-base --is-ancestor <commit> <tag> && echo yes` — rather than reading
`git tag --contains` output among other commands' output. A misread of exactly
that put "in v2026.10" into these notes on 2026-10-11; the v2026.10 rebase
showed it was wrong.

### Upstream submission — the `strlcpy()` fix

**Accepted and merged into `next`.** Sent 2026-08-29; Patchwork state
`accepted`; landed as `0b4c421190a` ("ide: Fix truncation of device strings by
one character"), committed by Tom Rini on 2026-09-15 to `next`, during the
v2026.10 `-rc` period, and merged into `main` after the v2026.10 release. It is
**not in v2026.10**; the first release with it is v2027.01. Patch-identical to
what was sent. The worktree and the `ide-strlcpy-fix` branch were removed on 2026-10-11;
the `sent/…-v1` tag is kept as the record of exactly what went out.

This was an **upstream U-Boot bug**, not a sparky1 change, so it went to the
mailing list and drops out of this fork on the v2027.01 rebase — see the rebase
note above.

| | |
|---|---|
| Message-Id | `20260829-ide-strlcpy-fix-v1-1-05676b02d822@gmail.com` |
| Archive | `lore.kernel.org/u-boot/<message-id>/` |
| Patchwork | `patchwork.ozlabs.org/project/uboot/` |
| Local tag | `sent/20260829-ide-strlcpy-fix-83225ba994d5-v1` |
| Based on | `upstream/main` @ `67a095a6896` (v2026.10-rc3-13) |

b4 tagged v1 as sent and rolled the tracking branch to **v2**, ready for a
reroll if review asks for changes. Note `lore.kernel.org` sits behind Anubis
(a proof-of-work anti-scraper gate), so scripted fetches of it fail — use the
Patchwork REST API instead:

```bash
curl -s "https://patchwork.ozlabs.org/api/patches/?project=uboot&msgid=<message-id>"
```

The rest of this section records how it was done, which is what a reroll or a
second patch will need.

**U-Boot takes emailed patches, not GitHub PRs.** The tree ships a `.b4-config`,
so `b4` is the recommended tool.

**The bug.** `ide_probe()` copies the IDENTIFY strings into the block device's
descriptor with `strlcpy(dst, src, BLK_*_SIZE)`. `strlcpy()` takes the full
buffer size *including* the NUL and copies at most `size - 1` characters, but
these fields are declared `char[BLK_*_SIZE + 1]` — so a maximum-length string
loses its last character. An 8-character firmware revision (`HDX 5.08` on the
SanDisk CF card here) is reported by `ide info` as `HDX 5.0`. The boot banner is
unaffected because it prints the local `blk_desc` that `ide_ident()` filled,
*before* this copy — that boot-vs-`ide info` split is what identified it.

**The root cause** is a `strncpy()` → `strlcpy()` conversion that kept the size
argument even though the two functions interpret it differently. The original
was correct:

```c
strncpy(desc->vendor, ..., BLK_VEN_SIZE);
desc->vendor[BLK_VEN_SIZE] = '\0';
```

copying up to `BLK_VEN_SIZE` characters into a `BLK_VEN_SIZE + 1` byte buffer,
where `strlcpy(..., BLK_VEN_SIZE)` copies only `BLK_VEN_SIZE - 1`. The `Fixes:`
tag is therefore `db89e72302d0` ("ide: Move setting of vendor strings into
ide_probe()") — **not** `d7d57436e7a6`, which only swapped the source operand
and left the size argument untouched. Check with
`git log -S 'BLK_VEN_SIZE' -- drivers/block/ide.c`.

The fix passes `sizeof()` the destination instead, and is deliberately a bare
three-line change with no comment.

#### Where it lived — a worktree, not a checkout

```
~/Projects/u-boot-ide-fix     branch ide-strlcpy-fix, based on upstream/main
```

(Both removed 2026-10-11, after it merged. A second patch should follow the
same pattern under its own name.) The worktree is what makes this work at all. `CLAUDE.md` does not exist
upstream and usually has uncommitted edits, so `git checkout -b … upstream/main`
in the main tree **refuses** rather than deleting it — the `git stash push
drivers/block/ide.c` route does not survive the checkout. A worktree sidesteps
the stash entirely and leaves the `sparky` tree untouched:

```bash
git fetch upstream --tags
git worktree add -b ide-strlcpy-fix ~/Projects/u-boot-ide-fix upstream/main
```

**Base on `upstream/main`, not `upstream/master`.** `upstream/master` is a
stale ref trailing the live branch by a long way — confirm with
`git rev-parse upstream/master upstream/main`, which shows two different
commits.

#### Tooling on beelink0

Ubuntu 24.04 refuses `pip install b4` (PEP 668,
`error: externally-managed-environment`). `apt install b4` works but needs sudo
and gives an older 0.13.0. The no-sudo route is a venv, which also gets a
current b4:

```bash
python3 -m venv ~/.venvs/b4
~/.venvs/b4/bin/pip install b4
ln -sf ~/.venvs/b4/bin/b4 ~/.local/bin/b4      # ~/.local/bin is already on PATH
```

Done — b4 0.16.0. `git send-email` is **not** installed (package `git-email`,
needs sudo) and is not needed: b4 speaks SMTP itself.

#### The commit — already made

`git commit -s` (the `-s` adds `Signed-off-by:`, a Developer's Certificate of
Origin assertion, not a formality). Both `scripts/checkpatch.pl --strict -g HEAD`
and `b4 prep --check` are clean. Recipients came from:

```bash
b4 prep --enroll upstream/main
b4 prep --auto-to-cc
```

giving **To** Simon Glass `<sjg@chromium.org>` and the list, **Cc** Tom Rini
`<trini@konsulko.com>` (maintainer). b4 folds the cover letter into a
single-patch series automatically, so the `EDITME` placeholder never goes out.

**The list address is `u-boot@lists.u-boot-project.org`.** The older
`u-boot@lists.denx.de` is out of date; `get_maintainer.pl` reports the current
one.

#### Commit-message hygiene — what checkpatch does and does not see

`scripts/checkpatch.pl` checks the **commit message** as well as the diff, but
only when you give it something that contains one:

```bash
scripts/checkpatch.pl --strict -g HEAD            # checks message + diff
git format-patch -1 --stdout HEAD | scripts/checkpatch.pl --strict -
scripts/checkpatch.pl --strict some.diff          # diff ONLY - message unchecked
```

Running it on a bare `git diff` file reports "no obvious style problems" while
silently skipping every message check. That hid two defects in the
`ide_wait_drq()` commit until it was re-run with `-g`:

- **Wrap the body at 75 characters.** Anything longer earns
  `Prefer a maximum 75 chars per line (possible unwrapped commit description?)`.
- **Only seven signature tags are recognised** — `Acked-by`, `Co-developed-by`,
  `Reported-by`, `Reviewed-by`, `Signed-off-by`, `Suggested-by`, `Tested-by`.
  Anything else is `Non-standard signature`. Note `Co-developed-by:` is not a
  free substitute: the kernel convention requires a matching `Signed-off-by:`
  from that same person, so using it without one trades a warning for an error.
  Drop non-standard trailers from the version that goes to the list.

**`git commit --amend -F msg` silently drops `Signed-off-by:`** unless you also
pass `-s`, because the trailer lives in the message you just replaced. Always
re-check with `git log -1 --format='%(trailers)'` after rewording — the DCO
assertion is the one thing that must not go missing.

To reword a commit that is not HEAD without interactive rebase:

```bash
git branch backup-pre-reword <branch>          # safety net
git checkout <sha> && git commit --amend -s -F newmsg
git rebase --onto $(git rev-parse HEAD) <sha> <branch>
git diff backup-pre-reword <branch>            # must be empty: messages only
```

#### Sending

1. Create a Gmail App Password at `myaccount.google.com/apppasswords`
   (requires 2FA on the account; blocked if Advanced Protection is on).
2. Mail it to yourself first, check it is not whitespace-mangled, then send:

```bash
cd ~/Projects/u-boot-ide-fix
b4 send --reflect --no-sign     # to yourself; prompts for the password
b4 send --no-sign
```

**`--reflect` keeps the real `To:`/`Cc:` headers** and rewrites only the SMTP
envelope, so the copy in your inbox lists Simon Glass and the list even though
delivery was to you alone. That is the point — it shows what the real send will
look like. Nothing leaks to the list.

Confirm the reflected copy survived Gmail byte-intact before the real send —
this is the check that matters, and identical tree hashes prove it where eyeballing
the diff does not:

```bash
git checkout -b am-test upstream/main
git am ~/Downloads/<saved>.eml          # must apply cleanly
git rev-parse am-test^{tree} ide-strlcpy-fix^{tree}   # must match
git checkout ide-strlcpy-fix && git branch -D am-test
```

The commit SHAs *will* differ — `git am` makes a new commit with a new
committer timestamp. Tree hash and patch-id are what must match.

`--no-sign` is required **every time**. With no SMTP settings b4 defaults to
the kernel.org **web endpoint**, which mandates a patatt signature and aborts
with `patatt.signingkey is not set`; `--no-sign` plus the configured SMTP
settings switches it to plain SMTP. (`patatt genkey` +
`b4 send --web-auth-new` is the alternative, and then needs no SMTP at all.)

The non-secret SMTP settings are already in `~/.gitconfig` —
`sendemail.smtpserver` = `smtp.gmail.com`, port 587, tls, and `smtpuser`.

**b4 prompts for the app password on every send, and never stores it.**
`git_credential_fill()` runs `git credential fill` and returns the password,
but never calls `git credential approve` — so `credential.helper = store` never
gets the chance to save it, and nothing lands in `~/.git-credentials`. Retyping
16 characters blind at a `getpass` prompt is the most likely source of a
`535 5.7.8 Username and Password not accepted`: Google displays the password as
`abcd efgh ijkl mnop`, and pasting it *with* the spaces gives 19 characters and
a rejection. To store it deliberately and stop the prompting (plaintext, mode
0600):

```bash
 printf 'protocol=smtp\nhost=smtp.gmail.com:587\nusername=graeme.harker@gmail.com\npassword=APPPASSWORD\n' \
  | git credential approve
```

(Leading space keeps it out of `~/.bash_history`.)

**Type the app password without its spaces.** Google displays it as
`abcd efgh ijkl mnop`; the spaces are visual grouping, not part of the secret.
b4 passes whatever you type **verbatim** to `smtp.login()` — nothing strips it —
so including them sends a 19-character password and earns
`535 5.7.8 Username and Password not accepted`, which reads exactly like a
revoked or wrong password. This cost several failed sends. A quick way to tell
the two apart, since a `getpass` prompt shows nothing:

```python
import getpass, smtplib
pw = getpass.getpass().replace(" ", "")
print(len(pw))                      # must be 16
s = smtplib.SMTP("smtp.gmail.com", 587); s.starttls()
s.login("graeme.harker@gmail.com", pw)   # no mail sent
```

If that succeeds but `b4 send` still fails, the password is fine and the spaces
are the difference.

**Gotcha:** `scripts/get_maintainer.pl` gives a polluted answer when run inside
this fork — it reports *you* as a contributor, because local commits touch
`ide.c`. Run it from the worktree. It also wants a patch file, not a revision
range:

```bash
git format-patch -1 -o /tmp && scripts/get_maintainer.pl --norolestats /tmp/0001-*.patch
```

**Afterwards.** Track it at `patchwork.ozlabs.org/project/uboot/`. Expect a
wait; U-Boot batches through merge windows and `-rc` periods take fixes only —
which is fine, this *is* a fix. This one took 17 days from sending to being
committed (2026-08-29 → 2026-09-15), inside the v2026.10 cycle.

Once a patch lands, **do not drop the local copy straight away** — only once
`sparky` is rebased onto a tag containing it, or the bug comes back in the
meantime. Then remove the worktree and branch, keeping the `sent/` tag. If a
transient Patchwork API call returns an empty body, retry before concluding
anything; it did once on 2026-10-11.

## Memory map

Decoded from **A25/A24** into four 16 MB quadrants, repeating every 64 MB
(A26–A31 are not decoded). **SRAM is at 0 and the NVRAM above it** since
2026-10-11 — busdecoder revision 03 in the hardware project, U-Boot
`f2f34cafe7b` on `sparky` — because Linux wants RAM at 0. That commit was made
on a branch of its own, `sparky1-sram-at-0`, since deleted; older notes name
it.

| Region | Base | Chip select | U-Boot setting |
|--------|------|-------------|----------------|
| SRAM | `0x00000000` | `/RAMCS` | `CFG_SYS_SDRAM_BASE`, `CFG_SYS_INIT_RAM_ADDR` |
| NVRAM (boot image) | `0x01000000` | `/ROMCS` | `CONFIG_TEXT_BASE`, `CFG_SYS_FLASH_BASE` |
| XR68C681 DUART | `0x02000000` | `/UARTCS` | `reg` in `sparky1.dts`, `CONFIG_DEBUG_UART_BASE` |
| CompactFlash | `0x03000000` | `/DISKCS` | `CONFIG_SYS_ATA_BASE_ADDR` |

**Checked on the board 2026-10-11** (build `8a8f9e327f89`, card
`SDCFXPS-064G`): boots to the prompt and probes the CF, from a cold power-on
(6/6), the RESET button and the programmer's `/RESET` pulse; `time sleep 1`
gives 1.000 s, so the timer interrupt works through the RAM vector table at 0;
the INIT_RAM poison test is clean (16128 longwords); the D-cache invalidation
and I-cache toggle tests pass; LBA 0 reads `55aa`; `part list` and `load` to
`${loadaddr}` work; `reset` comes back through a full boot (6/6). No CF writes
were tested.

**The NVRAM decodes only A18–A0**, so it repeats every 512 KB through its
16 MB quadrant, and **the CPU can write it**. Anything that writes into
`0x01xxxxxx` — a mistyped `mw` or an old-map address — lands in the boot image.
That happened once during the move: an `mw.l 0x013f0000` meant for bank B
wrote NVRAM offset `0x70000`. It missed U-Boot, which ends at about
`0x3d8b7`. Check the image with `crc32 0x01000000 <len>` against the file.

**Older logs and notes use older maps:**

| Until | SRAM | NVRAM | DUART | CF |
|-------|------|-------|-------|----|
| 2026-10-10 (A31/A30 decode) | `0x40000000` | `0x00000000` | `0x80000000` | `0xC0000000` |
| 2026-10-11 (A25/A24, before the swap) | `0x01000000` | `0x00000000` | `0x02000000` | `0x03000000` |

None of the old addresses alias their old device on today's board: the
A31/A30 ones all decode as SRAM now (A25 = A24 = 0), and `0x01xxxxxx` is the
NVRAM. An image built for either older map does not boot. Transcripts quoted
below keep the addresses they were taken with.

**All 8 MB is configured** (`CFG_SYS_SDRAM_SIZE` `0x00800000`). The SRAM is two
banks of four 1M × 8 devices selected by A22: bank A `0x00000000`–`0x003FFFFF`,
bank B `0x00400000`–`0x007FFFFF`. U-Boot relocates to the top of bank B, so it
runs entirely from bank B. (On 2026-10-11, at `0x01000000`, the banks were shown
not to alias and a 128 KB copy across the boundary compared clean; the swap
changed only which quadrant selects them.)

### Booting from the NVRAM with RAM at 0 — `/BOOT`

The 68030 fetches its reset SSP from `0x00000000` and its PC from
`0x00000004`, which is now SRAM, uninitialised at power-on. The `boot` GAL in
the hardware project (`plds/boot/`) asserts `/BOOT` through `/RESET` and for
the first 8 **completed** bus cycles after it — the two vectors, a byte at a
time from the 8-bit NVRAM — and while it is asserted busdecoder sends the
bottom quadrant to the NVRAM instead of the SRAM. The reset PC in the image is
`0x01000400`, in the NVRAM's own quadrant, so execution carries on from the
NVRAM once `/BOOT` releases.

Three things on the U-Boot side follow from it:

- **`start.S` points VBR at the image's own table** (`0x01000000`) before
  anything else. VBR is 0 after reset, which is now blank SRAM, so without
  this an exception before `trap_init()` would vector through garbage.
- **`reset` runs the `RESET` instruction from the NVRAM**, not from the
  relocated copy: `/RESET` re-arms `/BOOT`, which would send U-Boot's own next
  8 cycles in the SRAM quadrant to the NVRAM. `do_reset()` jumps to
  `reset_from_rom()` in `start.S` at its link-time address.
- **`do_reset()` copies the reset vectors into RAM at 0** first, into the two
  slots `trap_init()` leaves unused. See the next point for why.

**Two ways the 8-cycle count goes wrong, both seen on the board:**

1. **A floating `/AS` at power-on — fixed in hardware.** The CPU three-states
   `/AS` during reset, and with nothing holding it high it floated low; the
   GAL saw a bus cycle in progress, then counted it as ended when the CPU
   began driving `/AS`. `/BOOT` released after 7 real fetches, the 8th byte of
   the reset PC came from blank SRAM, and the CPU ran wild — sometimes writing
   into the NVRAM. Every cold power-on failed (3/3) while every `/RESET`
   pulse worked, because then `/AS` had been driven high beforehand. **A
   4.7 kΩ pull-up on `/AS` fixed it** (6/6 cold boots since), and it is a
   requirement of the board, not an option: the logic analyser showed exactly
   8 NVRAM fetches before `/BOOT` releases with it fitted.
2. **The CPU was reset as a `RESET` instruction's pulse ended — a slow
   `/RESET` rise, fixed in hardware with a 1 kΩ pull-up, and also handled
   in software.** The instruction itself does not reset a 68030: *"A reset signal
   from the processor (asserted as part of the RESET instruction) resets
   external devices only; the internal state of the processor is not
   altered"* (MC68030 User's Manual, in `~/Projects/datasheets`). But the
   same manual (§7.8) adds that an external `/RESET` which *"extend[s] beyond
   the reset period of the instruction by at least eight clock cycles"*
   resets the processor — and on this board the CPU fetches its reset
   vectors again as the pulse ends (seen with probes on A24 and R/W). So
   `/RESET`, as the CPU's input sees it, stays low at least 8 clocks (500 ns)
   after the CPU lets go. The reason was a slow rise: the net had no
   discrete pull-up on the schematic, only the supervisor's — 5 kΩ inside an
   MCP130 (the datasheet in the repo), none in the MCP120 the schematic
   names — against the capacitance of the CPU, DUART, button, Teensy and
   breadboard wiring; the analyser saw about 300 ns of ringing at its lower
   threshold.

   **Measured 2026-10-11 on the Rigol** (CH1 on the CPU's `/RESET` pin,
   U-Boot `reset`, trace smoothed over 42 ns): an RC-shaped rise from
   0.06 V to 4.89 V, time constant about 0.6 µs — about 5 kΩ against
   roughly 120 pF. From the moment the CPU lets go it reaches 0.8 V (the
   68030's guaranteed low) after 125 ns, 2 clocks; 2.0 V (guaranteed high)
   after 365 ns, 5.8 clocks; 2.4 V after 480 ns, 7.7 clocks; 90% after
   1.4 µs. The line also carries about ±0.3 V of noise, so the band where
   the input could read either way stretches to about 7–8 clocks.
   Borderline against the manual's eight clocks on its own, so it was
   tested directly.

   **Confirmed with a 1 kΩ pull-up on `/RESET`, fitted the same day.** The
   rise now reaches 0.8 V after 20 ns, **2.0 V after 70 ns (1.1 clocks)**
   and 90% after 285 ns. The build from *before* the vector-copy fix
   (`b2d0fd32782c`), whose `reset` hung every time, then passed **6/6**,
   with RAM at 0–7 zeroed before each attempt so leftover vectors could not
   rescue it. So the CPU no longer resets: it carries on from the `RESET`
   instruction to `jmp _start`, as the manual says. **The 1 kΩ on `/RESET`
   is part of the board now**, like the 4.7 kΩ on `/AS`.

   While it happened, the bus cycle in flight as `/RESET` released was
   counted as one of the 8, so again the last byte of the reset PC came from
   the SRAM, and `reset` hung after `resetting ...`. `do_reset()` still
   copies the boot image's vectors to RAM at 0 first, so that byte is right
   wherever it comes from — kept as a safeguard, since a slow `/RESET` is
   easy to bring back by moving wiring or adding load to the net.

The lesson for anything that pulses `/RESET` from software: **make RAM at 0
hold the same reset vectors as the NVRAM**, so the exact cycle on which
`/BOOT` releases cannot matter.

### How U-Boot uses the SRAM

U-Boot takes both ends of the SRAM: the vector table at the bottom, and its
relocated code, heap and stack at the top. The load address sits in between,
with room below it for a kernel to unpack into. With 8 MB configured:

```
0x00800000  ┐ end of SRAM
0x007c2000  │ U-Boot: relocated code                 (relocaddr)
            │ heap (256 KB), board info, device tree
0x00780bd8  │   boot_params
0x0077eaa0  │   fdt_blob
            │ gd and the stack, growing down
            ┘
            │ free: room for the uImage file (~3.5 MB)
0x00400000  ← CONFIG_SYS_LOAD_ADDR: uImage file lands here; start of bank B
            │ free: where bootm unpacks the kernel (4 MB less 4 KB)
0x00001000  ← kernel run address (mkimage -a)
0x00010000    end of INIT_RAM (pre-relocation only, free afterwards)
0x00000000  ← RAM vector table (1 KB), start of SRAM
```

**Measured with `bdinfo` on 2026-10-11** (build `8a8f9e327f89`): `relocaddr`
`0x007c2000`, `boot_params` `0x00780bd8`, `fdt_blob` `0x0077eaa0` — each
exactly 16 MB below its value before the swap, so the layout inside the SRAM
is unchanged. `bdinfo` reports neither the heap's bounds nor the stack pointer
on this port, so only those three are measured; treat the free space above
`0x00400000` as about 3.5 MB less whatever the stack uses. `relocaddr` moves
with the image size, so recheck after big changes.

**`CONFIG_SYS_LOAD_ADDR` is where the file lands, not where the kernel runs.**
`load`/`fatload` with no address argument use `$loadaddr`, falling back to
`CONFIG_SYS_LOAD_ADDR` (`fs/fs.c`); `bootm` with no argument looks there for
the image (`boot/image-board.c`). `bootm` then unpacks the kernel to the load
address in the uImage *header*, set by `mkimage -a`, and jumps to `mkimage -e`.

| Address | Role | Set by |
|---------|------|--------|
| `0x00400000` | scratch: the raw uImage file | `CONFIG_SYS_LOAD_ADDR` |
| `0x00001000` | where the kernel runs | `mkimage -a 0x00001000 -e 0x00001000` |

The kernel goes at `0x00001000` because classic m68k links at `0x1000` with
`PAGE_OFFSET` 0 — see the Linux boot section of the hardware project's notes.
Physical and virtual addresses now agree, which is what the swap was for.

**Two size limits:**

1. **The uImage file must fit below U-Boot** — about 3.5 MB from
   `0x00400000`. A bigger load runs into the stack and heap. Ample for a
   compressed kernel; an uncompressed uImage is the case to watch.
2. **The unpacked kernel must fit below the file** — 4 MB less 4 KB, from
   `0x00001000` to `0x00400000`. Only the image counts: the kernel's BSS and
   the bootinfo after it may run past `0x00400000`, because by the time either
   is written `bootm` has finished with the file.

Breaking the second fails loudly, not silently. `bootm` overwrites its own
compressed source while unpacking, but `bootm_load_os()` checks for the overlap
afterwards and, for a legacy uImage, refuses to boot (`BOOTM_ERR_OVERLAP`). The
check comes after the damage, so the file must be reloaded before retrying.

**History.** The load address was `0x01200000` until 2026-10-11, which left
the unpacked kernel only about 2 MB; it then moved to `0x01400000`, the start
of bank B, and with the swap to `0x00400000`, still the start of bank B. It
assumes 8 MB; at 4 MB it would be the end of RAM. The test recipes below use
`0x00200000` as an explicit scratch address, not `${loadaddr}`.

## Repository layout (m68k-relevant paths)

| Path | Purpose |
|------|---------|
| `arch/m68k/Kconfig` | CPU family selects and board targets |
| `arch/m68k/cpu/mc68030/` | 68030 CPU family implementation |
| `arch/m68k/include/asm/` | Architecture headers — `immap.h`, `mc68030.h`, etc. |
| `arch/m68k/include/asm/coldfire/uart.h` | ColdFire internal UART register map (upstream, unmodified) |
| `include/xr68c681.h` | XR68C681 DUART register map (`xr68c681_t`) and bit macros |
| `arch/m68k/dts/sparky1.dts` | Device tree for sparky1 |
| `board/sparky/sparky1/` | Board-specific code |
| `configs/sparky1_defconfig` | Build configuration |
| `include/configs/sparky1.h` | Legacy config header |
| `drivers/serial/xr68c681_serial.c` | XR68C681 serial driver |
| `drivers/timer/xr68c681_timer.c` | XR68C681 timer driver |
| `drivers/gpio/mc68681_gpio.c` | XR68C681 GPIO (output port) driver |
| `drivers/block/ide.c` | Generic IDE driver — extended for 8-bit data ports (sparky1 now uses 16-bit) |
| `include/ata.h` | ATA register offsets, computed from `CONFIG_SYS_ATA_*` |
| `drivers/serial/serial_mcf.c` | ColdFire serial driver (uses `<asm/coldfire/uart.h>`) |
| `doc/board/sparky/index.rst` | Sphinx toctree entry for sparky board docs |
| `doc/board/sparky/sparky1.rst` | Hardware overview, build instructions, peripheral support |

## Boot sequence — sparky1

### Phase 0 — start.S (from flash, before relocation and before C)

```
reset vector fetch (0x000000/0x000004, sent to the NVRAM by /BOOT → _start = 0x01000400)
arch/m68k/cpu/mc68030/start.S:_start
  ├─ %sr = 0x2700                                             disable all interrupts
  ├─ %vbr = _vectors (0x01000000)                             the image's own table: 0 is blank SRAM until trap_init()
  ├─ %a5 = __got_start                                        set %a5 to the linker's Global Offset Table in Flash
  ├─ %sp = CFG_SYS_INIT_RAM_ADDR + CFG_SYS_INIT_RAM_SIZE      set %sp to top of the init ram area (0x0001:0000)
  ├─ common/init/board_init.c:board_init_f_alloc_reserve()    reserve space for global_data at top of init ram 
  ├─ common/init/board_init.c:board_init_f_init_reserve()     zero the global_data struct
  ├─ arch/m68k/cpu/mc68030/cpu_init.c:cpu_init_f()
  │    ├─ arch/m68k/lib/cache.c:icache_enable()               invalidate and enable instruction cache
  │    └─ arch/m68k/lib/cache.c:dcache_enable()               invalidate and enable data cache
  └─ common/board_f.c:board_init_f(0)                         
```

### Phase 1 — board_init_f (common/board_f.c, still in flash)

```
common/board_f.c:initcall_run_f()
  ├─ setup_mon_len()                         gd->mon_length = _bss_end - CONFIG_SYS_MONITOR_BASE
  ├─ lib/fdtdec.c:fdtdec_setup()             locate FDT blob
  ├─ trace_early_init()                      CONFIG_TRACE_EARLY
  ├─ initf_malloc()                          pre-reloc malloc arena
  ├─ common/log.c:log_init()
  ├─ initf_bootstage()
  ├─ event_init()
  ├─ arch_cpu_init()                                          no-op (no mc68030 override)
  ├─ initf_dm()
  │    ├─ drivers/core/root.c:dm_init_and_scan(true)          bind pre-reloc DM devices from FDT
  │    │    └─ binds xr68c681 serial, timer, gpio from sparky1.dts
  │    └─ dm_autoprobe()
  ├─ arch/m68k/cpu/mc68030/speed.c:get_clocks()
  │    └─ gd->cpu_clk = CFG_SYS_CLK                           (16 MHz)
  ├─ timer_init()                                             weak no-op (DM timer, not TIMER_EARLY)
  ├─ env/env.c:env_init()
  ├─ init_baud_rate()                                         gd->baudrate = 115200
  ├─ serial_init()                                            probes xr68c681_serial (DM_FLAG_PRE_RELOC)
  │    └─ drivers/serial/xr68c681_serial.c:xr68c681_serial_probe()
  │         └─ xr68c681_serial_init_common()
  │              ├─ RESET_RX/TX/ERROR/MR
  │              ├─ uimr = 0                      mask all interrupts
  │              ├─ uacr = 0xF0                   BRG Set 2, timer mode X1/CLK÷16
  │              ├─ umr = 0x13                    MR1: 8 data bits, no parity
  │              ├─ umr = 0x07                    MR2: 1 stop bit
  │              └─ xr68c681_serial_setbrg_common(115200)
  │                   ├─ SET_RX_EXTEND / SET_TX_EXTEND   (X=1)
  │                   ├─ ucsr = 0x88
  │                   └─ RX_ENABLED | TX_ENABLED
  ├─ common/console.c:console_init_f()
  ├─ lib/display_options.c:display_options()                  
  │    └─ "U-Boot 2026.04 ..."
  ├─ arch/m68k/cpu/mc68030/cpu.c:print_cpuinfo()
  │    └─ "CPU:   Motorola MC68030"
  ├─ board/sparky/sparky1/sparky1.c:show_board_info() → checkboard()
  │    └─ "Board: Sparky1"
  ├─ announce_dram_init()
  │    └─ "DRAM:  "
  ├─ board/sparky/sparky1/sparky1.c:dram_init()
  │    └─ gd->ram_size = CFG_SYS_SDRAM_SIZE
  ├─ setup_dest_addr()                      pick relocation address (top of RAM)
  ├─ reserve_uboot/malloc/board/global_data/fdt/stacks()
  ├─ dram_init_banksize()
  ├─ show_dram_config()
  ├─ setup_bdinfo()
  ├─ reloc_fdt()
  ├─ setup_reloc()
  └─ jump_to_copy() → arch/m68k/cpu/mc68030/start.S:relocate_code()
```

### Relocation — relocate_code (start.S)

```
arch/m68k/cpu/mc68030/start.S:relocate_code(new_sp, gd, dest_addr)
  ├─ copy flash → RAM (longword loop)
  ├─ clear BSS
  ├─ fix GOT table (add relocation offset to every entry)
  └─ jmp board_init_r
```

### Phase 2 — board_init_r (common/board_r.c, running from RAM)

```
common/board_r.c:initcall_run_r()
  ├─ initr_trace() / initr_reloc()
  ├─ initr_reloc_global_data()               fix gd pointers after relocation
  ├─ initr_malloc()                          full post-reloc heap active
  ├─ common/log.c:log_init() / common/board_r.c:initr_bootstage()
  ├─ initr_of_live()                         build live DT tree
  ├─ initr_dm()                              full DM re-init in RAM
  │    ├─ drivers/core/root.c:dm_init_and_scan(false)
  │    └─ dm_autoprobe()
  ├─ initr_lmb()
  ├─ initr_dm_devices()                      no dm_timer_init (TIMER_EARLY not set)
  ├─ stdio_init_tables()
  ├─ drivers/serial/serial-uclass.c:serial_initialize()       re-probe xr68c681_serial in RAM context
  ├─ initr_announce()
  ├─ arch/m68k/lib/traps.c:arch_initr_trap()
  │    └─ trap_init(CFG_SYS_SDRAM_BASE)
  │         ├─ write _exc_handler → vectors 2–24, 32–63 in RAM table
  │         ├─ write _int_handler → vectors 25–31, 64–255 in RAM table
  │         └─ setvbr(CFG_SYS_SDRAM_BASE)      point VBR at RAM vector table
  ├─ power_init_board()                                        weak no-op
  ├─ arch/m68k/cpu/mc68030/cpu_init.c:cpu_init_r()            no-op
  ├─ initr_env()                             load environment variables
  ├─ common/stdio.c:stdio_add_devices()
  ├─ common/console.c:console_init_r()                        full console active
  ├─ arch/m68k/cpu/mc68030/interrupts.c:interrupt_init()
  │    └─ arch/m68k/lib/interrupts.c:enable_interrupts()      %sr interrupt mask → 0
  ├─ drivers/timer/timer-uclass.c:dm_timer_init()             M68K timer_init() path
  │    └─ drivers/timer/xr68c681_timer.c:xr68c681_timer_probe()
  │         ├─ uctu/uctl = 0x04/0x80            N=0x0480 → 100 Hz
  │         ├─ uacr = 0xF0                      BRG Set 2 + timer mode
  │         ├─ ivr = 0x40
  │         ├─ arch/m68k/lib/interrupts.c:irq_install_handler()   register timer_interrupt_handler
  │         └─ uimr = 0x08                      enable counter/timer interrupt
  ├─ board/sparky/sparky1/sparky1.c:board_late_init()
  │    └─ uclass_first_device_err(UCLASS_IDE) → drivers/block/ide.c:ide_probe()
  │         ├─ ide_init_one(0)                 select master, 0xFF check, poll BSY
  │         ├─ ide_ident(0)
  │         │    ├─ ATA_CMD_ID_ATA             IDENTIFY DEVICE
  │         │    └─ ide_input_swap_data()      256 words, each byte-swapped by inw()
  │         └─ blk_create_devicef("ide_blk")   → "ide 0" block device
  └─ common/board_r.c:run_main_loop() → common/main.c:main_loop()
```

**Notes:**
- `TIMER_EARLY` is not set — the timer is not available during `board_init_f`. Any `get_timer()` call before `timer_init` in phase 2 would trigger a lazy `dm_timer_init()`, which would panic as the timer device isn't probed yet pre-relocation.
- `board_early_init_f`, `board_init` and `misc_init_r` are not configured and are skipped. `board_late_init` **is** configured, and exists solely to probe the CompactFlash.
- **The CompactFlash must not be autoprobed.** `dm_autoprobe()` runs inside `initr_dm()`, long before `interrupt_init()` and the M68K `timer_init()` further down `init_sequence_r[]`. The XR68C681 tick counter is only advanced by the timer's 100 Hz interrupt handler, so a controller probed at `initr_dm()` time reaches `ide_probe()`'s `mdelay(100)` with the counter frozen at zero and `__udelay()` spins forever. Probing from `board_late_init()` is what avoids that; do not add `DM_FLAG_PROBE_AFTER_BIND` to the IDE driver.
- The XR68C681 serial driver carries `DM_FLAG_PRE_RELOC` so it is bound and probed before relocation, then re-probed in RAM after relocation.

### Phase 3 — handing over to an OS (`bootm`)

`head.S` in the m68k kernel expects to be entered with interrupts masked and
the caches off. Two hooks make `bootm` do that, both added 2026-10-25:

```
boot/bootm_os.c:boot_selected_os()
  ├─ arch/m68k/cpu/mc68030/cpu_init.c:arch_preboot_os()
  │    ├─ move.w #0x2700,%sr                  mask interrupts to level 7
  │    └─ movec CI|CD → CACR                  invalidate and disable both caches,
  │                                           in one write (EI, ED, FI, FD, IBE,
  │                                           DBE and WA all cleared)
  ├─ board_preboot_os()                       weak no-op
  └─ arch/m68k/lib/bootm.c:do_bootm_linux()
       └─ boot/bootm.c:bootm_final()          "Starting kernel ..."
            └─ dm_remove_devices_active()     devices with DM_FLAG_OS_PREPARE
                 └─ drivers/timer/xr68c681_timer.c:xr68c681_timer_remove()
                      ├─ uimr = 0               mask the timer interrupt at the DUART
                      └─ read uopc              clear any tick already pending
       └─ jump to the entry point
```

**Notes:**
- **Both are needed.** Masking the CPU alone only defers the tick: the DUART
  keeps `/IRQ` asserted, and the kernel would take it through a vector it has not
  set up as soon as it lowered its own mask.
- The data cache is write-through, so turning it off loses nothing; the I-cache
  matters, since `bootm` has just written the kernel into memory it may hold.
- The console stays polled, so `bootm_final()`'s message still prints after the
  mask.
- **`go` runs neither hook**, so it enters with interrupts at level 0, the
  caches on and the timer ticking. Something entered with `go` must clean up
  itself.
- `arch_preboot_os()` was checked on the board with a stand-in that prints SR
  and CACR on entry: `SR=2000 CACR=00000101` with `go`, `SR=2700 CACR=00000000`
  with `bootm`.
- Still wrong for Linux: `do_bootm_linux()` passes arguments the ColdFire way, not
  as the bootinfo chain classic m68k expects. See the hardware project's "Boot
  Linux from the CompactFlash card".

### Exception vector table

The vector table in flash is **never copied to RAM**. Instead, `trap_init()` in `arch/m68k/lib/traps.c` builds a fresh table directly in RAM:

- **Destination:** `CFG_SYS_SDRAM_BASE` = `0x00000000` (base of SDRAM)
- **Contents:** vectors 2–24 and 32–63 → `_exc_handler`; vectors 25–31 and 64–255 → `_int_handler` (all pointing to their relocated RAM addresses). Vectors 0 (initial SP) and 1 (reset PC) are not written by `trap_init()`; `do_reset()` fills them with the boot image's reset vectors before pulsing `/RESET` — see [`/BOOT`](#booting-from-the-nvram-with-ram-at-0--boot).
- **VBR set:** `setvbr(0x00000000)` at the end of `trap_init()`, called from `arch_initr_trap()` during `initcall_run_r()` in phase 2.

Before `setvbr` is called, VBR points at the flash vector table at `0x01000000`, set by `start.S` as its first act, and that table handles any early exceptions. (VBR's reset value, 0, is blank SRAM on this map.)

## Build

```bash
# Configure for sparky1
make sparky1_defconfig

# Build (cross-compiler required)
make CROSS_COMPILE=m68k-linux-gnu- -j$(nproc)
```

The 68030 requires `-mcpu=68030` (not `-mcpu=5307` or any ColdFire flag). Verify the cross-compiler supports classic m68k ISA — ColdFire-only compilers will not work.

## Programming the board

The boot store is a Dallas DS1250Y NVSRAM at `0x01000000`. It is not removed
and flashed in a programmer — it stays soldered and is written **in-circuit**
by the Teensy++ 2.0 in `../nvram-programmer/`, which requests the 68030 bus via
`/BR`/`/BGACK`, DMA-writes the NVRAM, reads it back to verify, releases the
bus and then pulses `/RESET`.

```bash
cd ../nvram-programmer && ./nvram_write ~/Projects/u-boot/u-boot.bin
```

Success prints `Done.` then `Verified.` — the read-back matched, so the
contents are confirmed, not merely written. The board reboots into the new
image on its own; no manual reset.

**`u-boot.bin` is written at the start of the NVRAM, not at 0x400.** The
image is linked at `CONFIG_TEXT_BASE` = `0x01000000` and *starts* with the
exception vector table: offset 0x004 holds `0x01000400`, which is `_start`,
and 0x008 onward hold `_exc_handler`. `CONFIG_SYS_MONITOR_BASE` =
`0x01000400` is where U-Boot's code sits *inside* the image. The programmer
writes from NVRAM offset 0, which is correct. The whole image must fit the
512 KB device (currently ~246 KB, `0x3d8b7` bytes).

**The programmer must drive A24 high** to reach the NVRAM at `0x01000000` —
nvram-programmer branch `a24-high`, uploaded to the Teensy 2026-10-11. By the
time it takes the bus `/BOOT` is long released, so A25 = A24 = 0 would be the
SRAM. **A mismatch is silent**: old firmware on the new map (or new firmware
on busdecoder revision 02) writes the SRAM, reads it back from the SRAM, and
prints `Verified.` with the NVRAM untouched. A changed banner, or
`crc32 0x01000000 <len>` matching the file, is what proves a write landed.

### Gotchas

- **The Teensy is powered from the SBC rail.** If `/dev/ttyACM0` has vanished,
  the board is off — that is the first thing to check when `nvram_write`
  reports `No such file or directory`.
- **The serial console is `/dev/ttyUSB0`** (an FTDI TTL232R), 115200 8N1.
  It is a different device from the programmer, so flashing and console
  capture can run at the same time.
- **Only one process can hold the console.** minicom and any script fight over
  it; `fuser -v /dev/ttyUSB0` names the holder. With minicom closed the board
  can be driven programmatically — send a command, wait for the `=> ` prompt,
  read the reply — which is far quicker than copying dumps out of a terminal
  by hand, and lets output be diffed and decoded directly.
- **minicom with line wrap off silently truncates at column 80.** The `Ser#:`
  field of the IDE probe line starts exactly at column 80, so with wrapping
  disabled only the *last* character of the serial number survives, which
  looks exactly like corrupted IDENTIFY data. Turn wrapping on before reading
  anything into a diagnosis.
- **`reset` reboots the board** — implemented 2026-09-01. It was a stub until
  then, returning to the prompt having done nothing, so older notes saying a
  reboot needs a reflash are out of date. It prints `resetting ...` and comes
  back through the full boot. Start the console capture *before* sending it, or
  the boot log is missed — a capture that opens the port and then flushes will
  throw the banner away and the board will look dead. On the SRAM-at-0 map it
  hung after `resetting ...` until `do_reset()` learned to copy the reset
  vectors into RAM at 0 (`8a8f9e327f8`); see
  [`/BOOT`](#booting-from-the-nvram-with-ram-at-0--boot).
- **Cold boots are safe again, but only with the pull-up on `/AS`.** Without
  it every power-on failed and the runaway CPU could write into the NVRAM. If
  the board is ever silent after a power-on but boots from the RESET button,
  suspect that pull-up first, and checksum the NVRAM before trusting it.
- **The console cable is powered from USB, the Teensy from the board.**
  `/dev/ttyUSB0` stays put across a board power cycle; `/dev/ttyACM0`
  disappearing and reappearing is what marks one. A script that waits for the
  console to re-enumerate after power-on will wait forever.
- **`reset` pulses /RESET, `ide reset` does not** — `ide reset` only issues
  SRST (`CONFIG_IDE_SRST`). The card's `-RESET` (CF
  pin 41) is on the board's `/RESET` net (the hardware project's CF connector
  table), so `reset` hard-resets the card too and is a far better proxy for a
  cold boot. The `RESET` instruction takes 518 clocks (MC68030 User's Manual,
  instruction timing table), nearly all of them with the line driven — ~32 µs
  at 16 MHz, against the card's 10 µs minimum reset width (SanDisk
  manual, `Tw (RESET)`).
- The `/RESET` pulse `nvram_write` issues after a successful write still works
  and is the only option if the board is wedged past the console.
- **A CF card a test left busy may need a power cycle.** On 2026-10-11, after
  a hardware-project test, U-Boot's probe printed `Bus 0: ....` indefinitely —
  the card reporting BSY — through the programmer's `/RESET` pulse; a power
  cycle cleared it. The card's `-RESET` is on that net (and was then — the
  adapter with that wiring went in on 2026-10-09), so a hardware reset did
  not clear it, and an SRST through device control probably would not either.
- **Do not swap the CF card with the power on.** A hot insert put the board
  into a continuous `Bogus External Interrupt Vector 144` storm, and NVRAM
  reads through the programmer went unstable — three reads of the same KB
  differing — because the card was contending for the bus. Power down, swap,
  power up.

## Testing

### Cache verification

`CONFIG_CMD_CACHE=y` and `CONFIG_CMD_TIME=y` are both set in `configs/sparky1_defconfig`. The XR68C681 timer runs at 100 Hz (10 ms resolution), so each timed operation should run long enough to register — at 16 MHz the 256 KB blocks below typically take several hundred milliseconds.

**Functional test** — verifies data integrity through enable/disable cycles and that `dcache_invalid()` fires correctly on re-enable:

```
# Verify both caches report enabled at boot
icache
dcache

# Write with D-cache ON, disable, read back — write-through means memory is always current
mw.l 0x00200000 0xdeadbeef 0x40
md.l 0x00200000 8
dcache off
md.l 0x00200000 8
# Both reads must show 0xdeadbeef

# Write new pattern with cache OFF, re-enable — dcache_enable() calls dcache_invalid()
# so re-enable must flush any stale lines; if this reads 0xdeadbeef the invalidation is broken
mw.l 0x00200000 0xcafebabe 0x40
dcache on
md.l 0x00200000 8
# Must show 0xcafebabe

# I-cache toggle — a hang or reset here means pipeline state was corrupted on re-enable
icache off
icache on
icache
dcache
```

**Timing test** — compares raw memory throughput with caches off vs on. `mw` exercises the write path; `cmp` reads 128 KB twice (src + dst) with minimal serial output:

```
# Pre-fill both regions with matching data so cmp won't complain
mw.l 0x00100000 0xdeadbeef 0x10000
mw.l 0x00110000 0xdeadbeef 0x10000

echo "--- caches OFF ---"
icache off
dcache off
time mw.l 0x00100000 0x12345678 0x10000
time cmp.l 0x00100000 0x00110000 0x8000

echo "--- caches ON ---"
icache on
dcache on
time mw.l 0x00100000 0x12345678 0x10000
time cmp.l 0x00100000 0x00110000 0x8000
```

The scratch addresses (0x00100000 / 0x00110000) sit in bank A of the 8 MB SRAM (bank A `0x00000000`–`0x003FFFFF`), above INIT_RAM and well below where U-Boot relocates to near the top. If both timed results round to zero, increase the count to `0x40000` (1 MB). The D-cache speedup is most visible on the `cmp` (read-heavy) operation; `mw` speedup is smaller because the 68030 D-cache is write-through.

### CompactFlash verification

The controller is probed during `board_late_init()`, so a card present at reset
is reported in the boot log:

```
Bus 0: OK
  Device 0: Model: SanDisk SDCFB-128 Firm: HDX 4.02 Ser#: ...
            Type: Hard Disk
            Capacity: 122.5 MB = 0.1 GB (250880 x 512)
```

`Bus 0: not available` means the status register read back `0xFF` — an empty
socket, or `/DISKCS` not reaching the card.

#### Moot since the 16-bit port — 8-bit mode intermittently refused at cold boot

The 16-bit port never sends `SET FEATURES 0x01`, so this cannot happen now.
The diagnosis is kept because the card state it points at — probed while still
running its power-on self-test — could still affect IDENTIFY.

Seen 2026-08-30 on a SanDisk `SDCFXPS-032G`:

```
Bus 0: OK
  Device 0: 8-bit transfer mode rejected (status 0x6b, error 0x40)
not available
```

When it bites the card is unusable for the whole session. It is **specific to
cold boot** — 0 out of 30 warm `ide reset` re-probes reproduced it, and an
`ide reset` afterwards brings the card up fine.

The status/error pair says this is not a card politely declining. A device
that does not support 8-bit PIO answers status `0x51` (`DRDY|DSC|ERR`) with
error `0x04` (`ABRT`). Here status `0x6b` is `DRDY|DF|DRQ|IDX|ERR` and error
`0x40` is `UNC` — `DRQ` set after a *non-data* command, plus device fault and
an uncorrectable-data error, is incoherent. That is what a card still running
its power-on self-test looks like when probed too early, and `ide_init_one()`
has no settle delay before `ide_ident()` issues `SET FEATURES`.

Warm re-probing being clean is what rules out marginal signalling: a bus
problem would show up warm too. The likely fix is a settle delay, or retrying
`SET FEATURES` once, before declaring the card unusable.

Then, at the prompt:

```
ide info                          # re-report what was found
part list ide 0                   # partition table
ls ide 0:1 /                      # FAT directory listing
load ide 0:1 0x00200000 /file     # read a file into SDRAM
```

#### Read/write integrity

`ide read` and `ide write` take **all three arguments in hex** — address,
block number and block count — and operate on the current device, which
`ide device` selects (it defaults to 0).

    ide read  addr blk# cnt
    ide write addr blk# cnt

The tests below use two 4 KB buffers at `0x00100000` and `0x00110000`. Those
sit in bank A of SRAM (`0x00000000`–`0x003FFFFF`), well above INIT_RAM
and well below where U-Boot relocates to, so nothing live is at risk. Sizes
line up as: 8 sectors × 512 B = 4096 B = `0x1000` bytes = `0x400` longwords,
which is the count `mw.l` and `cmp.l` take.

**These tests overwrite the card.** LBA `0x1000` is used throughout — far
enough in to miss the partition table, but if the card holds anything you
care about, pick an LBA past the end of the last partition instead.

Single sector first, since a fault there is easier to read than one buried in
a multi-sector burst:

```
ide device 0
mw.l 0x00100000 0xdeadbeef 0x80     # 0x80 longwords = 512 B = one sector
mw.l 0x00110000 0x00000000 0x80     # clear dest: a failed read must not pass
ide write 0x00100000 0x1000 0x1
ide read  0x00110000 0x1000 0x1
cmp.l 0x00100000 0x00110000 0x80
```

Then 8 sectors in one command. This is worth doing separately: the card
re-asserts DRQ for each sector without being re-commanded, so it exercises
the inter-sector turnaround that the single-sector path never reaches.

```
mw.l 0x00100000 0xdeadbeef 0x400
mw.l 0x00110000 0x00000000 0x400
ide write 0x00100000 0x1000 0x8
ide read  0x00110000 0x1000 0x8
cmp.l 0x00100000 0x00110000 0x400
```

A pass prints `Total of 1024 word(s) were the same` — `cmp` calls a `.l`
object a "word" regardless of the suffix, so that is 1024 longwords, not
1024 16-bit words. A failure names both addresses and both values:

```
word at 0x00100000 (0xdeadbeef) != word at 0x00110000 (0x00000000)
```

`0xdeadbeef` is a poor pattern for spotting a sector landing at the wrong
offset, since every longword is identical. For a non-repeating pattern,
copy 4 KB of U-Boot's own image out of NVRAM instead of filling a constant —
it is varied, always present, and read-only as far as this test goes (the CPU
*can* write the NVRAM, so never swap the operands):

```
cp.l 0x01001000 0x00100000 0x400
mw.l 0x00110000 0x00000000 0x400
ide write 0x00100000 0x1000 0x8
ide read  0x00110000 0x1000 0x8
cmp.l 0x00100000 0x00110000 0x400
```

**What a round-trip cannot catch.** Writing and reading through the same
driver hides any fault that is symmetric. If `ide_output_data()` and
`ide_input_data()` both transposed byte pairs, every `cmp` above would still
pass while every sector on the card was scrambled for anything else that
reads it. Confirming the ordering needs data this board did not write.

The cheapest source is a card partitioned or formatted on a PC: LBA 0 ends
with the boot signature `0x55 0xAA` at offset `0x1FE`.

```
ide read 0x00100000 0x0 0x1
md.w 0x001001fe 1
```

On big-endian m68k the byte at the lower address is the high half, so a
correct data path shows `0x55aa`. `0xaa55` means every byte pair is
transposed — fix `ide_input_data()`, do not "correct" it downstream. If the
card has no partition table at all this check says nothing; fall back to
running the hardware project's `tests/cf_test/` and reading a sector it
wrote.

#### The write path used to duplicate a byte — fixed 2026-08-30

**Fixed**, but the signature is kept here because it is distinctive and one
unexplained event did occur after the fix. Roughly 5–8% of written sectors used
to come back with one byte duplicated and the rest of the sector shifted one
byte later, losing the final byte:

```
fd 4c d0 81 2f 00 2d 40 ...   source
fd 4c d0 d0 81 2f 00 2d ...   read back   (d0 duplicated)
```

The offset moved from run to run and was never tied to a sector boundary, so it
happened inside `ide_output_data()`'s 512-byte loop. Reads were always clean.

**The cause is signal integrity on the CF data lines and the cure was
hardware** — 33R series termination resistors at the IDE connector. It was not
a driver bug, and the `udelay()` change was not responsible (5/10 failures
pre-`udelay`, 3/10 after). Since fitting them, **39,824,384 sectors have been
written, read back and compared with exactly one corrupt sector**, and that one
came in the first few thousand. It looked like this, and two independent
read-backs agreed, so the damage was on the card, not in the read path:

```
SRC: 02 82 00 00 | 00 ff 72 02 b2 8c 66 00 ...
DST: 02 82 00 00 | 00 00 ff 72 02 b2 8c 66 ...   <- 00 duplicated, rest shifted
```

**Judging a change here needs volume.** A 10-trial 8-sector run is only 80
sectors and can say nothing about a rate below a few percent — the two clean
10-trial runs that first suggested "fixed" were far too small to mean it. Use
`soak.py`-style runs of millions of sectors, and state the bound the sample
actually supports rather than the point estimate.

That mistake is worth naming: a single corruption event was written up here as
a rate of "1 in 46,736", and 40 million sectors later it is bounded below
7.5e-08. One event is not a rate. The same trap caught the reboot count in the
open section below, where 24 inferred resets turned out to be one measured one.

**Why it went unnoticed for so long.** The one thing the write path had been
exercised by is formatting a card from the SBC — and that writes **almost
entirely zeros** (the metadata region is blasted with a zero buffer). A
duplicated byte inside a run of zeros is invisible: the sector is still all
zeros. Only the eight content sectors carry non-zero data, and they are
themselves mostly zeros. So a format could succeed, and Linux could mount the
result, while the write path was this broken.

#### Testing writes from a script — two traps that fake a clean run

Both of these produced false PASSES on 2026-08-30 and cost two whole 10-trial
runs before they were spotted. Anything that reports pass/fail counts from this
board needs to defend against them.

1. **The console drops characters.** Blasting a command at 115200 with no flow
   control makes the 16 MHz 68030 lose bytes mid-line — `mw.l 0x01180000 0
   0x20000` arrived as `mw.` and U-Boot answered with a usage message, leaving
   the buffer unpoisoned. (That was SRAM at the time; on today's map `0x01xxxxxx`
   is the NVRAM, so do not reuse the address.) Send commands character by character (~1.5 ms apart)
   and **verify the echoed first line matches what was sent**, retrying if not.
   A stray `> ` continuation prompt, left by an unterminated quote, will also
   hang any driver waiting for `=> `; send Ctrl-C first to resync.
2. **`ide write` failures are silent unless you parse them.** It prints
   `N blocks written: OK|ERROR`, and a partial write still returns a prompt. A
   `cmp` round-trip then *passes* if an earlier trial happened to write the same
   data to the same LBA. Always check the OK/ERROR line, **vary the source data
   between trials**, poison the destination buffer before reading back, and use
   a fresh LBA per trial so stale card contents cannot stand in for a write that
   never happened.

Also give the reader a long idle window: `ide write` and `ide read` go silent
for seconds while they work, so a 0.6 s idle timeout will abandon the command
mid-flight and desynchronise the stream. Wait for the prompt, not for silence.

A useful trick for sensitivity: parse the `N blocks written` count and read back
exactly those N blocks. A trial that stalls part-way still contributes N
verified sectors, which is how 4,592 sectors were checked while every 1024-block
write was still aborting.

#### A scrambled model string does not mean a driver bug

The obvious reading of a mangled `ide info` line is transposed byte pairs in
`ide_input_swap_data()`. **That reading cost most of a day and was wrong.** A
counterfeit SanDisk Extreme Pro 64 GB reported model `FSPC-X6SG0 4`, a
capacity of 117440519 sectors against a true 125059072, and its LBA28 capacity
words one slot early at words 59/60 — while sector reads from the same card
were byte-perfect. Neither a byte-ordering change, nor removing INITIALIZE
DEVICE PARAMETERS, nor enabling LBA48, nor padding every port read with ~1 µs
of NOPs altered a single byte of its IDENTIFY response. The card's firmware
was simply lying.

Two tells, in hindsight: a genuine SanDisk puts the vendor in the model string
(`SanDisk SDCFX-008G`), and the counterfeit's bare `SDCFXPS-064G` did not; and
its CF-specific words 7/8 held the correct total while words 60/61 did not.

**So do not debug the driver from IDENTIFY output.** In order of cost:

1. **Try a second card.** This is the fastest discriminator by a wide margin
   and needs one power cycle. A genuine card puts every landmark in its
   standard place — words 54–56 mirroring words 1/3/6, word 64 = `0x0003`,
   words 65–68 = `0x0078`, and LBA28, LBA48 and words 7/8 all agreeing.
2. **Check the sector path separately**, against a sector the board did not
   write. IDENTIFY and sector data are deliberately asymmetric — one is
   byte-swapped, the other is not — so one can be wrong while the other is
   right, and a fault in either is invisible to a round-trip through this
   driver alone.
3. Only then suspect `ide_input_swap_data()`.

#### Formatting a card from the SBC

U-Boot has no `mkfs`, and this defconfig has no `mbr`, `gpt`, `loadb` or FAT
write support either. A card can still be formatted entirely from the board,
because a fresh FAT32 volume is almost all zeros — for an 8 GB card at 32 KB
clusters only **eight** sectors carry data:

| disk LBA | contents |
|----------|----------|
| 0 | MBR |
| 2048 / 2049 | boot sector / FSInfo |
| 2054 / 2055 | their backups |
| 2112 / 4032 | first sector of FAT1 / FAT2 (`F8FFFF0F FFFFFF0F F8FFFF0F`) |
| 5952 | root directory cluster (volume label) |

Everything from LBA 2048 to 6015 is zeros. So:

1. On the host, generate the structures with the real tools rather than
   computing FAT arithmetic by hand — `sfdisk` on a sparse full-disk image for
   the MBR, `mkfs.vfat -F 32 -s 64` on a sparse partition-sized image for the
   filesystem. Force the cluster size: left to itself `mkfs.vfat` picks 4 KB
   clusters, which makes each FAT 15256 sectors and the write take five
   minutes instead of forty seconds.
2. Zero a 256 KB buffer with `mw.l <buf> 0 0x10000` and blast it over the
   metadata region with a handful of `ide write <buf> <lba> 0x200`.
3. Build each content sector in RAM — `mw.l <buf> 0 0x80`, then one `mw.l` per
   non-zero longword (about 130 in total across all eight) — and `ide write`
   it to its LBA. Read the source longwords **big-endian** so the values
   reproduce the on-disk byte order through m68k's `mw.l`.

Keep the buffer well below `relocaddr` (check `bdinfo`; it was
`0x007c2000` with 8 MB on the SRAM-at-0 map), so `0x00100000` is safe. The whole run is ~157 commands and takes about two
minutes, dominated by the metadata zeroing.

This is also the only real test of the **write** path: format from the SBC,
then confirm Linux mounts the result read-write and can allocate clusters in
it. A round-trip through this driver alone cannot detect a symmetric fault.

### INIT_RAM relocation verification

After relocation, U-Boot must use only the relocated `gd`, stack, and heap in the upper SDRAM — nothing should touch INIT_RAM (0x00000000–0x0000FFFF, the bottom 64 KB of SDRAM) again.

**Important:** the exception vector table lives at `0x00000000–0x000003FF` (256 vectors × 4 bytes). Do **not** poison that region — any exception during the test would vector to the poison value and hard-crash. Start poisoning at `0x00000400` (the first byte after the table). The usable poison range is therefore `0x00000400–0x0000FFFF` = 63 KB − 0 = 0x3F00 longwords.

Poison from `0x00000400` with a distinctive value, write the same pattern to a reference area above it, exercise U-Boot, then `cmp` the two regions. Any address reported by `cmp` is a location that was written after poisoning — a stale pointer or residual stack/heap use.

```
# Poison INIT_RAM, skipping the vector table at 0x00000000–0x000003FF
# 0x3F00 longwords = 63 KB starting at 0x00000400
mw.l 0x00000400 0xdeadc0de 0x3f00

# Write same pattern to reference area (above INIT_RAM, below the 0x00200000 scratch area)
mw.l 0x00020400 0xdeadc0de 0x3f00

# Confirm relocated gd, sp, and heap are all up near 0x007xxxxx — not in INIT_RAM
bdinfo

# Exercise U-Boot to trigger any residual INIT_RAM access
printenv
dm tree
md.l 0x00000400 4

# Compare — "Total of 16128 word(s) were the same" means INIT_RAM is clean
# (cmp calls a .l object a "word"; that is 16128 longwords)
# Any reported address means something still touched INIT_RAM after relocation
cmp.l 0x00000400 0x00020400 0x3f00
```

If `cmp` flags an address, the offset from `0x00000400` locates which part of INIT_RAM was touched: offsets near zero suggest a stale heap or BSS pointer; offsets near `0xBBFC` (= `0xF000 - 0x400`) suggest a leftover stack reference (the initial stack and `gd` struct were reserved at the top of INIT_RAM by `board_init_f_alloc_reserve()`).

Run `bdinfo` before poisoning to confirm that `gd`, `sp`, and the malloc arena are all well above `0x0000FFFF`. If any of them appear in the `0x00000xxx` range, relocation did not complete correctly and poisoning would corrupt live data.

## Key differences: classic m68k vs ColdFire

| Feature | Classic 68030 | ColdFire (MCF530x, etc.) |
|---------|--------------|--------------------------|
| ISA | Full m68k (including MOVES, BFINS, etc.) | Reduced ColdFire ISA subset |
| MMU | External PMMU (68851 integrated in 68030) | Optional in some MCF variants |
| Cache | Separate I/D caches with %cacr | Unified cache, different %cacr layout |
| Stack alignment | 2-byte word | 4-byte long word (ColdFire ABI) |
| `start.S` cache init | %cacr/%caar registers | ColdFire-specific cache ops |
| Exception stack frame | Variable-length with format/vector word | Fixed 8-byte frame |

### Exception stack frame differences

**Classic 68030** uses variable-length frames. All frames begin with the same 8-byte base (from low to high address at %sp):

```
%sp+0  old %sr
%sp+2  old PC (high word)
%sp+4  old PC (low word)
%sp+6  format/vector word  [15:12] = frame format code, [11:0] = vector offset (vector# × 4)
```

This is the order in `struct pt_regs` in `arch/m68k/include/asm/ptrace.h`
(`sr`, `pc`, `format`, `vector`), and in Linux's `uapi/asm/ptrace.h` for
non-ColdFire builds.

| Format | Total size | Used for |
|--------|-----------|---------|
| `$0` | 8 bytes | Most exceptions, including interrupts |
| `$1` | 8 bytes | Throwaway frame, interrupt with the M bit set (U-Boot runs with M clear) |
| `$2` | 12 bytes | CHK, TRAPcc, divide by zero, trace — adds the instruction address |
| `$9` | 20 bytes | Coprocessor mid-instruction |
| `$A` | 32 bytes | Short bus cycle fault |
| `$B` | 92 bytes | Long bus cycle fault (bus/address error) |

The `RTE` instruction reads the format code to know how many words to pop. An exception handler that manually adjusts the stack or re-enters via `RTE` **must** preserve or correctly reconstruct this word.

**ColdFire** puts the format/vector information *first*: one longword at
`%sp+0` holding format [31:28], fault status [27:26] and [17:16], vector
*number* [25:18] and the old SR [15:0], then the old PC at `%sp+4`. The frame
is always 8 bytes, but the format field is `$4`–`$7`, recording how misaligned
the SSP was (0–3 bytes) before the exception, and `RTE` undoes that alignment
as it pops. Linux tells the two layouts apart with `#ifdef __mcoldfire__`,
which the compiler defines for every ColdFire `-mcpu`.

**Corrected 2026-10-25:** earlier versions of these notes listed the classic
frame with the format/vector word at `%sp+0`, which is the ColdFire order. The
code was right throughout; only the notes were wrong.

**Implication for `interrupts.c`**: the 68030 `do_irq` / exception handler wrappers must not assume a fixed frame size. Decode the format word first before walking or modifying the frame.
