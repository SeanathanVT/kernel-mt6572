# MTK global compile-time defines for the standalone cci72_we_jb3 kernel build.
#
# kernel/Makefile does '-include $(MTK_PROJECT)_mtk_cust.mak'. In a full MTK/
# Android build, codegen generates this from ProjectConfig.mk's
# AUTO_ADD_GLOBAL_DEFINE_* lists. Emitting all ~195 of those is wrong for a
# standalone *kernel* build: many enable code for hardware this music-player
# target doesn't have and that references undefined platform symbols (e.g.
# MTK_TVOUT_SUPPORT pulls TVC_BASE/TV_ROT_BASE/TVE_BASE into mt_devs.c), and
# several valued ones (LCM_WIDTH, MTK_SIM*_SOCKET_TYPE, ...) conflict with a
# definition another -D source already provides. The tree in fact compiles
# cleanly with no MTK defines up to the one spot that needs one, so define only
# the flags the kernel code we actually build requires, and add more only if a
# specific build error demands it.
#
# Use MTK_CDEFS only (-> KBUILD_CFLAGS, reaches C compilation); adding the same
# define via MTK_CPPDEFS/MTK_ADEFS too produces "<command-line>: X redefined".
#
# The Y1's charger IC is the FAN5405 (confirmed from a stock-kernel string:
# "power off FAN5405 and system"). BQ24196/NCP1851 are also in the generic
# AUTO_ADD list but the Y1 doesn't use them, and their drivers are shipped
# incomplete in this GPL drop (missing mt6320_battery.h / ncp1851.h).
#
# MTK_FAN5405_SUPPORT does double duty: a make variable that gates building the
# FAN5405 driver (power/Makefile: fan5405.o charging_hw_fan5405.o) which defines
# fan5405_set_otg_en, AND a -D so usb20.c/musb_otg_if.c take the FAN5405 branch
# (fan5405_set_otg_en for USB-host OTG VBUS, not the undefined board GPIO or the
# uncompilable bq24196 path). Need both, or the callers compile but the
# definition is never built/linked.
MTK_FAN5405_SUPPORT := yes
export MTK_FAN5405_SUPPORT
MTK_CDEFS += -DMTK_FAN5405_SUPPORT

# fan5405.c quote-includes "cust_charging.h", which lives in the platform's
# custom battery dir (not on the standalone build's include path, since no
# natural-built file referenced it). Add it so the FAN5405 driver compiles.
MTK_INC += -I$(MTK_ROOT_CUSTOM)/mt6572/kernel/battery/battery
