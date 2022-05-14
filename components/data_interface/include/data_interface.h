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

typedef struct {
    uint16_t value;
    dataUnit_t unit;
} dataQueueContent_t;

QueueHandle_t dataIf_createQueue(uint8_t size);