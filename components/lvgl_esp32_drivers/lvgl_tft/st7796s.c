/**
 * @file st7796s.c
 *
 * Mostly taken from lbthomsen/esp-idf-littlevgl github.
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "esp_log.h"

#include "st7796s.h"

#include "disp_spi.h"
#include "driver/gpio.h"

/*********************
 *      DEFINES
 *********************/
#define TAG "st7796s"
/**********************
 *      TYPEDEFS
 **********************/

/*The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct. */
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void st7796s_set_orientation(uint8_t orientation);

static void st7796s_send_cmd(uint8_t cmd);
static void st7796s_send_data(void *data, uint16_t length);
static void st7796s_send_color(void *data, uint16_t length);

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
void st7796s_init(void)
{
    lcd_init_cmd_t st7796s_init_cmds[] = {
        {0x11, {0}, 0x80},
        {0xF0, {0xc3}, 1},
        {0xf0, {0x96}, 1},
        {0x36, {0x28}, 1},
        {0x3a, {0x55}, 1},
        {0xb4, {0x01}, 1},
        {0xb7, {0xc6}, 1},
        {0xe8, {0x40,0x8a,0x00,0x00,0x29,0x19,0xa5,0x33}, 8},
        {0xc1, {0x06}, 1},
        {0xc2, {0xa7}, 1},
        {0xc5, {0x18}, 1}, 
        {0xe0, {0xf0,0x09,0x0b,0x06,0x04,0x15,0x2f,0x54,0x42,0x3c,0x17,0x14,0x18,0x1b}, 14},
        {0xe1, {0xf0,0x09,0x0b,0x06,0x04,0x03,0x2d,0x43,0x42,0x3b,0x16,0x14,0x17,0x1b}, 14},
        {0xF0, {0x3c}, 1},
        {0xF0, {0x69}, 0x81},
        {0x29, {0}, 0},
        {0x2c, {0}, 0},
        {0, {0}, 0xff},
        /*
        {0xCF, {0x00, 0x83, 0X30}, 3},
        {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
        {st7796s_PWCTRL2, {0x85, 0x01, 0x79}, 3},
        {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
        {0xF7, {0x20}, 1},
        {0xEA, {0x00, 0x00}, 2},
        {st7796s_LCMCTRL, {0x26}, 1},
        {st7796s_IDSET, {0x11}, 1},
        {st7796s_VCMOFSET, {0x35, 0x3E}, 2},
        {st7796s_CABCCTRL, {0xBE}, 1},
        {st7796s_MADCTL, {0x00}, 1}, // Set to 0x28 if your display is flipped
        {st7796s_COLMOD, {0x55}, 1},
	{st7796s_INVON, {0}, 0},
        {st7796s_RGBCTRL, {0x00, 0x1B}, 2},
        {0xF2, {0x08}, 1},
        {st7796s_GAMSET, {0x01}, 1},
        {st7796s_PVGAMCTRL, {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x32, 0x44, 0x42, 0x06, 0x0E, 0x12, 0x14, 0x17}, 14},
        {st7796s_NVGAMCTRL, {0xD0, 0x00, 0x02, 0x07, 0x0A, 0x28, 0x31, 0x54, 0x47, 0x0E, 0x1C, 0x17, 0x1B, 0x1E}, 14},
        {st7796s_CASET, {0x00, 0x00, 0x00, 0xEF}, 4},
        {st7796s_RASET, {0x00, 0x00, 0x01, 0x3f}, 4},
        {st7796s_RAMWR, {0}, 0},
        {st7796s_GCTRL, {0x07}, 1},
        {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
        {st7796s_SLPOUT, {0}, 0x80},
        {st7796s_DISPON, {0}, 0x80},
        {0, {0}, 0xff},
        */
    };

    //Initialize non-SPI GPIOs
    gpio_set_direction(ST7796S_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(ST7796S_RST, GPIO_MODE_OUTPUT);
    
#if ST7796S_ENABLE_BACKLIGHT_CONTROL
    gpio_set_direction(ST7796S_BCKL, GPIO_MODE_OUTPUT);
#endif

    //Reset the display
    gpio_set_level(ST7796S_RST, 0);
    vTaskDelay(200 / portTICK_PERIOD_MS);
    gpio_set_level(ST7796S_RST, 1);
    vTaskDelay(200 / portTICK_PERIOD_MS);

    printf("st7796s initialization.\n");

    //Send all the commands
    uint16_t cmd = 0;
    while (st7796s_init_cmds[cmd].databytes!=0xff) {
        st7796s_send_cmd(st7796s_init_cmds[cmd].cmd);
        st7796s_send_data(st7796s_init_cmds[cmd].data, st7796s_init_cmds[cmd].databytes&0x1F);
        if (st7796s_init_cmds[cmd].databytes & 0x80) {
                vTaskDelay(150 / portTICK_PERIOD_MS);
        }
        cmd++;
    }

    st7796s_enable_backlight(true);

    //st7796s_set_orientation(CONFIG_LVGL_DISPLAY_ORIENTATION);
}

void st7796s_enable_backlight(bool backlight)
{
#if ST7796S_ENABLE_BACKLIGHT_CONTROL
    printf("%s backlight.\n", backlight ? "Enabling" : "Disabling");
    uint32_t tmp = 0;

#if (ST7796S_BCKL_ACTIVE_LVL==1)
    tmp = backlight ? 1 : 0;
#else
    tmp = backlight ? 0 : 1;
#endif

    gpio_set_level(ST7796S_BCKL, tmp);
#endif
}

/* The st7796s display controller can drive 320*240 displays, when using a 240*240
 * display there's a gap of 80px, we need to edit the coordinates to take into
 * account that gap, this is not necessary in all orientations. */
void st7796s_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
    uint8_t data[4] = {0};

    uint16_t offsetx1 = area->x1;
    uint16_t offsetx2 = area->x2;
    uint16_t offsety1 = area->y1;
    uint16_t offsety2 = area->y2;

#if (LV_HOR_RES_MAX == 240) && (LV_VER_RES_MAX == 240)
#if (CONFIG_LVGL_DISPLAY_ORIENTATION_PORTRAIT)
    offsetx1 += 80;
    offsetx2 += 80;
#elif (CONFIG_LVGL_DISPLAY_ORIENTATION_LANDSCAPE_INVERTED)
    offsety1 += 80;
    offsety2 += 80;
#endif
#endif

    /*Column addresses*/
    st7796s_send_cmd(ST7796S_CASET);
    data[0] = (offsetx1 >> 8) & 0xFF;
    data[1] = offsetx1 & 0xFF;
    data[2] = (offsetx2 >> 8) & 0xFF;
    data[3] = offsetx2 & 0xFF;
    st7796s_send_data(data, 4);

    /*Page addresses*/
    st7796s_send_cmd(ST7796S_RASET);
    data[0] = (offsety1 >> 8) & 0xFF;
    data[1] = offsety1 & 0xFF;
    data[2] = (offsety2 >> 8) & 0xFF;
    data[3] = offsety2 & 0xFF;
    st7796s_send_data(data, 4);

    /*Memory write*/
    st7796s_send_cmd(ST7796S_RAMWR);

    uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);

    st7796s_send_color((void*)color_map, size * 2);

}

/**********************
 *   STATIC FUNCTIONS
 **********************/
static void st7796s_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ST7796S_DC, 0);
    disp_spi_send_data(&cmd, 1);
}

static void st7796s_send_data(void * data, uint16_t length)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ST7796S_DC, 1);
    disp_spi_send_data(data, length);
}

static void st7796s_send_color(void * data, uint16_t length)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ST7796S_DC, 1);
    disp_spi_send_colors(data, length);
}

static void st7796s_set_orientation(uint8_t orientation)
{
    // ESP_ASSERT(orientation < 4);

    const char *orientation_str[] = {
        "PORTRAIT", "PORTRAIT_INVERTED", "LANDSCAPE", "LANDSCAPE_INVERTED"
    };

    ESP_LOGI(TAG, "Display orientation: %s", orientation_str[orientation]);

#if defined (CONFIG_LVGL_PREDEFINED_DISPLAY_NONE)
    uint8_t data[] = {0xC0, 0x00, 0x60, 0xA0};
#endif

    ESP_LOGI(TAG, "0x36 command value: 0x%02X", data[orientation]);

    st7796s_send_cmd(ST7796S_MADCTL);
    st7796s_send_data((void *) &data[orientation], 1);
}
