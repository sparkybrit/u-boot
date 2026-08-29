# U-Boot for Sparky1 SBC

This is a fork of U-Boot (currently rebased to v2026.04) adding support for the **sparky1** — a custom 68030-based single-board computer. The upstream project supports the m68k architecture only for ColdFire (MCF) CPUs; this fork extends it with classic 68030 CPU support and drivers for the XR68C681 DUART.

## Branch layout

All custom work is on the `sparky` branch — currently ~29 commits on top of an
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
git rebase v2026.07           # one release at a time, never an -rc tag
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

If a file shows up in that intersection and has nothing to do with sparky1,
suspect a leftover rather than a conflict to resolve. Two MMC files were in
this list until they were traced back to a revert that had not fully landed —
one of them referenced a symbol that does not exist anywhere in the tree.

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

Limited by the 100 Hz system timer, not the bus. Measured on a SanDisk
SDCFX-008G: **~50 KB/s reading** (a 4 KB file via `load` takes ~1.4 s once
directory and FAT lookups are counted) and **~18 KB/s writing** (2 MB of
filesystem metadata in 112 s).

The cost is **not** `ide_wait()` — its `udelay(100)` only runs when the card is
actually BUSY. It is the **unconditional `udelay(50)` inside `ide_read()`'s
per-block loop**, right after the PIO read command. Every `udelay()` on this
board costs a full 10 ms tick whatever you ask for, because `usec_to_tick(50)`
is 0 at 100 Hz and `__udelay()`'s loop is `while (get_ticks() < tmp+1)` — the
`+1` forces a wait for the next tick edge. One tick per sector caps reads at
~100 sectors/s ≈ 51 KB/s, which is exactly what is measured.

Going faster means a finer time source — the XR68C681 counter registers can be
read live — not a faster bus. See the TODO below. Sixteen-bit mode is wired up in `cfmanager` but commented
out; enabling it needs the PLD change, CF D8–D15 wired to D23–D16, **and**
clearing `CONFIG_SYS_ATA_DATA_8BIT`.

### TODO — give `udelay()` a real time source

Not done. Would lift CF throughput by roughly an order of magnitude and improve
every timeout in U-Boot; nothing depends on the current behaviour.

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
A sparky1 `__udelay()` that spins on the live counter leaves `counter`,
`clock-frequency` and `get_timer()` untouched:

- The XR68C681 counter decrements at 3.6864 MHz / 16 = **230400 Hz**, i.e. one
  count per **4.34 µs** — about 2300× finer than the 10 ms tick.
- Read CTU/CTL live, spin until enough counts have elapsed, handling wrap.

**Two wrinkles to handle:**

1. In timer mode the counter reaches zero **twice** per interrupt period
   (`f = clk / 2N`, N = 1152), so its absolute phase is ambiguous. That is fine
   for a delay, where only *elapsed* counts matter, but it is why this cannot
   naively be reused to make `get_count()` fine-grained as well.
2. The counter is not running until `xr68c681_timer_probe()` starts it, so
   guard against `udelay()` calls made earlier in `board_init_f`. Falling back
   to a spin loop is acceptable there.

**Expected result.** The per-sector cost drops from 10 ms to ~50 µs, after which
reads are bounded by the transfer itself (512 byte-reads, ~0.5–1 ms/sector)
rather than the tick. Verify with `time ide read 0x40100000 0x1000 0x80`
(128 sectors) before and after — do not trust an estimate, measure it.

### Rebase note

`drivers/block/ide.c` is now a modified upstream file and will conflict when
rebasing through release tags. The changes are three `#if
IS_ENABLED(CONFIG_SYS_ATA_DATA_8BIT)` blocks around the data-path functions,
the `ide_set_8bit_mode()` helper and its call in `ide_ident()`, the `0xFF`
no-device check in `ide_init_one()`, `.of_match` on `U_BOOT_DRIVER(ide)`, and
the `strlcpy()` size fix in `ide_probe()` (upstream passes `BLK_*_SIZE` where
the buffers are `BLK_*_SIZE + 1`, which truncates a maximum-length string by
one character - it printed `HDX 5.08` as `HDX 5.0`). That last one is an
upstream bug, not a sparky1 change, and should drop out once fixed upstream.

### Upstream submission — the `strlcpy()` fix

**Sent 2026-08-29, awaiting review.** This is an **upstream U-Boot bug**, not a
sparky1 change, so it goes to the mailing list and should disappear from this
fork on a later rebase.

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

#### Where it lives — a worktree, not a checkout

```
~/Projects/u-boot-ide-fix     branch ide-strlcpy-fix, based on upstream/main
```

The worktree is what makes this work at all. `CLAUDE.md` does not exist
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
which is fine, this *is* a fix. Once it lands upstream, drop the local change,
remove it from the `ide.c` rebase note above so it stops being a rebase
conflict, and `git worktree remove ~/Projects/u-boot-ide-fix`.

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

## Programming the board

The boot store is a Dallas DS1250Y NVSRAM at `0x00000000`. It is not removed
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

**`u-boot.bin` is written at NVRAM address 0, not 0x400.** The image *starts*
with the exception vector table: offset 0x004 holds `0x00000400`, which is
`_start`, and 0x008 onward hold `_exc_handler`. `CONFIG_SYS_MONITOR_BASE`
= 0x400 is where U-Boot sits *inside* the image, not where the image is
loaded. The programmer always writes from address 0, which is correct.
The whole image must fit the 512 KB device (currently ~283 KB).

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
- To reboot without reflashing, send U-Boot's own `reset` command.

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

#### Read/write integrity

`ide read` and `ide write` take **all three arguments in hex** — address,
block number and block count — and operate on the current device, which
`ide device` selects (it defaults to 0).

    ide read  addr blk# cnt
    ide write addr blk# cnt

The tests below use two 4 KB buffers at `0x40100000` and `0x40110000`. Those
sit in the middle of SDRAM (`0x40000000`–`0x403FFFFF`), well above INIT_RAM
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
mw.l 0x40100000 0xdeadbeef 0x80     # 0x80 longwords = 512 B = one sector
mw.l 0x40110000 0x00000000 0x80     # clear dest: a failed read must not pass
ide write 0x40100000 0x1000 0x1
ide read  0x40110000 0x1000 0x1
cmp.l 0x40100000 0x40110000 0x80
```

Then 8 sectors in one command. This is worth doing separately: the card
re-asserts DRQ for each sector without being re-commanded, so it exercises
the inter-sector turnaround that the single-sector path never reaches.

```
mw.l 0x40100000 0xdeadbeef 0x400
mw.l 0x40110000 0x00000000 0x400
ide write 0x40100000 0x1000 0x8
ide read  0x40110000 0x1000 0x8
cmp.l 0x40100000 0x40110000 0x400
```

A pass prints `Total of 1024 word(s) were the same` — `cmp` calls a `.l`
object a "word" regardless of the suffix, so that is 1024 longwords, not
1024 16-bit words. A failure names both addresses and both values:

```
word at 0x40100000 (0xdeadbeef) != word at 0x40110000 (0x00000000)
```

`0xdeadbeef` is a poor pattern for spotting a sector landing at the wrong
offset, since every longword is identical. For a non-repeating pattern,
copy 4 KB of U-Boot's own image out of NVRAM instead of filling a constant —
it is varied, read-only and always present:

```
cp.l 0x00001000 0x40100000 0x400
mw.l 0x40110000 0x00000000 0x400
ide write 0x40100000 0x1000 0x8
ide read  0x40110000 0x1000 0x8
cmp.l 0x40100000 0x40110000 0x400
```

**What a round-trip cannot catch.** Writing and reading through the same
driver hides any fault that is symmetric. If `ide_output_data()` and
`ide_input_data()` both transposed byte pairs, every `cmp` above would still
pass while every sector on the card was scrambled for anything else that
reads it. Confirming the ordering needs data this board did not write.

The cheapest source is a card partitioned or formatted on a PC: LBA 0 ends
with the boot signature `0x55 0xAA` at offset `0x1FE`.

```
ide read 0x40100000 0x0 0x1
md.w 0x401001fe 1
```

On big-endian m68k the byte at the lower address is the high half, so a
correct data path shows `0x55aa`. `0xaa55` means every byte pair is
transposed — fix `ide_input_data()`, do not "correct" it downstream. If the
card has no partition table at all this check says nothing; fall back to
running the hardware project's `tests/cf_test/` and reading a sector it
wrote.

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

Keep the buffer well below `relocaddr` (check `bdinfo`; it is around
`0x403b8000`), so `0x40100000` is safe. The whole run is ~157 commands and
takes about two minutes, dominated by the metadata zeroing.

This is also the only real test of the **write** path: format from the SBC,
then confirm Linux mounts the result read-write and can allocate clusters in
it. A round-trip through this driver alone cannot detect a symmetric fault.

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

# Compare — "Total of 16128 word(s) were the same" means INIT_RAM is clean
# (cmp calls a .l object a "word"; that is 16128 longwords)
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
