#include <stdio.h>
#include "bluetooth_keyboard_sender.h"
#include "physical_keyboard.h"

void app_main(void)
{    
    sender_init();
    keyboard_init();

    QueueHandle_t keyboardInterfaceQueue = keyboardIf_createQueue(10);
    sender_setInboundQueue(keyboardInterfaceQueue);
    keyboard_setOutboundQueue(keyboardInterfaceQueue);

    sender_start();
    keyboard_start();
}
