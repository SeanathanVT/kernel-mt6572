// ST7701(S) 480x360 MIPI-DSI video-mode panel (BOE), as used on the Innioasis
// Y1 (MTK project g368_nyx).  Auto-detected at boot against
// gc9503v_hvga_dsi_vdo_hsd via compare_id, exactly as the stock LK does.
//
// Init sequence, DSI timing, reset timing and compare_id recovered byte-for-byte
// from the device's own stock lk.bin (see gc9503v_hvga_dsi_vdo_hsd.c for the
// rationale: the g368_nyx BSP is not public and this panel ships with neither
// the cci72_we_jb3 GPL drop nor any public MTK tree).

#ifdef BUILD_LK
#else
    #include <linux/string.h>
    #if defined(BUILD_UBOOT)
        #include <asm/arch/mt_gpio.h>
    #else
        #include <mach/mt_gpio.h>
    #endif
#endif
#include "lcm_drv.h"

// ---------------------------------------------------------------------------
//  Local Constants
// ---------------------------------------------------------------------------

#define FRAME_WIDTH  (480)
#define FRAME_HEIGHT (360)

#define REGFLAG_DELAY          0xAB
#define REGFLAG_END_OF_TABLE   0xAA   // END OF REGISTERS MARKER

// ---------------------------------------------------------------------------
//  Local Variables
// ---------------------------------------------------------------------------

static LCM_UTIL_FUNCS lcm_util = {0};

#define SET_RESET_PIN(v)    (lcm_util.set_reset_pin((v)))
#define UDELAY(n)           (lcm_util.udelay(n))
#define MDELAY(n)           (lcm_util.mdelay(n))

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update) \
        lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update) \
        lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size) \
        lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};

// Initialization sequence carved from stock lk.bin (47 entries, table @ VA
// 0x80059548).  ST7701S BKx bank-select via 0xFF 77 01 ...; ends in Sleep-Out
// (0x11) + Display-On (0x29).
static struct LCM_setting_table lcm_initialization_setting[] = {
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x13}},
    {0xEF, 1, {0x08}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x10}},
    {0xC0, 2, {0x2C, 0x00}},
    {0xC1, 2, {0x10, 0x0C}},
    {0xC2, 2, {0x21, 0x0A}},
    {0xCC, 1, {0x10}},
    {0xB0, 16, {0x00, 0x0B, 0x12, 0x0D, 0x10, 0x06, 0x02, 0x08, 0x07, 0x1F, 0x04, 0x11, 0x0F, 0x29, 0x31, 0x1E}},
    {0xB1, 16, {0x00, 0x0B, 0x13, 0x0D, 0x11, 0x06, 0x03, 0x08, 0x07, 0x20, 0x04, 0x12, 0x11, 0x29, 0x31, 0x1E}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x11}},
    {0xB0, 1, {0x5D}},
    {0xB1, 1, {0x72}},
    {0xB2, 1, {0x84}},
    {0xB3, 1, {0x80}},
    {0xB5, 1, {0x4D}},
    {0xB7, 1, {0x85}},
    {0xB8, 1, {0x20}},
    {0xB9, 1, {0x10}},
    {0xC1, 1, {0x78}},
    {0xC2, 1, {0x78}},
    {0xD0, 1, {0x88}},
    {REGFLAG_DELAY, 100, {}},
    {0xE0, 3, {0x80, 0x00, 0x02}},
    {0xE1, 11, {0x05, 0x00, 0x07, 0x00, 0x06, 0x00, 0x08, 0x00, 0x00, 0x33, 0x33}},
    {0xE2, 12, {0x00, 0x00, 0x30, 0x30, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}},
    {0xE3, 4, {0x00, 0x00, 0x11, 0x11}},
    {0xE4, 2, {0x44, 0x44}},
    {0xE5, 16, {0x0C, 0x78, 0x00, 0xE0, 0x0E, 0x7A, 0x00, 0xE0, 0x08, 0x74, 0x00, 0xE0, 0x0A, 0x76, 0x00, 0xE0}},
    {0xE6, 4, {0x00, 0x00, 0x11, 0x11}},
    {0xE7, 2, {0x44, 0x44}},
    {0xE8, 16, {0x0D, 0x79, 0x00, 0xE0, 0x0F, 0x7B, 0x00, 0xE0, 0x09, 0x75, 0x00, 0xE0, 0x0B, 0x77, 0x00, 0xE0}},
    {0xE9, 2, {0x36, 0x00}},
    {0xEB, 7, {0x00, 0x01, 0xE4, 0xE4, 0x44, 0x88, 0x40}},
    {0xED, 16, {0xA1, 0xC2, 0xFB, 0x0F, 0x67, 0x45, 0xFF, 0xFF, 0xFF, 0xFF, 0x54, 0x76, 0xF0, 0xBF, 0x2C, 0x1A}},
    {0xEF, 6, {0x10, 0x0D, 0x04, 0x08, 0x3F, 0x1F}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x13}},
    {0xE8, 2, {0x00, 0x0E}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}},
    {0x11, 0, {}},
    {REGFLAG_DELAY, 120, {}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x13}},
    {0xE8, 2, {0x00, 0x0C}},
    {REGFLAG_DELAY, 10, {}},
    {0xE8, 2, {0x00, 0x00}},
    {0xFF, 5, {0x77, 0x01, 0x00, 0x00, 0x00}},
    {0x29, 0, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static struct LCM_setting_table lcm_deep_sleep_mode_in_setting[] = {
    {0x28, 0, {}},
    {REGFLAG_DELAY, 10, {}},
    {0x10, 0, {}},
    {REGFLAG_DELAY, 120, {}},
    {REGFLAG_END_OF_TABLE, 0x00, {}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        unsigned cmd = table[i].cmd;
        switch (cmd) {
            case REGFLAG_DELAY:
                MDELAY(table[i].count);
                break;
            case REGFLAG_END_OF_TABLE:
                break;
            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

// ---------------------------------------------------------------------------
//  LCM Driver Implementations
// ---------------------------------------------------------------------------
static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));

    params->type   = LCM_TYPE_DSI;
    params->width  = FRAME_WIDTH;
    params->height = FRAME_HEIGHT;

    params->dsi.mode = SYNC_PULSE_VDO_MODE;
    params->dsi.LANE_NUM = LCM_TWO_LANE;

    params->dsi.data_format.color_order = LCM_COLOR_ORDER_RGB;
    params->dsi.data_format.trans_seq   = LCM_DSI_TRANS_SEQ_MSB_FIRST;
    params->dsi.data_format.padding     = LCM_DSI_PADDING_ON_LSB;
    params->dsi.data_format.format      = LCM_DSI_FORMAT_RGB888;

    params->dsi.intermediat_buffer_num = 2;
    params->dsi.PS = LCM_PACKED_PS_24BIT_RGB888;

    params->dsi.word_count  = FRAME_WIDTH * 3;
    params->dsi.packet_size = 256;

    params->dsi.vertical_sync_active  = 8;
    params->dsi.vertical_backporch    = 100;
    params->dsi.vertical_frontporch   = 100;
    params->dsi.vertical_active_line  = FRAME_HEIGHT;

    params->dsi.horizontal_sync_active  = 10;
    params->dsi.horizontal_backporch    = 80;
    params->dsi.horizontal_frontporch   = 80;
    params->dsi.horizontal_active_pixel = FRAME_WIDTH;

    // PLL config (stock values; fref=26MHz, fvco=fref*(fbk_div+1)*2/div)
    params->dsi.pll_div1 = 2;
    params->dsi.pll_div2 = 0;
    params->dsi.fbk_div  = 15;
}

static void lcm_init(void)
{
    SET_RESET_PIN(1);
    MDELAY(20);
    SET_RESET_PIN(0);
    MDELAY(50);
    SET_RESET_PIN(1);
    MDELAY(150);

    push_table(lcm_initialization_setting,
               sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_suspend(void)
{
    push_table(lcm_deep_sleep_mode_in_setting,
               sizeof(lcm_deep_sleep_mode_in_setting) / sizeof(struct LCM_setting_table), 1);
}

static void lcm_resume(void)
{
    lcm_init();
}

// Read DA register 0xA1 (4 bytes); ST7701 reports id (buf[0]<<8 | buf[1]) of
// 0x8800 or 0x9900.
static unsigned int lcm_compare_id(void)
{
    unsigned int id;
    unsigned char buffer[4];
    unsigned int data_array[16];

    SET_RESET_PIN(1);
    MDELAY(20);
    SET_RESET_PIN(0);
    MDELAY(50);
    SET_RESET_PIN(1);
    MDELAY(150);

    data_array[0] = 0x00013700;   // set max return packet size = 1
    dsi_set_cmdq(data_array, 1, 1);

    read_reg_v2(0xA1, buffer, 4);
    id = ((unsigned int)buffer[0] << 8) | buffer[1];

    return (id == 0x8800 || id == 0x9900) ? 1 : 0;
}

// ---------------------------------------------------------------------------
//  Get LCM Driver Hooks
// ---------------------------------------------------------------------------
LCM_DRIVER st7701_hvga_dsi_vdo_boe_lcm_drv =
{
    .name           = "st7701_hvga_dsi_vdo_boe",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params     = lcm_get_params,
    .init           = lcm_init,
    .suspend        = lcm_suspend,
    .resume         = lcm_resume,
    .compare_id     = lcm_compare_id,
};
