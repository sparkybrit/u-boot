# U-Boot for Sparky1 SBC

This is a fork of U-Boot (currently rebased to v2026.04) adding support for the **sparky1** — a custom 68030-based single-board computer. The upstream project supports the m68k architecture only for ColdFire (MCF) CPUs; this fork extends it with classic 68030 CPU support and drivers for the XR68C681 DUART.

## Branch layout

All custom work is on the `sparky` branch. `master` tracks upstream (`https://github.com/u-boot/u-boot.git`). To update master from upstream:

```bash
git checkout master
git fetch upstream
git merge upstream/master
git push origin master
```

To incrementally rebase `sparky` onto the new master, rebase through each stable release tag one at a time (e.g. `git rebase v2026.07`), resolving conflicts at each step before moving to the next.

## What this fork adds (sparky branch summary)

### New CPU support — `arch/m68k/cpu/mc68030/`
- `start.S` — reset vector, early 68030 init, cache setup, jump to `board_init_f`
- `cpu.c` — `cpu_init()`, `watchdog_reset()`
- `cpu_init.c` — chip-select and memory-controller setup
- `interrupts.c` — interrupt controller init
- `speed.c` — clock/speed reporting

### Sparky1 board — `board/sparky/sparky1/`
- `Kconfig`, `Makefile`, `sparky1.c` — board init, `checkboard()`, `dram_init()`
- `configs/sparky1_defconfig` — full build configuration
- `include/configs/sparky1.h` — legacy config header
- `arch/m68k/dts/sparky1.dts` — device tree: XR68C681 DUART at 0x80000000 (UART, timer, GPIO)

### XR68C681 DUART drivers
- `drivers/serial/xr68c681_serial.c` — serial driver (UCLASS_SERIAL)
- `drivers/timer/xr68c681_timer.c` — 100 Hz counter/timer driver (UCLASS_TIMER)
- `drivers/gpio/mc68681_gpio.c` — output port GPIO driver (UCLASS_GPIO)
- `include/xr68c681.h` — register map (`xr68c681_t`) and all bit macros for the discrete chip
- `include/dm/platform_data/mc68681.h` — shared DM platform data struct (`mc68681_plat`)
- Compatible strings: `exar,xr68c681` (serial), `exar,xr68c681_timer` (timer), `motorola,mc68681_gpio` (GPIO)

### CompactFlash / IDE
- `arch/m68k/dts/sparky1.dts` — `ide@c0000000` node, `compatible = "u-boot,ide"`
- `board/sparky/sparky1/sparky1.c` — `board_late_init()` probes the controller
- Uses upstream `drivers/block/ide.c` in 8-bit mode; see [CompactFlash / IDE](#compactflash--ide--key-implementation-notes)

### Modified upstream files
- `arch/m68k/include/asm/uart.h` → moved to `arch/m68k/include/asm/coldfire/uart.h` (restored to upstream ColdFire content; `serial_mcf.c` updated to match)
- `arch/m68k/lib/cache.c`, `interrupts.c` — extended for 68030 (non-ColdFire) builds
- `arch/m68k/lib/Makefile` — split ARCH_COLDFIRE bundle; common files now `obj-y`, ColdFire-only (`bdinfo.o`) stays conditional
- `arch/m68k/Kconfig` — added `MC680x0`, `MC68030`, `TARGET_SPARKY1` alongside upstream's QEMU m68k additions
- `lib/time.c`, `drivers/timer/timer-uclass.c` — minor fixes
- `drivers/block/ide.c`, `drivers/block/Kconfig` — added `CONFIG_SYS_ATA_DATA_8BIT`
  (8-bit data port), a `u-boot,ide` devicetree binding, and a floating-bus
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

## CompactFlash / IDE — key implementation notes

A CompactFlash card in True IDE mode hangs off `/DISKCS` at `0xC0000000`. It is
driven by upstream's generic `drivers/block/ide.c`, which needed three changes
to cope with the wiring.

### The wiring

| Property | Value | Kconfig |
|----------|-------|---------|
| Base address | `0xC0000000` | `CONFIG_SYS_ATA_BASE_ADDR` |
| Registers | 8, byte-wide, selected by A2–A0 | `CONFIG_SYS_ATA_STRIDE=0x1` |
| Data port | offset 0, **8 bits** on D31–D24 | `CONFIG_SYS_ATA_DATA_8BIT=y` |
| Devices | one, master only | `CONFIG_SYS_IDE_MAXDEVICE=0x1` |

Timing (`/IORD`, `/IOWR`, wait states, `/DSACK0`) comes from the `cfmanager`
GAL in the sparky1 hardware project, which stretches every CF cycle to ~437 ns
for ATA PIO mode 0. The 400 ns ATA requires between the last data byte and the
next status read is therefore free — no extra delay in the driver.

### 8-bit data port

Only the low byte of the ATA data bus is wired, so `CONFIG_SYS_ATA_DATA_8BIT`
switches `ide_input_data()`, `ide_output_data()` and `ide_input_swap_data()` to
byte transfers, and makes `ide_ident()` call `ide_set_8bit_mode()` before
IDENTIFY — which issues `INITIALIZE DEVICE PARAMETERS` (0x91, tolerated if it
fails; it only affects CHS translation, never used here) and then
`SET FEATURES` (0xEF) with subcommand `0x01`, "enable 8-bit PIO data transfer".
That last one matters: without it the card also drives D8–D15.

`ide_set_8bit_mode()` sits in `ide_ident()` rather than `ide_init_one()`
because 8-bit mode is a **per-device** setting, and `ide_init_one()` runs once
per bus.

### Byte ordering — the part that is easy to get wrong

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

### CS1 is not decoded — keep CONFIG_ATAPI off

Only A2–A0 reach the card, so the alternate status and device control
registers are unreachable. `ATA_DEV_CTL` has exactly one user in the driver,
`atapi_wait_mask()`, which is compiled out unless `CONFIG_ATAPI` is set —
hence `CONFIG_SYS_ATA_DATA_8BIT` carries `depends on !ATAPI`. `RESET–` is not
software-controlled either, so `CONFIG_IDE_RESET` stays off and there is no
`ide_set_reset()`.

### Throughput

Limited by the 100 Hz system timer, not the bus. `ide_wait()` polls with
`udelay(100)`, and the finest tick available is 10 ms, so any transfer that has
to wait at all waits a whole tick — reads land around 50 KB/s. Going faster
means a finer time source (the XR68C681 counter registers can be read live),
not a faster bus. Sixteen-bit mode is wired up in `cfmanager` but commented
out; enabling it needs the PLD change, CF D8–D15 wired to D23–D16, **and**
clearing `CONFIG_SYS_ATA_DATA_8BIT`.

### Rebase note

`drivers/block/ide.c` is now a modified upstream file and will conflict when
rebasing through release tags. The changes are three `#if
IS_ENABLED(CONFIG_SYS_ATA_DATA_8BIT)` blocks around the data-path functions,
the `ide_set_8bit_mode()` helper and its call in `ide_ident()`, the `0xFF`
no-device check in `ide_init_one()`, and `.of_match` on `U_BOOT_DRIVER(ide)`.

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
| `drivers/block/ide.c` | Generic IDE driver — extended for 8-bit data ports |
| `include/ata.h` | ATA register offsets, computed from `CONFIG_SYS_ATA_*` |
| `drivers/serial/serial_mcf.c` | ColdFire serial driver (uses `<asm/coldfire/uart.h>`) |
| `doc/board/sparky/index.rst` | Sphinx toctree entry for sparky board docs |
| `doc/board/sparky/sparky1.rst` | Hardware overview, build instructions, peripheral support |

## Boot sequence — sparky1

### Phase 0 — start.S (from flash, before relocation and before C)

```
reset vector fetch (0x000004 → _start)
arch/m68k/cpu/mc68030/start.S:_start
  ├─ %sr = 0x2700                                             disable all interrupts
  ├─ %a5 = __got_start                                        set %a5 to the linker's Global Offset Table in Flash
  ├─ %sp = CFG_SYS_INIT_RAM_ADDR + CFG_SYS_INIT_RAM_SIZE      set %sp to top of the init ram area (0x4001:0000)
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
  │         │    ├─ ide_set_8bit_mode()        INIT_DEV_PARAMS, then SET FEATURES 0x01
  │         │    ├─ ATA_CMD_ID_ATA             IDENTIFY DEVICE
  │         │    └─ ide_input_swap_data()      512 bytes, one at a time
  │         └─ blk_create_devicef("ide_blk")   → "ide 0" block device
  └─ common/board_r.c:run_main_loop() → common/main.c:main_loop()
```

**Notes:**
- `TIMER_EARLY` is not set — the timer is not available during `board_init_f`. Any `get_timer()` call before `timer_init` in phase 2 would trigger a lazy `dm_timer_init()`, which would panic as the timer device isn't probed yet pre-relocation.
- `board_early_init_f`, `board_init` and `misc_init_r` are not configured and are skipped. `board_late_init` **is** configured, and exists solely to probe the CompactFlash.
- **The CompactFlash must not be autoprobed.** `dm_autoprobe()` runs inside `initr_dm()`, long before `interrupt_init()` and the M68K `timer_init()` further down `init_sequence_r[]`. The XR68C681 tick counter is only advanced by the timer's 100 Hz interrupt handler, so a controller probed at `initr_dm()` time reaches `ide_probe()`'s `mdelay(100)` with the counter frozen at zero and `__udelay()` spins forever. Probing from `board_late_init()` is what avoids that; do not add `DM_FLAG_PROBE_AFTER_BIND` to the IDE driver.
- The XR68C681 serial driver carries `DM_FLAG_PRE_RELOC` so it is bound and probed before relocation, then re-probed in RAM after relocation.

### Exception vector table

The vector table in flash is **never copied to RAM**. Instead, `trap_init()` in `arch/m68k/lib/traps.c` builds a fresh table directly in RAM:

- **Destination:** `CFG_SYS_SDRAM_BASE` = `0x40000000` (base of SDRAM)
- **Contents:** vectors 2–24 and 32–63 → `_exc_handler`; vectors 25–31 and 64–255 → `_int_handler` (all pointing to their relocated RAM addresses). Vectors 0 (initial SP) and 1 (reset PC) are not written.
- **VBR set:** `setvbr(0x40000000)` at the end of `trap_init()`, called from `arch_initr_trap()` during `initcall_run_r()` in phase 2.

Before `setvbr` is called, VBR defaults to 0x000000 and the flash vector table handles any early exceptions.

## Build

```bash
# Configure for sparky1
make sparky1_defconfig

# Build (cross-compiler required)
make CROSS_COMPILE=m68k-linux-gnu- -j$(nproc)
```

The 68030 requires `-mcpu=68030` (not `-mcpu=5307` or any ColdFire flag). Verify the cross-compiler supports classic m68k ISA — ColdFire-only compilers will not work.

## Testing

### Cache verification

`CONFIG_CMD_CACHE=y` and `CONFIG_CMD_TIME=y` are both set in `configs/sparky1_defconfig`. The XR68C681 timer runs at 100 Hz (10 ms resolution), so each timed operation should run long enough to register — at 16 MHz the 256 KB blocks below typically take several hundred milliseconds.

**Functional test** — verifies data integrity through enable/disable cycles and that `dcache_invalid()` fires correctly on re-enable:

```
# Verify both caches report enabled at boot
icache
dcache

# Write with D-cache ON, disable, read back — write-through means memory is always current
mw.l 0x40200000 0xdeadbeef 0x40
md.l 0x40200000 8
dcache off
md.l 0x40200000 8
# Both reads must show 0xdeadbeef

# Write new pattern with cache OFF, re-enable — dcache_enable() calls dcache_invalid()
# so re-enable must flush any stale lines; if this reads 0xdeadbeef the invalidation is broken
mw.l 0x40200000 0xcafebabe 0x40
dcache on
md.l 0x40200000 8
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
mw.l 0x40100000 0xdeadbeef 0x10000
mw.l 0x40110000 0xdeadbeef 0x10000

echo "--- caches OFF ---"
icache off
dcache off
time mw.l 0x40100000 0x12345678 0x10000
time cmp.l 0x40100000 0x40110000 0x8000

echo "--- caches ON ---"
icache on
dcache on
time mw.l 0x40100000 0x12345678 0x10000
time cmp.l 0x40100000 0x40110000 0x8000
```

The scratch addresses (0x40100000 / 0x40110000) sit in the middle of the 4 MB SDRAM region (0x40000000–0x403FFFFF), well below where U-Boot relocates to near the top. If both timed results round to zero, increase the count to `0x40000` (1 MB). The D-cache speedup is most visible on the `cmp` (read-heavy) operation; `mw` speedup is smaller because the 68030 D-cache is write-through.

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
socket, or `/DISKCS` not reaching the card. `8-bit transfer mode rejected`
means the card refused `SET FEATURES 0x01`; that card cannot be used on this
board without going to 16-bit mode in `cfmanager`.

Then, at the prompt:

```
ide info                          # re-report what was found
part list ide 0                   # partition table
ls ide 0:1 /                      # FAT directory listing
load ide 0:1 0x40200000 /file     # read a file into SDRAM
```

**Read/write integrity** — `ide read`/`ide write` take block counts in hex.
This round-trips 8 sectors through a scratch area of SDRAM well below the
relocation address, and **overwrites LBA 0x1000 on the card**:

```
# fill a buffer, write it out, read it back to a second buffer, compare
mw.l 0x40100000 0xdeadbeef 0x400
ide write 0x40100000 0x1000 8
mw.l 0x40110000 0x00000000 0x400
ide read  0x40110000 0x1000 8
cmp.l 0x40100000 0x40110000 0x400
# "Total of 1024 longwords were the same" = the data path is clean both ways
```

If `cmp` reports mismatches at every odd longword, or the model string in
`ide info` reads as `aSDnsi kDCSBF2-18`, the byte ordering is wrong — check
`ide_input_swap_data()` against the notes above rather than "fixing" the
buffer.

### INIT_RAM relocation verification

After relocation, U-Boot must use only the relocated `gd`, stack, and heap in the upper SDRAM — nothing should touch INIT_RAM (0x40000000–0x4000FFFF, the bottom 64 KB of SDRAM) again.

**Important:** the exception vector table lives at `0x40000000–0x400003FF` (256 vectors × 4 bytes). Do **not** poison that region — any exception during the test would vector to the poison value and hard-crash. Start poisoning at `0x40000400` (the first byte after the table). The usable poison range is therefore `0x40000400–0x4000FFFF` = 63 KB − 0 = 0x3F00 longwords.

Poison from `0x40000400` with a distinctive value, write the same pattern to a reference area above it, exercise U-Boot, then `cmp` the two regions. Any address reported by `cmp` is a location that was written after poisoning — a stale pointer or residual stack/heap use.

```
# Poison INIT_RAM, skipping the vector table at 0x40000000–0x400003FF
# 0x3F00 longwords = 63 KB starting at 0x40000400
mw.l 0x40000400 0xdeadc0de 0x3f00

# Write same pattern to reference area (above INIT_RAM, below load address at 0x40200000)
mw.l 0x40020400 0xdeadc0de 0x3f00

# Confirm relocated gd, sp, and heap are all up near 0x403xxxxx — not in INIT_RAM
bdinfo

# Exercise U-Boot to trigger any residual INIT_RAM access
printenv
dm tree
md.l 0x40000400 4

# Compare — "Total of 16128 longwords were the same" means INIT_RAM is clean
# Any reported address means something still touched INIT_RAM after relocation
cmp.l 0x40000400 0x40020400 0x3f00
```

If `cmp` flags an address, the offset from `0x40000400` locates which part of INIT_RAM was touched: offsets near zero suggest a stale heap or BSS pointer; offsets near `0xBBFC` (= `0xF000 - 0x400`) suggest a leftover stack reference (the initial stack and `gd` struct were reserved at the top of INIT_RAM by `board_init_f_alloc_reserve()`).

Run `bdinfo` before poisoning to confirm that `gd`, `sp`, and the malloc arena are all well above `0x4000FFFF`. If any of them appear in the `0x40000xxx` range, relocation did not complete correctly and poisoning would corrupt live data.

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
%sp+0  format/vector word  [15:12] = frame format code, [11:0] = vector offset (vector# × 4)
%sp+2  old PC (high word)
%sp+4  old PC (low word)
%sp+6  old %sr
```

| Format | Total size | Used for |
|--------|-----------|---------|
| `$0` | 8 bytes | Most exceptions (normal) |
| `$2` | 12 bytes | Instruction continuation |
| `$9` | 20 bytes | Coprocessor mid-instruction |
| `$A` | 32 bytes | Short bus cycle |
| `$B` | 92 bytes | Long bus cycle (bus/address error) |

The `RTE` instruction reads the format code to know how many words to pop. An exception handler that manually adjusts the stack or re-enters via `RTE` **must** preserve or correctly reconstruct this word.

**ColdFire** always uses a fixed 8-byte frame; the format field is always `$4`. `RTE` always pops exactly 8 bytes.

**Implication for `interrupts.c`**: the 68030 `do_irq` / exception handler wrappers must not assume a fixed frame size. Decode the format word first before walking or modifying the frame.
