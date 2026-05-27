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
# MTK_BQ24196_SUPPORT: usb20.c uses the BQ24196 charger's OTG boost for USB-host
# VBUS instead of an undefined board GPIO. The Y1 has this charger.
MTK_CDEFS += -DMTK_BQ24196_SUPPORT
