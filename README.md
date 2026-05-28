# MT6572 kernel — Innioasis Y1 (Rockbox)

MediaTek MT6572 BSP kernel (Linux 3.4.5), forked from the stock Acer/MTK
source and reconfigured for the native Rockbox port of the Innioasis Y1.
The build target ("product") is `cci72_we_jb3`.

## Status

The same boot.img layout — Y1's stock kernel + our custom gzip cpio
initramfs — flashes to BOOTIMG and **boots Rockbox cleanly**, so the
LK BOOTIMG path, the orchestrator memcpy, and the ATAG handoff all
work correctly. Substituting our **rebuilt** zImage in place of stock,
however, panics in `populate_rootfs` with `Initramfs unpacking failed:
compression method lzma not configured` + `Freeing initrd memory:
1528K` — kernel-side, not LK-side. The 1528K value is invariant
across our gzip-cpio and PHASE6MARKER ramdisk variants, which means
the rebuilt kernel is reading or reserving DRAM at the initrd address
independently of the ATAG LK passes. Under active investigation.

The display panel and DSI bring-up are correct (LCM driver + DSI
PHY/PLL match the stock kernel byte-for-byte); a residual *dark
screen* after init hands off to Rockbox is a separate bring-up issue
(see [Known open issues](#known-open-issues)).

**Install (until BOOTIMG works):** wrap `arch/arm/boot/zImage` with
the y1-platform build script (`TARGET=recovery`) and flash the result
to the RECOVERY partition. The device's existing LK boots the custom
kernel from there. Stock-restore is two `mtk w` flashes (BOOTIMG +
SYSTEM, or BOOTIMG + RECOVERY + SYSTEM if recovery was overwritten).

## Quick start (from a fresh checkout)

### 1. Toolchain

A period ARM cross-toolchain is required. Linaro 4.9 `arm-eabi` works
(stock used AOSP `arm-eabi-4.6`). Modern GCC cannot build this 3.4
kernel without extensive patching.

```sh
wget https://releases.linaro.org/components/toolchain/binaries/4.9-2017.01/arm-eabi/gcc-linaro-4.9.4-2017.01-x86_64_arm-eabi.tar.xz
tar -xf gcc-linaro-4.9.4-2017.01-x86_64_arm-eabi.tar.xz -C $HOME
```

### 2. Host build dependencies

```sh
# Rocky/RHEL/Fedora
sudo dnf install -y gcc gcc-c++ make bc bison flex perl tar xz
# Debian/Ubuntu
sudo apt install -y build-essential bc bison flex perl tar xz-utils
```

### 3. Build

From the repo root, build the merged config then the kernel:

```sh
cd kernel
export ARCH=arm
export CROSS_COMPILE=$HOME/gcc-linaro-4.9.4-2017.01-x86_64_arm-eabi/bin/arm-eabi-
export TARGET_PRODUCT=cci72_we_jb3
export MTK_ROOT_CUSTOM=../mediatek/custom/
export MTK_PATH_PLATFORM=../mediatek/platform/mt6572/kernel/

# After pulling defconfig OR Kconfig-fragment changes, force a clean rebuild:
rm -f include/config/auto.conf.cmd
make mrproper

# Regenerate the merged .config from the defconfig + Kconfig fragments
make cci72_we_jb3_defconfig

# (Optional) verify per-project disables actually took post-merge:
grep -E '^CONFIG_KPROBES=|^CONFIG_IKCONFIG=|^CONFIG_HID_LOGITECH=' .config
# Expect: empty.  If anything echoes, see Config plumbing below.

# Force a full zImage rebuild from the changed sources every cycle.
# The 3.4 ARM build can rebuild Image without cascading to zImage --
# see "Pre-flash verification" below.
rm -f arch/arm/boot/zImage \
      arch/arm/boot/Image \
      arch/arm/boot/compressed/vmlinux \
      arch/arm/boot/compressed/piggy.gzip \
      arch/arm/boot/compressed/piggy.gzip.o

make -j"$(nproc)" zImage
```

Output: `arch/arm/boot/zImage`. Wrap with
`y1-platform/rockbox-boot/build-rockbox-boot.sh` and flash. See the
y1-platform README for the full flash flow.

> `TARGET_PRODUCT` must be set, or `mediatek/build/Makefile` errors
> out. The `mrproper` step matters when defconfig or a Kconfig
> fragment changes — `make` alone won't notice.

### 4. Pre-flash verification (always run before `mtk w bootimg`)

Every iteration where you edit kernel source, confirm the rebuilt
binary actually contains the edit BEFORE flashing. `make -j` on this
3.4 ARM tree can rebuild `arch/arm/boot/Image` and `vmlinux` from the
new source but **leave `arch/arm/boot/zImage` stale** (the
`Image → piggy.gzip → zImage` dependency does not cascade reliably);
`build-rockbox-boot.sh` then wraps the stale zImage and flashing
produces a boot that LOOKS identical to the previous one. The
canonical clean-rebuild commands above (the `rm -f ... && make
zImage`) sidestep that bug, but verify anyway:

```sh
# Pick any unique string that appears in your source change -- a
# printk you added, a function name, etc.  Example: the Y1DIAG
# diagnostic patch.
MARKER=Y1DIAG
grep -c "$MARKER" arch/arm/boot/zImage                       # expect > 0
grep -c "$MARKER" /path/to/build/y1-rockbox-bootimg.img      # expect > 0
```

If either grep returns 0, the binary you're about to flash does NOT
have your edit. Re-run the clean rebuild + re-wrap before flashing.

## Config plumbing (read this when a CONFIG change doesn't stick)

The MTK build merges Kconfig fragments **common → platform → project
→ flavor** during `make cci72_we_jb3_defconfig`. Each later layer
overrides the earlier one. Inputs (in merge order):

| Layer | Path | Scope |
|---|---|---|
| arch defconfig | `kernel/arch/arm/configs/cci72_we_jb3_defconfig` | per-project |
| common | `mediatek/config/common/autoconfig/kconfig/{USER,AEE,config.mk}` | all projects |
| platform | `mediatek/config/mt6572/autoconfig/kconfig/platform` | all mt6572 projects |
| project | `mediatek/config/cci72_we_jb3/autoconfig/kconfig/project` | per-project |
| flavor | `mediatek/config/cci72_we_jb3/autoconfig/kconfig/flavor` (if present) | per-project |

The **platform** layer explicitly sets a lot of `CONFIG_X=y` lines.
Disabling those only in the defconfig doesn't stick — the platform
layer re-enables them after. **Per-project overrides have to go in
the project fragment** (`mediatek/config/cci72_we_jb3/autoconfig/kconfig/project`),
which is merged later and wins. The defconfig is kept in sync for
round-trip hygiene but it's the project fragment that drives the
build. MTK make-vars (`MTK_*_SUPPORT`) live in
`mediatek/config/cci72_we_jb3/ProjectConfig.mk`, which the build
reads after `cust.mak` — set `:=yes` there, not in cust.mak (cust.mak
is for the `-D` defines).

## Changes from the stock BSP

- **Bloat trim against stock.** 72 options the platform fragment
  enables that stock kallsyms shows aren't built into the stock Y1
  kernel are disabled in both the defconfig and the project fragment.
  Categories: kernel-side profiling/tracers (IKCONFIG, PROFILING,
  OPROFILE, KPROBES, SCHEDSTATS, MTK_MET, MTK_SCHED_TRACERS,
  MTPROF_*, PREEMPT_MONITOR, ISR_MONITOR, MT_CHRDEV_REG,
  PRINTK_PROCESS_INFO); USB debug; netfilter matches/targets stock
  doesn't have; QoS/classifier; wireless debug; USB ACM + obsolete
  USB-IDE storage quirks; HID brand quirk drivers for gaming
  peripherals; misc (SCSI_TGT, CRYPTO_TWOFISH). Per-option rationale
  in the commit messages of `a7ee0264` (defconfig) and `459a79dd`
  (project fragment); every drop is anchored to a 0-hit grep against
  `/work/stock-kernel-re/stock_syms.txt`.
- **Eng/debug stripped earlier** (commit `45c73a7a`): `MT_ENG_BUILD`,
  `SLUB_DEBUG_ON`, `DEBUG_INFO`, `USB_DEBUG`, `KGDB`/`KGDB_KDB`,
  `FIQ_DEBUGGER`, `HIBERNATION`, `PM_DEBUG`/`ADVANCED_DEBUG`/
  `TEST_SUSPEND`, `EXT4_DEBUG`/`JBD_DEBUG`/`DM_DEBUG`,
  `FAULT_INJECTION` — all off. `MTK_AEE_FEATURE` + `MTK_RAM_CONSOLE`
  stay on (observability).
- **Single-LUN USB mass storage.** Stock gadget exposes two LUNs
  (one empty) which many car head units reject; this exposes only
  the SD card so it enumerates as a plain USB flash drive.
- **In-kernel Bluetooth (BlueZ) + virtual HCI.** Stock Android drove
  the CONSYS combo chip from userspace and left the kernel BT
  subsystem off; `CONFIG_BT`+`CONFIG_BT_HCIVHCI` are enabled so a
  userspace H4 shim can bridge `/dev/stpbt` to `hci0`.
- **USB host/OTG + USB Audio Class.** `CONFIG_USB_MTK_OTG`/
  `HDRC_HCD` + `CONFIG_SND_USB_AUDIO` so a USB-C DAC or USB-C
  headset can be used for digital audio out.
- **Correct display panel.** The stock `cci72_we_jb3` defconfig
  targets the `nt35510` (480×800 command-mode) panel the Y1 doesn't
  have; the rebuilt kernel ships the Y1's actual panels —
  `gc9503v_hvga_dsi_vdo_hsd` and `st7701_hvga_dsi_vdo_boe` — at
  480×360 in DSI video mode, auto-detected at boot via `compare_id`.
  Both LCM drivers were reverse-engineered from the stock bootloader
  and **verified byte-for-byte identical to the stock g368_nyx
  kernel's LCM drivers** (init table, every `get_params` DSI field
  including PLL: `pll_div1=2`/`fbk_div=15` for ST7701, `pll_div1=1`/
  `fbk_div=13` for GC9503V).
- **Charger = FAN5405.** `MTK_FAN5405_SUPPORT=yes` (stock-default
  BQ24196/NCP1851 drivers are incomplete in this drop); also gates
  the USB-host OTG VBUS path.
- **Pinned defconfig.**
  `kernel/arch/arm/configs/cci72_we_jb3_defconfig` is the single
  source of truth; round-trips with the post-merge `.config` when
  the project-fragment overrides are also in sync. Generated
  artifacts (`.config`, `include/config/`, `include/generated/`)
  are not tracked — they regenerate from the defconfig.

## Observability

There is no UART (`printk.disable_uart=1`, pads unreachable on
hardware), no external SD slot (the SD is internal), and the rebuilt
kernel can't expose USB-MSC until userspace is up. The two reliable
signals are:

- **`mtk r expdb`** over BROM. On any kernel panic, MTK AEE writes
  the panic record + ~85 KB of pre-panic dmesg ring buffer to the
  EXPDB partition, **XOR-encoded with `0xAABBCCDD`**. A raw read
  looks like a blank fill-pattern partition — decoding reveals the
  panic stack and boot log:

  ```python
  d   = open('expdb.bin','rb').read()
  fill = bytes.fromhex('ddccbbaa')
  dec = bytes(b ^ fill[i%4] for i,b in enumerate(d))
  open('expdb.decoded','wb').write(dec)
  # then: strings -n 8 expdb.decoded | grep -E 'panic|Unable|Oops'
  ```

  Layout in the decoded partition: AEE header ~`0x000–0x014`, panic
  backtrace `0x300–0x4a8`, pre-panic dmesg ring buffer
  `0x1200..~0x21400`.

- **Visual oracle.** The y1-platform init paints distinct LCD
  colours at each bring-up stage (RED → ORANGE → YELLOW → GREEN →
  CYAN/GRAY → PINK → BLUE → PURPLE). Useful when init is running
  but no kernel panic fires.

## Requirements

- **A case-sensitive filesystem.** The tree contains files that
  differ only in case (e.g. `xt_HL.c` vs `xt_hl.c`); a
  case-insensitive volume (default macOS, some network shares)
  silently corrupts the checkout.
- **An x86_64 Linux host.**
- **A period ARM cross-toolchain** (Linaro 4.9 arm-eabi works; see
  Quick start).

## Known open issues

- **Rebuilt-kernel BOOTIMG initrd panic.** Same boot.img layout, only
  the zImage changes; with stock zImage it boots Rockbox, with our
  zImage it panics with `compression method lzma not configured` +
  `Freeing initrd memory: 1528K`. The 1528K size is invariant across
  ramdisk contents (gzip cpio or PHASE6MARKER marker) and across
  defconfig changes, suggesting the rebuilt kernel itself is
  reading/reserving DRAM at `initrd_start` (0x84100000) differently
  from stock. Under investigation; current line is comparing the
  merged `.config` against what stock-kernel kallsyms tells us
  shipped, and seeing whether bringing the rebuilt kernel size /
  feature set closer to stock makes the panic go away. Install via
  RECOVERY in the meantime.
- **Dark screen / Rockbox runs blind.** With the kernel booted from
  RECOVERY, init runs and execs `rockbox.y1`, but the panel scans out
  black instead of the Rockbox UI. The display engine completes init
  cleanly (one-time RDMA0 underflow at the LK→mtkfb handoff that
  doesn't repeat); suspects are the boot-logo handover
  (`mtkfb_set_lcm_inited`), Rockbox's fb-pan/buffer geometry, or
  backlight handoff via `disp_bls_set_backlight`. Reproducible with
  `DUMP_LOGS=1` in the y1-platform build for a full `Y1_dmesg.txt`
  capture.
- **Stock kernel RE workspace.** `/work/stock-kernel-re/` has the
  decompressed stock kernel (`vmlinux_g368_nyx.bin`, VA base
  `0xC0008000`) and full 42,751-symbol kallsyms dump
  (`stock_syms.txt`) for any future function-level comparison.
  Stock and our build are byte-for-byte identical in the LCM
  drivers and the DSI PHY/PLL setup.
