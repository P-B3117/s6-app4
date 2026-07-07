#include "manchester_encoder.h"
#include "rmt.h"
#include <stdint.h>
#include <stdio.h>

typedef struct {
  rmt_encoder_t base;
  rmt_encoder_handle_t copy_encoder;
  rmt_symbol_word_t manchester_table[256][8];
  size_t current_byte;
  uint8_t state;
} rmt_manchester_encoder_t;

// Fills one byte's worth of Manchester symbols
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

// Main encode function - now properly handles multi-byte buffers + partial
// encodes
static size_t rmt_encode_manchester(rmt_encoder_t *encoder,
                                     rmt_channel_handle_t channel,
                                     const void *primary_data, size_t data_size,
                                     rmt_encode_state_t *ret_state)
{
    rmt_manchester_encoder_t *manchester_encoder =
        __containerof(encoder, rmt_manchester_encoder_t, base);
    rmt_encode_state_t session_state = RMT_ENCODING_RESET;
    rmt_encode_state_t state = RMT_ENCODING_RESET;
    size_t encoded_symbols = 0;
    const uint8_t *data = (const uint8_t *)primary_data;
    rmt_encoder_handle_t copy_encoder = manchester_encoder->copy_encoder;

    switch (manchester_encoder->state) {
    case 0: // encode bytes one at a time, resuming on the current byte after MEM_FULL
        while (manchester_encoder->current_byte < data_size) {
            rmt_symbol_word_t *symbols =
                manchester_encoder->manchester_table[data[manchester_encoder->current_byte]];

            encoded_symbols += copy_encoder->encode(copy_encoder, channel, symbols,
                                                     8 * sizeof(rmt_symbol_word_t), &session_state);

            if (session_state & RMT_ENCODING_COMPLETE) {
                manchester_encoder->current_byte++; // only advance once this byte's symbols are fully flushed
            }
            if (session_state & RMT_ENCODING_MEM_FULL) {
                state |= RMT_ENCODING_MEM_FULL;
                goto out; // yield; current_byte stays put so we resume on the same byte next call
            }
        }
        manchester_encoder->state = 1; // all bytes done, move to finalize
    // fall-through
    case 1: // whole transaction complete
        manchester_encoder->current_byte = 0;   // reset byte cursor for next transaction
        manchester_encoder->state = RMT_ENCODING_RESET; // back to initial state
        state |= RMT_ENCODING_COMPLETE;
        break;
    }
out:
    *ret_state = state;
    return encoded_symbols;
}

static esp_err_t rmt_manchester_encoder_reset(rmt_encoder_t *encoder) {
  rmt_manchester_encoder_t *manchester_encoder =
      __containerof(encoder, rmt_manchester_encoder_t, base);
  manchester_encoder->current_byte = 0;

  return rmt_encoder_reset(manchester_encoder->copy_encoder);
}

static esp_err_t rmt_manchester_encoder_del(rmt_encoder_t *encoder) {
  rmt_manchester_encoder_t *manchester_encoder =
      __containerof(encoder, rmt_manchester_encoder_t, base);
  if (manchester_encoder->copy_encoder) {
    rmt_del_encoder(manchester_encoder->copy_encoder);
  }
  free(manchester_encoder);
  return ESP_OK;
}

esp_err_t rmt_new_manchester_encoder(rmt_encoder_handle_t *ret_encoder) {
  rmt_manchester_encoder_t *manchester_encoder =
      calloc(1, sizeof(rmt_manchester_encoder_t));
  if (!manchester_encoder) {
    return ESP_ERR_NO_MEM;
  }

  manchester_encoder->base.encode = rmt_encode_manchester;
  manchester_encoder->base.reset = rmt_manchester_encoder_reset;
  manchester_encoder->base.del = rmt_manchester_encoder_del;

  // Build lookup table
  for (int b = 0; b < 256; b++) {
    build_manchester_symbols((uint8_t)b,
                             manchester_encoder->manchester_table[b]);
  }

  rmt_copy_encoder_config_t copy_config = {};
  esp_err_t err =
      rmt_new_copy_encoder(&copy_config, &manchester_encoder->copy_encoder);
  if (err != ESP_OK) {
    free(manchester_encoder);
    return err;
  }

  *ret_encoder = &manchester_encoder->base;
  return ESP_OK;
}
