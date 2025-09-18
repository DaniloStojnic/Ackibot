#include <esp_log.h>
#include <esp_task_wdt.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>

#include <rak3172.h>


#include "sht31.h"

#include <sstream>


static RAK3172_t _Device = RAK3172_DEFAULT_CONFIG(CONFIG_RAK3172_UART_PORT, CONFIG_RAK3172_UART_RX, CONFIG_RAK3172_UART_TX, CONFIG_RAK3172_UART_BAUD);

static StackType_t _applicationStack[8192];

static StaticTask_t _applicationBuffer;

static TaskHandle_t _applicationHandle;

static const uint8_t Payload[]                  = {1, 2, 3};

static const char* TAG 							= "main";

static void applicationTask(void* p_Parameter)
{
    RAK3172_Error_t Error;
    RAK3172_Info_t Info;



    _Device.Info = &Info;

    Error = RAK3172_Init(_Device);
    if(Error != RAK3172_ERR_OK)
    {
        ESP_LOGE(TAG, "Cannot initialize RAK3172! Error: 0x%04lX", Error);
    }

    ESP_LOGI(TAG, "Firmware: %s", Info.Firmware.c_str());
    ESP_LOGI(TAG, "Serial number: %s", Info.Serial.c_str());
    ESP_LOGI(TAG, "Current mode: %u", _Device.Mode);

    Error = RAK3172_P2P_Init(_Device, 868000000, RAK_PSF_12, RAK_BW_125, RAK_CR_45, 200, 14);
    if(Error != RAK3172_ERR_OK)
    {
        ESP_LOGE(TAG, "Cannot initialize RAK3172 P2P! Error: 0x%04lX", Error);
    }


    vTaskDelay(10 / portTICK_PERIOD_MS); // Seg fault without this.
    float temp, humi;
    ESP_ERROR_CHECK(sht31_init());


    while(true){

        ESP_ERROR_CHECK(sht31_read_temp_humi(&temp, &humi));
    
        ESP_LOGI(TAG, "temp = %.2f, humi = %.2f\n", temp, humi);

        std::ostringstream payload;
        payload << "temp = " << temp << " humi = " << humi;


        Error = RAK3172_P2P_Transmit(
            _Device,
            reinterpret_cast<const uint8_t*>(payload.str().c_str()),
            payload.str().size()
        );
        if(Error != RAK3172_ERR_OK)
        {
            ESP_LOGE(TAG, "Cannot transmit LoRa message! Error: 0x%04lX", Error);
        }


        vTaskDelay(1000 / portTICK_PERIOD_MS); 
    
    }

/*


    Error = RAK3172_P2P_Listen(_Device, 60000);
    if(Error != RAK3172_ERR_OK)
    {
        ESP_LOGE(TAG, "Cannot start LoRa listening! Error: 0x%04lX", Error);
    }

    while(true)
    {
        if(RAK3172_P2P_isListening(_Device))
        {
            RAK3172_Rx_t Message;

            if(RAK3172_P2P_PopItem(_Device, &Message) == RAK3172_ERR_OK)
            {
                ESP_LOGI(TAG, " RSSI: %i", Message.RSSI);
                ESP_LOGI(TAG, " SNR: %i", Message.SNR);
                ESP_LOGI(TAG, " Payload: %s", Message.Payload.c_str());
            }
        }
        else
        {
            ESP_LOGI(TAG, "Not listening...");
        }

        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
*/
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting application...");

    _applicationHandle = xTaskCreateStatic(applicationTask, "applicationTask", sizeof(_applicationStack), NULL, 1, _applicationStack, &_applicationBuffer);
    if(_applicationHandle == NULL)
    {
        ESP_LOGE(TAG, "    Unable to create application task!");

        esp_restart();
    }
}