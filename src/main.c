#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ds2484.h"
#include "onewire_search.h"
#include "ds18b20.h"

static const char *TAG = "MAIN";

void app_main(void) {
    ESP_LOGI(TAG, "Старт системы мониторинга температуры...");

    ESP_ERROR_CHECK(ds2484_init());

    if (!ds2484_reset()) {
        ESP_LOGE(TAG, "Ошибка инициализации DS2484!");
        return;
    }
    
    ds2484_active_pullup(true);
    ESP_LOGI(TAG, "DS2484 готов, активная подтяжка включена.");

    while (1) {
        // 1. Глобальный запуск конвертации для всех датчиков (Skip ROM)
        if (ds2484_onewire_reset()) {
            ds2484_onewire_write_byte(0xCC); // Skip ROM
            ds2484_onewire_write_byte(0x44); // Convert T
        }

        // 2. Асинхронное ожидание готовности (до 750мс, проверяя каждые 20мс)
        uint32_t start_time = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        bool conversion_done = false;

        while (((uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS) - start_time) < 750) {
            uint8_t bit = 0;
            if (ds2484_onewire_read_bit(&bit) && bit == 1) {
                conversion_done = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
        }

        if (!conversion_done) {
            ESP_LOGW(TAG, "Конвертация заняла много времени.");
        }

        // 3. Сканирование шины и чтение результатов
        uint8_t address[8];
        int count = 0;
        onewire_search_reset();

        while (onewire_search(address)) {
            if (address[0] == 0x28) { // DS18B20
                float temp = 0.0f;
                if (ds18b20_read_temperature(address, &temp)) {
                    ESP_LOGI(TAG, "Датчик [%d] (%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X): Температура: %.2f °C",
                             count + 1,
                             address[0], address[1], address[2], address[3],
                             address[4], address[5], address[6], address[7],
                             temp);
                } else {
                    ESP_LOGW(TAG, "Датчик [%d]: Ошибка чтения температуры.", count + 1);
                }
            }
            count++;
        }

        if (count == 0) {
            ESP_LOGW(TAG, "Датчики 1-Wire не найдены.");
        }

        ESP_LOGI(TAG, "-----------------------------------");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}