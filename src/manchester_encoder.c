#include "manchester_encoder.h"

#define RMT_HALF_BIT_TICKS 1  // set to whatever your resolution_hz gives you for a half-bit period

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_handle_t copy_encoder;
    rmt_symbol_word_t manchester_table[256][8];
} rmt_manchester_encoder_t;

// Fills one byte's worth of Manchester symbols (your function, adapted to write into the LUT)
static void build_manchester_symbols(uint8_t byte, rmt_symbol_word_t *out) {
    for (int i = 0; i < 8; i++) {
        bool bit = byte & (1 << (7 - i));
        out[i].duration0 = RMT_HALF_BIT_TICKS;
        out[i].duration1 = RMT_HALF_BIT_TICKS;
        if (bit) {
            out[i].level0 = 0;
            out[i].level1 = 1;
        } else {
            out[i].level0 = 1;
            out[i].level1 = 0;
        }
    }
}

// This is the function the driver actually calls
static size_t rmt_encode_manchester(rmt_encoder_t *encoder,
                                     rmt_channel_handle_t channel,
                                     const void *primary_data,
                                     size_t data_size,
                                     rmt_encode_state_t *ret_state)
{
    rmt_manchester_encoder_t *manchester_encoder = __containerof(encoder, rmt_manchester_encoder_t, base);
    rmt_encoder_handle_t copy_encoder = manchester_encoder->copy_encoder;
    const uint8_t *data = (const uint8_t *)primary_data;

    size_t encoded_symbols = 0;
    rmt_encode_state_t state = (rmt_encode_state_t)0;

    // NOTE: for multi-byte buffers you need to track progress across calls
    // (see "handling partial encodes" below) — simplified here for one byte at a time
    for (size_t i = 0; i < data_size; i++) {
        rmt_symbol_word_t *symbols = manchester_encoder->manchester_table[data[i]];
        encoded_symbols += copy_encoder->encode(copy_encoder, channel, symbols,
                                                  8 * sizeof(rmt_symbol_word_t), &state);
        if (state & RMT_ENCODING_MEM_FULL) {
            *ret_state = RMT_ENCODING_MEM_FULL;
            return encoded_symbols; // driver will call us again to continue
        }
    }

    *ret_state = RMT_ENCODING_COMPLETE;
    return encoded_symbols;
}

static esp_err_t rmt_manchester_encoder_reset(rmt_encoder_t *encoder) {
    rmt_manchester_encoder_t *manchester_encoder = __containerof(encoder, rmt_manchester_encoder_t, base);
    return rmt_encoder_reset(manchester_encoder->copy_encoder);
}

static esp_err_t rmt_manchester_encoder_del(rmt_encoder_t *encoder) {
    rmt_manchester_encoder_t *manchester_encoder = __containerof(encoder, rmt_manchester_encoder_t, base);
    rmt_del_encoder(manchester_encoder->copy_encoder);
    free(manchester_encoder);
    return ESP_OK;
}

esp_err_t rmt_new_manchester_encoder(rmt_encoder_handle_t *ret_encoder) {
    rmt_manchester_encoder_t *manchester_encoder = calloc(1, sizeof(rmt_manchester_encoder_t));
    if (!manchester_encoder) return ESP_ERR_NO_MEM;

    manchester_encoder->base.encode = rmt_encode_manchester;
    manchester_encoder->base.reset  = rmt_manchester_encoder_reset;
    manchester_encoder->base.del    = rmt_manchester_encoder_del;

    // build the LUT once, up front
    for (int b = 0; b < 256; b++) {
        build_manchester_symbols((uint8_t)b, manchester_encoder->manchester_table[b]);
    }

    rmt_copy_encoder_config_t copy_config = {};
    esp_err_t err = rmt_new_copy_encoder(&copy_config, &manchester_encoder->copy_encoder);
    if (err != ESP_OK) {
        free(manchester_encoder);
        return err;
    }

    *ret_encoder = &manchester_encoder->base;
    return ESP_OK;
}