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

### Modified upstream files
- `arch/m68k/include/asm/uart.h` → moved to `arch/m68k/include/asm/coldfire/uart.h` (restored to upstream ColdFire content; `serial_mcf.c` updated to match)
- `arch/m68k/lib/cache.c`, `interrupts.c` — extended for 68030 (non-ColdFire) builds
- `arch/m68k/lib/Makefile` — split ARCH_COLDFIRE bundle; common files now `obj-y`, ColdFire-only (`bdinfo.o`) stays conditional
- `arch/m68k/Kconfig` — added `MC680x0`, `MC68030`, `TARGET_SPARKY1` alongside upstream's QEMU m68k additions
- `lib/time.c`, `drivers/timer/timer-uclass.c` — minor fixes

## XR68C681 DUART — key implementation notes

The XR68C681 is Exar's version of the MC68C681 Dual UART with extended features. All three drivers (serial, timer, GPIO) include `<xr68c681.h>` and use the `xr68c681_t` register struct. Platform data (`mc68681_plat`) is shared via `<dm/platform_data/mc68681.h>`.

### Baud rate generation (XR68C681-specific)

The XR68C681 has a per-channel **BRG extend bit (X bit)** that selects an extended column in the BRG rate table. It is set/cleared via CR commands (not a register field):

| Command | Value | Effect |
|---------|-------|--------|
| `UART_UCR_SET_RX_EXTEND` | `0x08` | Set Rx X=1 (extended rates) |
| `UART_UCR_CLR_RX_EXTEND` | `0x09` | Clear Rx X=0 |
| `UART_UCR_SET_TX_EXTEND` | `0x0A` | Set Tx X=1 |
| `UART_UCR_CLR_TX_EXTEND` | `0x0B` | Clear Tx X=0 |

115200 baud requires CSR=`0x88` **plus** X=1 on both channels. CSR=`0xBB` alone gives 9600, not 115200.

The BRG table in `xr68c681_serial.c` encodes both the CSR value and the extend flag for each supported rate.

### ACR register

ACR (Auxiliary Control Register) at offset 0x04 is **write-only** — reads at that address return IPCR (Input Port Change Register), not ACR. Never use read-modify-write on ACR; always write the full value directly.

`UART_UACR_CLK` (`0xF0`) = BRG Set 2 (bit 7) + Timer mode X1/CLK÷16 (bits 6:4 = 111). This value is shared by both the serial init and the timer probe.

### Timer mode

The counter/timer runs at 100 Hz: N=0x0480 (1152), f = (3.6864 MHz / 16) / (2 × 1152) = 100 Hz. The interrupt is cleared by reading `uopc` (0x0F). In timer mode, the "stop counter" command does **not** actually halt the counter — it only clears ISR[3]. The `clock-frequency` DTS property must be set to `100`.

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
  └─ common/board_r.c:run_main_loop() → common/main.c:main_loop()
```

**Notes:**
- `TIMER_EARLY` is not set — the timer is not available during `board_init_f`. Any `get_timer()` call before `timer_init` in phase 2 would trigger a lazy `dm_timer_init()`, which would panic as the timer device isn't probed yet pre-relocation.
- `board_early_init_f`, `board_init`, `board_late_init`, `misc_init_r` are all not configured and are skipped.
- The XR68C681 serial driver carries `DM_FLAG_PRE_RELOC` so it is bound and probed before relocation, then re-probed in RAM after relocation.

## Build

```bash
# Configure for sparky1
make sparky1_defconfig

# Build (cross-compiler required)
make CROSS_COMPILE=m68k-linux-gnu- -j$(nproc)
```

The 68030 requires `-mcpu=68030` (not `-mcpu=5307` or any ColdFire flag). Verify the cross-compiler supports classic m68k ISA — ColdFire-only compilers will not work.

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
