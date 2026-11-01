#include "../app_api.h"

// Absolutely no other helper functions in this file.
void app_main(const app_api_t *api) {
    // Clear the screen
    api->display_clear();
    
    // Print a static test message
    api->print_normal("FIRMWARE 6.3.0 OK", 0, 127, 2);
    api->print_normal("UI IS WORKING", 0, 127, 4);
    
    // Push frame buffer to LCD
    api->blit_full();
    
    // Block until EXIT is pressed
    while (api->get_key() != APP_KEY_EXIT) {
        api->delay_ms(50);
    }
}
