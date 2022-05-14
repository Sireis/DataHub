#include "data_interface.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

QueueHandle_t dataIf_createQueue(uint8_t size)
{
    QueueHandle_t queue = xQueueCreate(size, sizeof(dataQueueContent_t));
    return queue;
}