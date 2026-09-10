#include "ds2484.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c.h"

#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SDA_IO 32
#define I2C_MASTER_SCL_IO 33
#define I2C_MASTER_FREQ_HZ 100000

#define DS248X_ADDRESS 0x18

#define DS248X_CMD_RESET 0xF0
#define DS248X_CMD_SET_READ_PTR 0xE1
#define DS248X_CMD_WRITE_CONFIG 0xD2
#define DS248X_CMD_1WIRE_RESET 0xB4
#define DS248X_CMD_1WIRE_SINGLE_BIT 0x87
#define DS248X_CMD_1WIRE_WRITE_BYTE 0xA5
#define DS248X_CMD_1WIRE_READ_BYTE 0x96

#define DS248X_REG_STATUS 0xF0
#define DS248X_REG_READ_DATA 0xE1
#define DS248X_REG_CONFIG 0xC3

static const char *TAG = "DS2484_DRIVER";

static uint32_t millis(void) {
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

static bool ds248_i2c_write(const uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS248X_ADDRESS << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, (uint8_t *)data, len, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

static bool ds248_i2c_read(uint8_t *data, size_t len) {
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS248X_ADDRESS << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, len, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    return (ret == ESP_OK);
}

static bool set_read_pointer(uint8_t reg) {
    uint8_t cmd[2] = {DS248X_CMD_SET_READ_PTR, reg};
    return ds248_i2c_write(cmd, 2);
}

static uint8_t read_status(void) {
    if (!set_read_pointer(DS248X_REG_STATUS)) return 0xFF;
    uint8_t status;
    if (!ds248_i2c_read(&status, 1)) return 0xFF;
    return status;
}

static bool is_1w_busy(void) {
    uint8_t status = read_status();
    return status != 0xFF && (status & 0x01);
}

static bool busy_wait(uint16_t timeout_ms) {
    uint32_t start = millis();
    while ((millis() - start) < timeout_ms) {
        if (!is_1w_busy()) return true;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
}

esp_err_t ds2484_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

bool ds2484_reset(void) {
    uint8_t cmd = DS248X_CMD_RESET;
    if (!ds248_i2c_write(&cmd, 1)) return false;
    uint8_t status = read_status();
    return (status != 0xFF) && (status & 0x10);
}

static bool write_config(uint8_t config) {
    if (!busy_wait(1000)) return false;
    uint8_t config_value = (config & 0x0F) | ((~config & 0x0F) << 4);
    uint8_t cmd[2] = {DS248X_CMD_WRITE_CONFIG, config_value};
    return ds248_i2c_write(cmd, 2);
}

bool ds2484_active_pullup(bool enable) {
    if (!set_read_pointer(DS248X_REG_CONFIG)) return false;
    uint8_t config;
    if (!ds248_i2c_read(&config, 1)) return false;
    if (enable) config |= 0x01; else config &= ~0x01;
    return write_config(config);
}

bool ds2484_onewire_reset(void) {
    if (!busy_wait(1000)) return false;
    uint8_t cmd = DS248X_CMD_1WIRE_RESET;
    if (!ds248_i2c_write(&cmd, 1)) return false;
    if (!busy_wait(1000)) return false;
    uint8_t status = read_status();
    return (status != 0xFF) && !(status & 0x04) && (status & 0x02);
}

bool ds2484_onewire_write_byte(uint8_t byte) {
    if (!busy_wait(1000)) return false;
    uint8_t cmd[2] = {DS248X_CMD_1WIRE_WRITE_BYTE, byte};
    if (!ds248_i2c_write(cmd, 2)) return false;
    return busy_wait(1000);
}

bool ds2484_onewire_read_byte(uint8_t *byte) {
    if (!busy_wait(1000)) return false;
    uint8_t cmd = DS248X_CMD_1WIRE_READ_BYTE;
    if (!ds248_i2c_write(&cmd, 1)) return false;
    if (!busy_wait(1000)) return false;
    if (!set_read_pointer(DS248X_REG_READ_DATA)) return false;
    return ds248_i2c_read(byte, 1);
}

bool ds2484_onewire_write_bit(bool bit) {
    if (!busy_wait(1000)) return false;
    uint8_t cmd[2] = {DS248X_CMD_1WIRE_SINGLE_BIT, bit ? (uint8_t)0x80 : (uint8_t)0x00};
    return ds248_i2c_write(cmd, 2);
}

bool ds2484_onewire_read_bit(uint8_t *bit) {
    if (!busy_wait(1000)) return false;
    if (!ds2484_onewire_write_bit(1)) return false;
    if (!busy_wait(1000)) return false;
    uint8_t status = read_status();
    if (status == 0xFF) return false;
    *bit = (status & 0x20) ? 1 : 0;
    return true;
}