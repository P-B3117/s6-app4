#pragma once

#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "freertos/semphr.h"


#define BUFFER_SIZE 16

#define BITRATE 10000000 // 100 MHz (10 ns ticks)
#define RMT_RES_HZ 80000000
#define RMT_HALF_BIT_TICKS (RMT_RES_HZ / (BITRATE * 2))

void setup_transmission();
void send_byte(uint8_t data);
SemaphoreHandle_t get_rx_semaphore();
size_t get_rx_symbols(rmt_symbol_word_t *out, size_t max_symbols);
