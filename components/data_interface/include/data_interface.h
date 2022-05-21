#pragma once
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

typedef enum {
    DATA_NO_DIMENSION,
    DATA_METER,
    DATA_CELSIUS,
    DATA_PERCENTAGE,
    DATA_RAW,
} dataUnit_t;

typedef enum {
    DATA_SOURCE_0,
    DATA_SOURCE_1,
    DATA_SOURCE_2,
    DATA_SOURCE_3,
    DATA_SOURCE_4,
    DATA_SOURCE_COUNT,
} dataSource_t;

typedef int32_t sample_t;

typedef struct {
    sample_t value;
    dataUnit_t unit;
    dataSource_t source;
} dataQueueContent_t;

QueueHandle_t dataIf_createQueue(uint8_t size);