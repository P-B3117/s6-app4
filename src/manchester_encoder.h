#pragma once

#include "driver/rmt_encoder.h"

esp_err_t rmt_new_manchester_encoder(rmt_encoder_handle_t *ret_encoder);
