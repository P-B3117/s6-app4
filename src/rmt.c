#include "rmt.h"
#include "manchester_encoder.h"

#include <string.h>

// 1 tick at resolution_hz == BITRATE. Tune this to your desired half-bit
// period: e.g. if BITRATE = 1000000 (1 tick = 1us) and you want a 2us
// half-bit, set this to 2.

static rmt_channel_handle_t tx = NULL;
static rmt_channel_handle_t rx = NULL;
static rmt_symbol_word_t rx_buffer[BUFFER_SIZE];
static SemaphoreHandle_t rx_sem;
static rmt_encoder_handle_t encoder = NULL;

static const rmt_rx_channel_config_t rx_cfg = {
    .gpio_num = GPIO_NUM_19,
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = BITRATE,
    .mem_block_symbols = 64,
};

static const rmt_tx_channel_config_t tx_cfg = {
    .gpio_num = GPIO_NUM_23,
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = BITRATE,
    .mem_block_symbols = 64,
    .trans_queue_depth = 4,
};

static bool IRAM_ATTR rmt_rx_done_cb(rmt_channel_handle_t channel,
                                     const rmt_rx_done_event_data_t *edata,
                                     void *user_data)
{
    size_t n = edata->num_symbols;
    if (n > BUFFER_SIZE)
    {
        n = BUFFER_SIZE; // clamp: never write past rx_buffer
    }
    memcpy(rx_buffer, edata->received_symbols, n * sizeof(rmt_symbol_word_t));

    BaseType_t high_task_wakeup = pdFALSE;
    xSemaphoreGiveFromISR(rx_sem, &high_task_wakeup);
    return high_task_wakeup == pdTRUE;
}

void send_byte(uint8_t data)
{
    rmt_transmit_config_t cfg = {
        .loop_count = 0,
    };
    rmt_transmit(tx, encoder, &data, sizeof(data), &cfg);
    rmt_tx_wait_all_done(tx, 0xffffffffUL);
}

void start_rx()
{
    rmt_receive_config_t receive_cfg = {
        .signal_range_min_ns = 100,
        .signal_range_max_ns = 1000000,
    };
    rmt_receive(rx, rx_buffer, sizeof(rx_buffer), &receive_cfg);
}

void setup_transmission()
{
    rmt_new_rx_channel(&rx_cfg, &rx);
    rmt_new_tx_channel(&tx_cfg, &tx);
    rmt_enable(tx);
    rmt_enable(rx);

    rx_sem = xSemaphoreCreateBinary();

    rmt_rx_event_callbacks_t event_callback = {
        .on_recv_done = rmt_rx_done_cb,
    };
    rmt_rx_register_event_callbacks(rx, &event_callback, NULL);

    rmt_new_manchester_encoder(&encoder);

    start_rx();
}

SemaphoreHandle_t get_rx_semaphore()
{
    return rx_sem;
}

size_t get_rx_symbols(rmt_symbol_word_t *out, size_t max_symbols)
{
    size_t n = max_symbols < BUFFER_SIZE ? max_symbols : BUFFER_SIZE;
    memcpy(out, rx_buffer, n * sizeof(rmt_symbol_word_t));
    return n;
}
