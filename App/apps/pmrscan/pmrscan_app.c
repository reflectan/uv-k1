#include "../app_api.h"

__attribute__((section(".text.entry"))) void app_main(const app_api_t *api) {
    api->display_clear();
    api->print_normal("LINKER FIXED", 0, 127, 2);
    api->print_normal("APP IS ALIVE", 0, 127, 4);
    api->blit_full();
    
    while (api->get_key() != APP_KEY_EXIT) {
        api->delay_ms(50);
    }
}
