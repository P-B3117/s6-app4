#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"

#define BUFFER_SIZE 16

#define RESOLUTION 10000000
#define RMT_SPEED_HZ 80000000

#define RMT_FULL_BIT_TICKS RESOLUTION / RMT_SPEED_HZ
#define RMT_HALF_BIT_TICKS RMT_FULL_BIT_TICKS / 2
#define RMT_FULL_BIT_THRESHOLD (RMT_HALF_BIT_TICKS + (RMT_HALF_BIT_TICKS / 2))

void setup_transmission();
void send_byte(uint8_t data);
void send_msg(uint8_t *data, size_t len);
void start_rx();
SemaphoreHandle_t get_rx_semaphore();
size_t get_rx_symbols(rmt_symbol_word_t *out, size_t max_symbols);
