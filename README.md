# MT6572 kernel — Innioasis Y1 (Rockbox)

MediaTek MT6572 BSP kernel (Linux 3.4.5), forked from the stock Acer/MTK
source and reconfigured for the native Rockbox port of the Innioasis Y1.
The build target ("product") is `cci72_we_jb3`.

## Status

**Kernel boots and runs Rockbox** when the resulting boot.img is flashed to
the **RECOVERY** partition. Flashing the same boot.img to **BOOTIMG** is
rejected by the Y1's LK in a way that delivers a corrupted ramdisk to the
kernel (`Initramfs unpacking failed: compression method lzma not configured`
→ `Kernel panic - not syncing: VFS: Unable to mount root fs`); the root cause
is in LK's per-partition load path and is not yet patched. The display panel
and DSI bring-up are correct (the LCM driver and DSI PHY/PLL match the stock
kernel byte-for-byte); a residual *dark screen* after init hands off to
Rockbox is the remaining bring-up problem, debuggable with real dmesg now
that observability is in place (see below).

**Install:** wrap `arch/arm/boot/zImage` with the y1-platform build script
(`TARGET=recovery`) and flash the result to the RECOVERY partition. The
device's existing LK boots the custom kernel from there. Stock-restore is
two `mtk w` flashes (BOOTIMG + SYSTEM, or BOOTIMG + RECOVERY + SYSTEM if the
recovery partition was overwritten).

## Observability

There is no UART (`printk.disable_uart=1`, pads unreachable on hardware), no
external SD slot (the SD is internal), and the rebuilt kernel can't expose
USB-MSC until userspace is up. The two reliable signals are:

- **`mtk r expdb`** over BROM. On any kernel panic, MTK AEE writes the panic
  record + ~85 KB of pre-panic dmesg ring buffer to the EXPDB partition,
  **XOR-encoded with `0xAABBCCDD`**. A naive read looks like a blank
  fill-pattern partition — decoding reveals the panic stack and boot log:

  ```python
  d   = open('expdb.bin','rb').read()
  fill = bytes.fromhex('ddccbbaa')
  dec = bytes(b ^ fill[i%4] for i,b in enumerate(d))
  open('expdb.decoded','wb').write(dec)
  # then: strings -n 8 expdb.decoded | grep -E 'panic|Unable|Oops'
  ```

  Layout in the decoded partition: AEE header ~`0x000–0x014`, panic
  backtrace `0x300–0x4a8`, pre-panic dmesg ring buffer `0x1200..~0x21400`.

- **Visual oracle.** The y1-platform init paints distinct colors at each
  bring-up stage (RED → ORANGE → YELLOW → GREEN → CYAN/GRAY → PINK → BLUE
  → PURPLE) and holds at the failure stages (WHITE: no ROCKBOXSYS;
  MAGENTA: mount failed; GRAY: `/dump_logs` marker absent). Useful when
  init is running but no kernel panic fires.

## Changes from the stock BSP

- **Single-LUN USB mass storage.** The stock gadget exposes two LUNs (one
  empty) which many car head units reject; this exposes only the SD card so
  it enumerates as a plain USB flash drive.
- **In-kernel Bluetooth (BlueZ) + virtual HCI.** Stock Android drove the
  CONSYS combo chip from userspace and left the kernel BT subsystem off;
  `CONFIG_BT`+`CONFIG_BT_HCIVHCI` are enabled so a userspace H4 shim can
  bridge `/dev/stpbt` to `hci0`.
- **USB host/OTG + USB Audio Class.** `CONFIG_USB_MTK_OTG`/`HDRC_HCD`
  + `CONFIG_SND_USB_AUDIO` so a USB-C DAC or USB-C headset can be used
  for digital audio out.
- **Correct display panel.** The stock `cci72_we_jb3` defconfig targets the
  `nt35510` (480×800 command-mode) panel the Y1 doesn't have; the rebuilt
  kernel ships the Y1's actual panels — `gc9503v_hvga_dsi_vdo_hsd` and
  `st7701_hvga_dsi_vdo_boe` — at 480×360 in DSI video mode, auto-detected
  at boot via `compare_id`. Both LCM drivers were reverse-engineered from
  the stock bootloader and **verified byte-for-byte identical to the stock
  g368_nyx kernel's LCM drivers** (init table, every `get_params` DSI
  field including PLL: `pll_div1=2`/`fbk_div=15` for ST7701,
  `pll_div1=1`/`fbk_div=13` for GC9503V).
- **Charger = FAN5405.** `MTK_FAN5405_SUPPORT=yes` (stock-default
  BQ24196/NCP1851 drivers are incomplete in this drop); also gates the
  USB-host OTG VBUS path.
- **Eng/debug configs disabled.** `MT_ENG_BUILD`, `SLUB_DEBUG_ON`,
  `DEBUG_INFO`, `USB_DEBUG`, `KGDB`/`KGDB_KDB`, `FIQ_DEBUGGER`,
  `HIBERNATION`, `PM_DEBUG`/`ADVANCED_DEBUG`/`TEST_SUSPEND`,
  `EXT4_DEBUG`/`JBD_DEBUG`/`DM_DEBUG`, `FAULT_INJECTION` — all off, to
  shrink the kernel toward stock size and remove debug-build behavior
  drift. `MTK_AEE_FEATURE` and `MTK_RAM_CONSOLE` stay enabled
  (observability).
- **Pinned defconfig.** `kernel/arch/arm/configs/cci72_we_jb3_defconfig` is
  the single source of truth; round-trips with the merged `.config`.
  Generated artifacts (`.config`, `include/config/`, `include/generated/`)
  are not tracked — they regenerate from the defconfig.

## Requirements

- **A case-sensitive filesystem.** The tree contains files that differ
  only in case (e.g. `xt_HL.c` vs `xt_hl.c`); a case-insensitive volume
  (default macOS, some network shares) silently corrupts the checkout.
- **An x86_64 Linux host.**
- **A period ARM cross-toolchain.** Linaro 4.9 `arm-eabi` works (the
  original BSP used AOSP `arm-eabi-4.6`). Modern GCC (10+) cannot build
  this 3.4 kernel without extensive patching.

Build dependencies (Rocky/RHEL):

```
sudo dnf install -y gcc gcc-c++ make bc bison flex perl tar xz
```

## Quick start

Get a toolchain:

```
wget https://releases.linaro.org/components/toolchain/binaries/4.9-2017.01/arm-eabi/gcc-linaro-4.9.4-2017.01-x86_64_arm-eabi.tar.xz
tar -xf gcc-linaro-4.9.4-2017.01-x86_64_arm-eabi.tar.xz
```

Build, from the `kernel/` directory:

```
cd kernel
export ARCH=arm
export CROSS_COMPILE=$HOME/gcc-linaro-4.9.4-2017.01-x86_64_arm-eabi/bin/arm-eabi-
export TARGET_PRODUCT=cci72_we_jb3
export MTK_ROOT_CUSTOM=../mediatek/custom/
export MTK_PATH_PLATFORM=../mediatek/platform/mt6572/kernel/

make cci72_we_jb3_defconfig
make -j"$(nproc)"
```

The kernel image is `arch/arm/boot/zImage`. Wrap it with
`y1-platform/rockbox-boot/build-rockbox-boot.sh` (use `TARGET=recovery`) and
flash the result to the RECOVERY partition.

> `TARGET_PRODUCT` must be set or the MTK build files error out. If a stale
> `include/config/auto.conf.cmd` from a previous build blocks the config
> step, remove it and re-run `make cci72_we_jb3_defconfig`.

## Known open issues

- **Dark screen / Rockbox runs blind.** With the kernel booting from
  RECOVERY, init runs and execs `rockbox.y1`, but the panel scans out
  black instead of the Rockbox UI. The display engine completes init
  cleanly (one-time RDMA0 underflow at the LK→mtkfb handoff that doesn't
  repeat); suspects are the boot-logo handover (`mtkfb_set_lcm_inited`),
  Rockbox's fb-pan/buffer geometry, or backlight handoff via
  `disp_bls_set_backlight`. Reproducible with `DUMP_LOGS=1` in the
  y1-platform build for a full `Y1_dmesg.txt` capture.
- **Transparent BOOTIMG boot.** Flashing the same boot.img to BOOTIMG
  yields a kernel-side panic in `populate_rootfs`
  (`compression method lzma not configured` — the kernel sees `5d 00` at
  `initrd_start` even though our wrapped ramdisk has gzip at the expected
  offset). LK's BOOTIMG load path corrupts the ramdisk delivery in a way
  the RECOVERY load path doesn't. The two LK loader functions
  (`mboot_android_load_bootimg` @ `0x800221ec`, `mboot_android_load_recoveryimg`
  @ `0x80022308`) are structurally identical in disasm; the divergence is in
  either the partition-struct's read function (`[r8+0x10]` differs per
  partition) or post-load processing (`fcn.80037c04` is the ATAG builder,
  called only after BOOTIMG load). Parked pending a dedicated LK RE pass.
- **Stock kernel RE workspace.** `/work/stock-kernel-re/` has the
  decompressed stock kernel (`vmlinux_g368_nyx.bin`, VA base `0xC0008000`)
  and full 42,751-symbol kallsyms dump (`stock_syms.txt`) for any future
  function-level comparison. Stock and our build are *byte-for-byte
  identical* in the LCM drivers and the DSI PHY/PLL setup.
