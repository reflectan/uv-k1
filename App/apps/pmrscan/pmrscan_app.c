#include "app_api.h"
#include "pmrscan_assets.h"

typedef enum {
    SCAN_IDLE,
    SCAN_ACTIVE
} scan_state_t;

// A basic memset to avoid linking libgcc/libc string.h
void my_memset(void *dest, uint8_t val, uint32_t len) {
    uint8_t *ptr = (uint8_t *)dest;
    while (len--) {
        *ptr++ = val;
    }
}

// Convert negative dBm to a string (e.g., "-105 dBm") using subtraction
void format_dbm(int16_t dbm, char *buf) {
    uint16_t val = (dbm < 0) ? -dbm : dbm; 
    uint8_t hundreds = 0;
    uint8_t tens = 0;

    while (val >= 100) { val -= 100; hundreds++; }
    while (val >= 10) { val -= 10; tens++; }

    uint8_t i = 0;
    buf[i++] = '-';
    if (hundreds > 0) buf[i++] = '0' + hundreds;
    buf[i++] = '0' + tens;
    buf[i++] = '0' + val;
    buf[i++] = ' '; buf[i++] = 'd'; buf[i++] = 'B'; buf[i++] = 'm';
    buf[i++] = '\0';
}

void app_main(const app_api_t *api) {
    scan_state_t state = SCAN_IDLE;
    uint8_t current_channel = 0;
    uint8_t key, prev_key = 0;
    
    int16_t signal_dbm = -130;
    int16_t noise_floor = -105;
    int16_t min_rssi = 0;

    // 1. Save original register states
    uint16_t orig_reg38 = api->bk_read(0x38);
    uint16_t orig_reg39 = api->bk_read(0x39);
    uint16_t orig_reg30 = api->bk_read(0x30);
    
    // Stack-allocated buffers to prevent global memory bloat
    char title_buf[16];
    char status_buf[16];
    char ch_buf[16] = "CH:   "; 
    char rssi_buf[16];
    char nf_buf[16];

    // Guarantee the speaker is muted at launch
    api->set_af(APP_AF_MUTE);
    api->audio_path(false);

    api->asset_read(ASSET_T_TITLE, title_buf, sizeof(title_buf));

    // 2. --- Robust Dynamic Noise Floor Calibration ---
    for (uint8_t ch = 0; ch < ASSET_PMR_CHANNELS; ch++) {
        uint32_t calib_freq;
        api->asset_read(ASSET_PMR_FREQS + (ch * sizeof(uint32_t)), &calib_freq, sizeof(calib_freq));
        
        api->bk_write(0x38, calib_freq & 0xFFFF);
        api->bk_write(0x39, (calib_freq >> 16) & 0xFFFF);
        api->bk_write(0x30, 0x0000); 
        api->bk_write(0x30, 0xBFF1);
        
        api->delay_ms(3); 
        int16_t current_rssi = api->rssi_dbm();
        if (current_rssi < min_rssi) {
            min_rssi = current_rssi;
        }
    }
    
    noise_floor = min_rssi + 6; 
    if (noise_floor > -95) noise_floor = -95;
    
    format_dbm(noise_floor, nf_buf);

    // 3. Main Event Loop
    while (1) {
        key = api->get_key();

        if (key != prev_key && key != 0) {
            if (key == APP_KEY_EXIT) {
                break;
            } else if (key == APP_KEY_MENU) {
                state = (state == SCAN_IDLE) ? SCAN_ACTIVE : SCAN_IDLE;
                if (state == SCAN_IDLE) {
                    api->set_af(APP_AF_MUTE);
                    api->audio_path(false);
                }
            }
        }
        prev_key = key;

        if (state == SCAN_ACTIVE) {
            uint32_t freq_10hz;
            api->asset_read(ASSET_PMR_FREQS + (current_channel * sizeof(uint32_t)), &freq_10hz, sizeof(freq_10hz));

            api->bk_write(0x38, freq_10hz & 0xFFFF);
            api->bk_write(0x39, (freq_10hz >> 16) & 0xFFFF);
            api->bk_write(0x30, 0x0000); 
            api->bk_write(0x30, 0xBFF1);

            api->delay_ms(3);
            signal_dbm = api->rssi_dbm();

            if (signal_dbm > noise_floor) {
                state = SCAN_IDLE; 
                api->set_af(APP_AF_FM);
                api->audio_path(true);
            } else {
                current_channel++;
                if (current_channel >= ASSET_PMR_CHANNELS) {
                    current_channel = 0;
                }
            }
        } else if (state == SCAN_IDLE) {
            signal_dbm = api->rssi_dbm();
        }

        // 4. Render UI
        my_memset(api->fb, 0, 7 * 128); 

        uint8_t display_ch = current_channel + 1;
        if (display_ch > 9) {
            ch_buf[4] = '1';
            ch_buf[5] = '0' + (display_ch - 10);
            ch_buf[6] = '\0';
        } else {
            ch_buf[4] = '0' + display_ch;
            ch_buf[5] = '\0';
            ch_buf[6] = '\0';
        }

        api->asset_read(state == SCAN_ACTIVE ? ASSET_T_SCANNING : ASSET_T_PAUSED, status_buf, sizeof(status_buf));
        format_dbm(signal_dbm, rssi_buf);

        // Parameters: (string, start_x, end_x, line_y)
        api->print_normal(title_buf, 0, 127, 0); 
        api->print_normal(ch_buf, 0, 127, 2);     
        api->print_normal(status_buf, 0, 127, 4); 
        
        api->print_normal(rssi_buf, 0, 60, 6);       // Live RSSI on bottom left
        api->print_normal(nf_buf, 64, 127, 6);       // Noise floor on bottom right

        api->blit_full();
    }

    // 5. Cleanup
    api->set_af(APP_AF_MUTE);
    api->audio_path(false);
    api->bk_write(0x38, orig_reg38);
    api->bk_write(0x39, orig_reg39);
    api->bk_write(0x30, 0x0000);
    api->bk_write(0x30, orig_reg30);
}
