#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"

#include "lv_demos.h"
#include "LCD_TK019F1935.h"
#include "LCD_TK035F5589.h"
#include "LCD_TK040F1510.h"
#include "LCD_TK019F1935_Touch.h"
#include "esp_lcd_touch_ft5x06.h"

#include "ui.h"
#include "myui.h"


/* LCD size */
#define EXAMPLE_LCD_H_RES   (800)
#define EXAMPLE_LCD_V_RES   (480)

#define CONFIG_EXAMPLE_LCD_I80_BUS_WIDTH    8
#define CONFIG_EXAMPLE_LCD_TOUCH_ENABLED    0       //Touch

// PCLK frequency can't go too high as the limitation of PSRAM bandwidth
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ     (20 * 1000 * 1000)

/* LCD pins */
#define EXAMPLE_PIN_NUM_DATA0          (GPIO_NUM_4)
#define EXAMPLE_PIN_NUM_DATA1          (GPIO_NUM_5)
#define EXAMPLE_PIN_NUM_DATA2          (GPIO_NUM_6)
#define EXAMPLE_PIN_NUM_DATA3          (GPIO_NUM_7)
#define EXAMPLE_PIN_NUM_DATA4          (GPIO_NUM_15)
#define EXAMPLE_PIN_NUM_DATA5          (GPIO_NUM_16)
#define EXAMPLE_PIN_NUM_DATA6          (GPIO_NUM_17)
#define EXAMPLE_PIN_NUM_DATA7          (GPIO_NUM_18)
#if CONFIG_EXAMPLE_LCD_I80_BUS_WIDTH > 8
#define EXAMPLE_PIN_NUM_DATA8          (GPIO_NUM_9)
#define EXAMPLE_PIN_NUM_DATA9          (GPIO_NUM_10)
#define EXAMPLE_PIN_NUM_DATA10         (GPIO_NUM_11)
#define EXAMPLE_PIN_NUM_DATA11         (GPIO_NUM_0)
#define EXAMPLE_PIN_NUM_DATA12         (GPIO_NUM_45)
#define EXAMPLE_PIN_NUM_DATA13         (GPIO_NUM_48)
#define EXAMPLE_PIN_NUM_DATA14         (GPIO_NUM_47)
#define EXAMPLE_PIN_NUM_DATA15         (GPIO_NUM_21)
#endif
#define EXAMPLE_PIN_NUM_PCLK           (GPIO_NUM_2)
#define EXAMPLE_PIN_NUM_CS             (GPIO_NUM_1)
#define EXAMPLE_PIN_NUM_DC             (GPIO_NUM_42)
#define EXAMPLE_PIN_NUM_RD             (GPIO_NUM_41)  //RD引脚一般不使用，可以直接接3.3V，省一个GPIO；（因为你写什么东西到屏上，单片机自己知道的，再去读的话浪费时间了。特殊颜色混合，并且显存紧张时，并且是裸编情况下可以使用RD）
#define EXAMPLE_PIN_NUM_RST            (GPIO_NUM_46) 
#define EXAMPLE_PIN_NUM_BK_LIGHT       (GPIO_NUM_NC)

// Bit number used to represent command and parameter
#define EXAMPLE_LCD_CMD_BITS           16
#define EXAMPLE_LCD_PARAM_BITS         16

//Touch
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
#define EXAMPLE_I2C_NUM                 1   // I2C number
#define EXAMPLE_I2C_SCL                 19
#define EXAMPLE_I2C_SDA                 20
#endif


/* Touch settings */
#define EXAMPLE_TOUCH_I2C_NUM       (1)
#define EXAMPLE_TOUCH_I2C_CLK_HZ    (400000)
/* LCD touch pins */
#define EXAMPLE_TOUCH_I2C_SCL       (GPIO_NUM_13)
#define EXAMPLE_TOUCH_I2C_SDA       (GPIO_NUM_20)
#define EXAMPLE_TOUCH_GPIO_INT      (GPIO_NUM_12)
    
// Supported alignment: 16, 32, 64. A higher alignment can enables higher burst transfer size, thus a higher i80 bus throughput.
#define EXAMPLE_PSRAM_DATA_ALIGNMENT   64

static const char *TAG = "MAIN";

/* LCD IO and panel */
static esp_lcd_panel_io_handle_t lcd_io = NULL;
static esp_lcd_panel_handle_t lcd_panel = NULL;
/* LCD Touch IO and panel */
static esp_lcd_panel_io_handle_t tp_io_handle = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
static esp_lcd_touch_handle_t tp_handle = NULL;
#endif

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
void lcd_touc_init(void) 
{
    i2c_master_bus_handle_t codec_i2c_bus;
    esp_lcd_panel_io_handle_t tp_io_handle;

    // Initialize I2C peripheral
    i2c_master_bus_config_t i2c_bus_cfg1 = {
        .i2c_port = EXAMPLE_I2C_NUM,
        .sda_io_num = EXAMPLE_I2C_SDA,
        .scl_io_num = EXAMPLE_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = {
            .enable_internal_pullup = 1,
        },
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg1, &codec_i2c_bus));

    esp_lcd_panel_io_i2c_config_t tp_io_config = {
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS, 
        .control_phase_bytes = 1,           
        .dc_bit_offset = 0,                 
        .lcd_cmd_bits = 8,                  
        .flags =                            
        {                                   
            .disable_control_phase = 1,     
        },
        .scl_speed_hz = 400000,                              
    };

    ESP_LOGI(TAG, "Initialize touch IO (I2C)");

    /* Touch IO handle */
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(codec_i2c_bus, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_V_RES,
        .y_max = EXAMPLE_LCD_H_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = GPIO_NUM_NC,
        .flags = {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };

    /* Initialize touch */
    ESP_LOGI(TAG, "Initialize touch controller FT5x06");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &tp_handle));
}
#endif

static esp_err_t lcd_init(void)
{
    /* LCD backlight */
    if (EXAMPLE_PIN_NUM_RD >= 0) {
        gpio_config_t bk_gpio_config = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << EXAMPLE_PIN_NUM_RD
        };
        ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
        gpio_set_level(EXAMPLE_PIN_NUM_RD, 1);
    }

        // 液晶屏控制IO初始化
    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus =  NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .dc_gpio_num = EXAMPLE_PIN_NUM_DC,
        .wr_gpio_num = EXAMPLE_PIN_NUM_PCLK,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .data_gpio_nums = {
            EXAMPLE_PIN_NUM_DATA0,
            EXAMPLE_PIN_NUM_DATA1,
            EXAMPLE_PIN_NUM_DATA2,
            EXAMPLE_PIN_NUM_DATA3,
            EXAMPLE_PIN_NUM_DATA4,
            EXAMPLE_PIN_NUM_DATA5,
            EXAMPLE_PIN_NUM_DATA6,
            EXAMPLE_PIN_NUM_DATA7,
#if CONFIG_EXAMPLE_LCD_I80_BUS_WIDTH > 8
            EXAMPLE_PIN_NUM_DATA8,
            EXAMPLE_PIN_NUM_DATA9,
            EXAMPLE_PIN_NUM_DATA10,
            EXAMPLE_PIN_NUM_DATA11,
            EXAMPLE_PIN_NUM_DATA12,
            EXAMPLE_PIN_NUM_DATA13,
            EXAMPLE_PIN_NUM_DATA14,
            EXAMPLE_PIN_NUM_DATA15,
#endif
        },
        .bus_width = CONFIG_EXAMPLE_LCD_I80_BUS_WIDTH,
        .max_transfer_bytes = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t),
        .psram_trans_align = EXAMPLE_PSRAM_DATA_ALIGNMENT,
        .sram_trans_align = 4,
    };

    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = EXAMPLE_PIN_NUM_CS,
        .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
        .lcd_cmd_bits = EXAMPLE_LCD_CMD_BITS,
        .lcd_param_bits = EXAMPLE_LCD_PARAM_BITS,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, &lcd_io));

    // 初始化液晶屏驱动芯片
    ESP_LOGD(TAG, "Install LCD driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_TK040F1510(lcd_io, &panel_config, &lcd_panel));

    esp_lcd_panel_reset(lcd_panel);
    esp_lcd_panel_init(lcd_panel);
    esp_lcd_panel_invert_color(lcd_panel, false);
    esp_lcd_panel_disp_on_off(lcd_panel, true);

    return ESP_OK;
}

static esp_err_t app_touch_init(void)
{
    /* Initilize I2C */
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = EXAMPLE_TOUCH_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = EXAMPLE_TOUCH_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = EXAMPLE_TOUCH_I2C_CLK_HZ
    };
    ESP_RETURN_ON_ERROR(i2c_param_config(EXAMPLE_TOUCH_I2C_NUM, &i2c_conf), TAG, "I2C configuration failed");
    ESP_RETURN_ON_ERROR(i2c_driver_install(EXAMPLE_TOUCH_I2C_NUM, i2c_conf.mode, 0, 0, 0), TAG, "I2C initialization failed");

    /* Initialize touch HW */
    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = EXAMPLE_LCD_V_RES,
        .y_max = EXAMPLE_LCD_H_RES,
        .rst_gpio_num = GPIO_NUM_NC, // Shared with LCD reset
        .int_gpio_num = GPIO_NUM_NC,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };
    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)EXAMPLE_TOUCH_I2C_NUM, &tp_io_config, &tp_io_handle), TAG, "");
    return esp_lcd_touch_new_i2c_ft5x06(tp_io_handle, &tp_cfg, &touch_handle);
}

static esp_err_t lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,         /* LVGL task priority */
        .task_stack = 8196,         /* LVGL task stack size */
        .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
        .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = lcd_io,
        .panel_handle = lcd_panel,
        .buffer_size = EXAMPLE_LCD_H_RES * 10 * sizeof(uint16_t),
        .double_buffer = true,
        .trans_size = false,
        .hres = EXAMPLE_LCD_H_RES,
        .vres = EXAMPLE_LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        /* Rotation values must be same as used in esp_lcd for initial settings of the screen */
        .rotation = {
            .swap_xy = true,
            .mirror_x = false,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = 0,
            .sw_rotate = 0,
            .swap_bytes = 1,
            .full_refresh = 0,
            .direct_mode = 0,
        }
    };

    lvgl_disp = lvgl_port_add_disp(&disp_cfg);
    if (lvgl_disp == NULL) {
        ESP_LOGE(TAG, "Failed to add display");
        return ESP_FAIL;
    }

#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = tp_handle,
    };

    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);
#endif

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);


    return ESP_OK;
}

//生成电量
void battery_show()
{
    //递增电量
    static int num = 0;
    num +=10;
    if(num > 100)
        num = 0;

    quantity_electricity_show(num);    //显示电池
}

//随机生成速度
void speed_show()
{
    // 设置随机种子（使用当前时间）
    srand(time(0));
    // 生成随机时速
    int random_speed1 = rand() % 220;
    int random_speed2 = rand() % 220;

    Code_table_show(random_speed1, random_speed2);   //显示时速
}

//随机生difference
void difference_show1()
{
    // 设置随机种子（使用当前时间）
    srand(time(0));
    // 生成随机时速
    int difference = rand() % 600;

    difference_show(difference);   //显示difference
}

//随机生成数组
void chart_up_show()
{
    // 设置随机种子（使用当前时间）
    srand(time(0));

    uint16_t point_cnt = lv_chart_get_point_count(ui_Chart1);
    
    // 生成模拟频谱数据（0-50)
    int32_t spectrum_data[point_cnt];
    for (int i = 0; i < point_cnt; i++) {
        spectrum_data[i] = rand() % 40;
    }

    // 批量更新图表
    chart_data_bulk_update(spectrum_data);
}

//随机生其他数据
void data_show()
{
    srand(time(0));
    
    int data = rand() % 1000;
    production_show(data);//显示production

    data = rand() % 1000;
    home_show(data);//显示home

    data = rand() % 1000;
    appliance_show(data);//显示appliance
}

//系统初始化任务
static void init_task(void *arg)
{

    lv_timer_t * battery_up = lv_timer_create(battery_show, 500, NULL);//电池刷新显示
    lv_timer_ready(battery_up);

    lv_timer_t * speed_up = lv_timer_create(speed_show, 1500, NULL);//时速刷新显示
    lv_timer_ready(speed_up);

    lv_timer_t * difference_up = lv_timer_create(difference_show1, 2000, NULL);//difference刷新显示
    lv_timer_ready(difference_up);

    //lv_timer_t * chart_up = lv_timer_create(chart_up_show, 600, NULL);//chart刷新显示
    //lv_timer_ready(chart_up);

    lv_timer_t * data_up = lv_timer_create(data_show, 2500, NULL);//其他数据刷新显示
    lv_timer_ready(data_up);

    key_create(ui_Screen2, ui_TextArea3, ui_TextArea4);  //键盘创建

    vTaskDelete(NULL);
}

static void app_main_display(void)
{
    /* Task lock */
    lvgl_port_lock(0);

    /* LVGL demo */
    //lv_demo_widgets();
    //lv_demo_stress();

    ui_init();

    /* Task unlock */
    lvgl_port_unlock();
}

void app_main(void)
{
#if CONFIG_EXAMPLE_LCD_TOUCH_ENABLED
    /* Touch initialization */
    lcd_touc_init();
#endif



    ESP_LOGI(TAG, "Initilize LCD.");

    /* LCD HW initialization */
    ESP_ERROR_CHECK(lcd_init());
    ESP_ERROR_CHECK(app_touch_init()); 
    ESP_LOGI(TAG, "Initilize LVGL.");

    /* LVGL initialization */
    ESP_ERROR_CHECK(lvgl_init());

    /* Show LVGL objects */
    app_main_display();

    xTaskCreate(init_task, "init_task", 1024*4, NULL, 10, NULL);
}
