# MT6572 kernel — Innioasis Y1 (Rockbox)

MediaTek MT6572 BSP kernel (Linux 3.4.5), forked from the stock Acer/MTK
source and reconfigured for the native Rockbox port of the Innioasis Y1.
The build target ("product") is `cci72_we_jb3`.

## Changes from the stock BSP

- **USB mass storage exposes a single LUN.** The stock gadget exposes a
  second, empty LUN that many car head units reject; this exposes only the
  SD card, so it enumerates as a plain USB flash drive.
- **In-kernel Bluetooth (BlueZ) + virtual HCI.** Stock Android drove the
  CONSYS combo chip from userspace and left the kernel BT subsystem off.
  It's enabled here, with a virtual HCI so a userspace H4 shim can bridge
  `/dev/stpbt` to `hci0` for BlueZ.
- **USB host/OTG + USB Audio Class.** Lets a USB-C DAC or USB-C headphones
  be used for digital audio out.
- **Correct display panel.** Stock `cci72_we_jb3` targets the `nt35510`
  (480×800 command-mode) panel the Y1 doesn't have, so the rebuilt kernel hung
  in display init and reset. The Y1 uses a 480×360 DSI *video*-mode panel —
  GC9503V or ST7701 — auto-detected at boot via `compare_id`. Both drivers were
  reverse-engineered from the stock bootloader
  (`mediatek/custom/common/kernel/lcm/{gc9503v_hvga_dsi_vdo_hsd,st7701_hvga_dsi_vdo_boe}`);
  selected in `mediatek/config/cci72_we_jb3/ProjectConfig.mk` (`CUSTOM_KERNEL_LCM`).
- **Charger = FAN5405.** `MTK_FAN5405_SUPPORT=yes` in `ProjectConfig.mk` (not
  the stock-default BQ24196/NCP1851, whose drivers are incomplete in this drop);
  also gates the USB-host OTG VBUS path.
- **Watchdog disabled (bring-up).** `mtk_wdt_probe` forces the WDT off so a
  userspace that doesn't yet kick `/dev/watchdog` (Rockbox) isn't reset ~30 s
  in. **Revert once Rockbox owns the watchdog.**
- **Pinned config.** The device config is
  `arch/arm/configs/cci72_we_jb3_defconfig`. Generated artifacts
  (`.config`, `include/config/`, `include/generated/`) are not tracked —
  they regenerate from the defconfig.

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

The kernel image is `arch/arm/boot/zImage`. Wrap it into a boot image with
your existing flash flow.

> `TARGET_PRODUCT` must be set or the MTK build files error out. If a stale
> `include/config/auto.conf.cmd` from a previous build blocks the config
> step, remove it and re-run `make cci72_we_jb3_defconfig`.
