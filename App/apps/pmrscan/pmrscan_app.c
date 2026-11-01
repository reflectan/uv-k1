#include "../app_api.h"
#include "pmrscan_assets.h" 

typedef enum { SCAN_IDLE, SCAN_ACTIVE } scan_state_t;

// Forward-declare helpers to physically force app_main to the top of the compiled binary
static void my_memset(void *dest, uint8_t val, uint32_t len);
static void format_dbm(int16_t dbm, char *buf, uint32_t buf_len);
static void format_channel(uint8_t channel, char *buf, uint32_t buf_len);
static void set_frequency(const app_api_t *api, uint32_t freq_10hz);
static void calibrate_noise_floor(const app_api_t *api, int16_t *noise_floor_out);

__attribute__((section(".text.entry"))) void app_main(const app_api_t *api) {
    // 1. Immediate Visual Feedback (Proves the app successfully launched)
    my_memset(api->fb, 0, 7 * 128);
    api->print_normal("BOOTING...", 0, 127, 2);
    api->blit_full();
    api->delay_ms(200);

    // 2. Hardware Band Constraint
    uint32_t vfo_10hz = api->rx_freq();
    if (vfo_10hz < 40000000 || vfo_10hz > 48000000) { // 400 MHz to 480 MHz check
        my_memset(api->fb, 0, 7 * 128);
        api->print_normal("ERROR: BAND MISMATCH", 0, 127, 0);
        api->print_normal("SWITCH VFO TO UHF", 0, 127, 2);
        api->print_normal("PRESS EXIT", 0, 127, 6);
        api->blit_full();
        while (api->get_key() != APP_KEY_EXIT) { api->delay_ms(50); }
        return; 
    }

    scan_state_t state = SCAN_IDLE;
    uint8_t current_channel = 0;
    uint8_t key, prev_key = 0;
    int16_t signal_dbm = -130;
    int16_t noise_floor = -105;

    // Save original registers
    uint16_t orig_reg38 = api->bk_read(0x38);
    uint16_t orig_reg39 = api->bk_read(0x39);
    uint16_t orig_reg30 = api->bk_read(0x30);

    char title_buf[16];
    char status_buf[16];
    char ch_buf[16];
    char rssi_buf[16];
    char nf_buf[16];

    my_memset(title_buf, 0, 16);
    my_memset(status_buf, 0, 16);
    my_memset(ch_buf, 0, 16);
    my_memset(rssi_buf, 0, 16);
    my_memset(nf_buf, 0, 16);

    api->set_af(APP_AF_MUTE);
    api->audio_path(false);

    api->asset_read(T_TITLE, title_buf, sizeof(title_buf));

    // Show calibration status
    my_memset(api->fb, 0, 7 * 128);
    api->print_normal("CALIBRATING...", 0, 127, 2);
    api->blit_full();

    calibrate_noise_floor(api, &noise_floor);
    format_dbm(noise_floor, nf_buf, sizeof(nf_buf));

    while (1) {
        key = api->get_key();

        // Prevent virtual screensaver keys from glitching the logic
        if (key == APP_KEY_SAVER || key == APP_KEY_WAKE) {
            prev_key = key;
            continue;
        }

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
            uint32_t freq_10hz = 0;
            api->asset_read(PMR_FREQS + (current_channel * sizeof(uint32_t)), &freq_10hz, sizeof(freq_10hz));
            set_frequency(api, freq_10hz);
            api->delay_ms(3);
            signal_dbm = api->rssi_dbm();

            if (signal_dbm > noise_floor + 3) {
                state = SCAN_IDLE;
                api->set_af(APP_AF_FM);
                api->audio_path(true);
            } else {
                current_channel++;
                if (current_channel >= PMR_CHANNELS) {
                    current_channel = 0;
                }
            }
        } else if (state == SCAN_IDLE) {
            signal_dbm = api->rssi_dbm();
        }

        my_memset(api->fb, 0, 7 * 128);

        format_channel(current_channel, ch_buf, sizeof(ch_buf));
        api->asset_read(state == SCAN_ACTIVE ? T_SCANNING : T_PAUSED, status_buf, sizeof(status_buf));
        format_dbm(signal_dbm, rssi_buf, sizeof(rssi_buf));

        api->print_normal(title_buf, 0, 127, 0);
        api->print_normal(ch_buf, 0, 127, 2);
        api->print_normal(status_buf, 0, 127, 4);
        api->print_normal(rssi_buf, 0, 60, 6);
        api->print_normal(nf_buf, 64, 127, 6);

        api->blit_full();
    }

    api->set_af(APP_AF_MUTE);
    api->audio_path(false);
    api->bk_write(0x38, orig_reg38);
    api->bk_write(0x39, orig_reg39);
    api->bk_write(0x30, 0x0000);
    api->bk_write(0x30, orig_reg30);
}

// Helper Implementations
static void my_memset(void *dest, uint8_t val, uint32_t len) {
    uint8_t *ptr = (uint8_t *)dest;
    while (len--) {
        *ptr++ = val;
    }
}

static void format_dbm(int16_t dbm, char *buf, uint32_t buf_len) {
    if (buf == NULL || buf_len < 8) return;
    int16_t val = (dbm < 0) ? -dbm : dbm; 
    uint8_t hundreds = 0, tens = 0;
    while (val >= 100) { val -= 100; hundreds++; }
    while (val >= 10) { val -= 10; tens++; }
    uint8_t i = 0;
    if (dbm < 0) buf[i++] = '-';
    if (hundreds > 0) buf[i++] = '0' + hundreds;
    buf[i++] = '0' + tens;
    buf[i++] = '0' + val;
    buf[i++] = ' '; buf[i++] = 'd'; buf[i++] = 'B'; buf[i++] = 'm';
    buf[i] = '\0';
}

static void format_channel(uint8_t channel, char *buf, uint32_t buf_len) {
    if (buf == NULL || buf_len < 8) return;
    uint8_t display_ch = channel + 1;
    buf[0] = 'C'; buf[1] = 'H'; buf[2] = ':'; buf[3] = ' ';
    if (display_ch > 9) {
        buf[4] = '1'; buf[5] = '0' + (display_ch - 10);
    } else {
        buf[4] = '0'; buf[5] = '0' + display_ch;
    }
    buf[6] = '\0';
}

static void set_frequency(const app_api_t *api, uint32_t freq_10hz) {
    api->bk_write(0x38, freq_10hz & 0xFFFF);
    api->bk_write(0x39, (freq_10hz >> 16) & 0xFFFF);
    api->bk_write(0x30, 0x0000);
    api->bk_write(0x30, 0xBFF1);
}

static void calibrate_noise_floor(const app_api_t *api, int16_t *noise_floor_out) {
    int16_t min_rssi = 0;
    int16_t noise_floor = -105;
    for (uint8_t ch = 0; ch < PMR_CHANNELS; ch++) {
        uint32_t calib_freq = 0;
        api->asset_read(PMR_FREQS + (ch * sizeof(uint32_t)), &calib_freq, sizeof(calib_freq));
        set_frequency(api, calib_freq);
        api->delay_ms(3);
        int16_t current_rssi = api->rssi_dbm();
        if (current_rssi < min_rssi) min_rssi = current_rssi;
    }
    noise_floor = min_rssi + 6;
    if (noise_floor > -95) noise_floor = -95;
    *noise_floor_out = noise_floor;
}
