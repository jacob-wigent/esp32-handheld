#include <stdio.h>
#include "display.h"

void app_main(void)
{
    // Initialize the display
    if (!display_init()) {
        printf("Failed to initialize display\n");
        return;
    }


    display_clear(0x001000); // Set the display to dim green
    display_show();
}