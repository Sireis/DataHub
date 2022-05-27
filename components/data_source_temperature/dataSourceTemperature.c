#include "dataSourceTemperature.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include <esp_log.h>
#include <esp_system.h>
#include "esp_timer.h"

#include "data_interface.h"

#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"

#define PERIOD_TIME_MS  (60*1000)

static const char *TAG = "dataSourceTemperature";
static TaskHandle_t _taskHandle;
static QueueHandle_t _outboundQueue = NULL;

#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
#define MULTISHOTS   16          //Multisampling

#define SUPPORTING_POINT_THETA_1    8000
#define SUPPORTING_POINT_THETA_2    22500

#define SUPPORTING_POINT_V_1        1002
#define SUPPORTING_POINT_V_2        631

static esp_adc_cal_characteristics_t *_adcCharacteristic;
static const adc_channel_t _activeAdcChannel = ADC_CHANNEL_0;
static const adc_bits_width_t _adcBitwidth = ADC_WIDTH_BIT_12;
static const adc_atten_t _adcAttenuation = ADC_ATTEN_DB_11;
static const adc_unit_t _adcUnit = ADC_UNIT_1;

static dataSource_t _sourceIndex = DATA_SOURCE_0;

static void task(void* parameters);
static void checkEfuse();
static void print_char_val_type(esp_adc_cal_value_t val_type);
static uint32_t sampleMilliVoltage();
static int32_t convertToTemperature(uint32_t millVoltage);

void dataSourceTemperature_init()
{
    checkEfuse();
    
    if (_adcUnit == ADC_UNIT_1) 
    {
        adc1_config_width(_adcBitwidth);
        adc1_config_channel_atten(_activeAdcChannel, _adcAttenuation);
    } 
    else 
    {
        adc2_config_channel_atten((adc2_channel_t)_activeAdcChannel, _adcAttenuation);
    }
    
    _adcCharacteristic = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(_adcUnit, _adcAttenuation, _adcBitwidth, DEFAULT_VREF, _adcCharacteristic);
    print_char_val_type(val_type);
}

void dataSourceTemperature_start()
{
    xTaskCreatePinnedToCore(task, 
                            "dataSourceTemperatureTask", 
                            2*1024, 
                            NULL, 
                            configMAX_PRIORITIES - 3, 
                            &_taskHandle, 
                            tskNO_AFFINITY
                            );
}

void dataSourceTemperature_setOutboundQueue(QueueHandle_t queue)
{
    _outboundQueue = queue;
}

void dataSourceTemperature_setSourceIndex(dataSource_t sourceIndex)
{
    _sourceIndex = sourceIndex;
}

static void task(void* parameters)
{    
    ESP_LOGI(TAG, "started");

    for(;;)
    {   
        int64_t timestampForNextShot = esp_timer_get_time() + PERIOD_TIME_MS*1000;
        if (_outboundQueue != NULL)
        {
            uint32_t milliVoltage = sampleMilliVoltage();
            dataQueueContent_t data = {
                .source = _sourceIndex,
                .unit = DATA_CELSIUS,
                .value = convertToTemperature(milliVoltage),
                //.value = milliVoltage,
            };
            ESP_LOGI(TAG, "Sampled temperature (%d)", data.value);
            xQueueSendToBack(_outboundQueue, &data, 500 / portTICK_RATE_MS);
        }

        int64_t currentTimestamp = esp_timer_get_time();
        int64_t timeLeftMs = (timestampForNextShot - currentTimestamp) / 1000;
        
        vTaskDelay(timeLeftMs / portTICK_RATE_MS);
    }

    vTaskDelete(_taskHandle);
}

static int32_t convertToTemperature(uint32_t milliVoltage)
{
    int32_t a = (SUPPORTING_POINT_THETA_1 - SUPPORTING_POINT_THETA_2) / (SUPPORTING_POINT_V_1 - SUPPORTING_POINT_V_2);
    int32_t b = SUPPORTING_POINT_THETA_1 - SUPPORTING_POINT_V_1*a;
    int32_t temperature = a*milliVoltage + b;
    return temperature;
}

static uint32_t sampleMilliVoltage()
{    
    uint32_t adcCumulatedReading = 0;
    //Multisampling
    for (int i = 0; i < MULTISHOTS; i++) 
    {
        if (_adcUnit == ADC_UNIT_1) 
        {
            adcCumulatedReading += adc1_get_raw((adc1_channel_t)_activeAdcChannel);
        } 
        else 
        {
            int raw;
            adc2_get_raw((adc2_channel_t)_activeAdcChannel, _adcBitwidth, &raw);
            adcCumulatedReading += raw;
        }
    }

    //Convert adc_reading to voltage in mV
    uint32_t voltage = esp_adc_cal_raw_to_voltage(adcCumulatedReading / MULTISHOTS, _adcCharacteristic);
    return voltage;
}

static void checkEfuse(void)
{
#if CONFIG_IDF_TARGET_ESP32
    //Check if TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }
    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
#elif CONFIG_IDF_TARGET_ESP32S2
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("Cannot retrieve eFuse Two Point calibration values. Default calibration values will be used.\n");
    }
#else
#error "This example is configured for ESP32/ESP32S2."
#endif
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}