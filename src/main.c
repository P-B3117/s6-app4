#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"

#include "driver/rmt_tx.h"
#include "driver/rmt_rx.h"

typedef struct {
    char* configString;
    int configInt;
} taskConfig;

const uint8_t preambule = 0x55;
const uint8_t start = 0x7E;
const uint8_t end = 0x7E;
const uint8_t enteteLength = 4;
const uint8_t crc = 2;

#define BITRATE 10000000 // 100 MHz (10 ns ticks)
#define RMT_RES_HZ 80000000

#define RMT_HALF_BIT_TICKS (RMT_RES_HZ / (BITRATE * 2))

const rmt_rx_channel_config_t rx_cfg = {
        .gpio_num = GPIO_NUM_19,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = BITRATE,   
        .mem_block_symbols = 64,
    };

const rmt_tx_channel_config_t tx_cfg = {
        .gpio_num = GPIO_NUM_23,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = BITRATE,   
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

rmt_channel_handle_t tx;
rmt_channel_handle_t rx;

static rmt_symbol_word_t rx_buffer[8];

enum com_typ {
    debut = 0x01,
    data = 0x02,
    fin = 0x03,
    nack = 0x04,
};

enum trame_pos {
    preambule_pos = 0,
    start_pos = 1,
    entete_pos = 2,
    charge_pos = 6,
}; // crc and end pos depends on charge longueur

typedef struct {
    uint8_t* entete; // type de comm, num sequence, longueur charge, volume dynamique
    uint8_t* chargeUtile;
    uint8_t chargeLength;
    uint8_t* crc;
} trame;

uint8_t crc_pos(trame* t) {
    return (charge_pos + t->chargeLength);
}

uint8_t end_pos(trame* t) {
    return (charge_pos + t->chargeLength + 2);
}

void start_rx(void)
{
    rmt_receive_config_t rx_cfg = {
        .signal_range_min_ns = 100,
        .signal_range_max_ns = 1000000,
    };

    rmt_receive(rx, rx_buffer, sizeof(rx_buffer), &rx_cfg);
}

static void encode_manchester(uint8_t byte, rmt_symbol_word_t *out)
{
    for (int i = 0; i < 8; i++) {

        bool bit = byte & (1 << (7 - i));

        out[i].duration0 = RMT_HALF_BIT_TICKS;
        out[i].duration1 = RMT_HALF_BIT_TICKS;

        if (bit) {
            // 1 = LOW then HIGH
            out[i].level0 = 0;
            out[i].level1 = 1;
        } else {
            // 0 = HIGH then LOW
            out[i].level0 = 1;
            out[i].level1 = 0;
        }
    }
}

static uint8_t decode_manchester(rmt_symbol_word_t *sym)
{
    uint8_t out = 0;

    for (int i = 0; i < 8; i++) {
        rmt_symbol_word_t s = sym[i];
        bool bit;

        // LOW→HIGH = 1
        if (s.level0 == 0 && s.level1 == 1) {
            bit = 1;
        }
        // HIGH→LOW = 0
        else {
            bit = 0;
        }

        out = (out << 1) | bit;
    }

    return out;
}

rmt_encoder_handle_t encoder = NULL;
void send_byte(uint8_t data)
{
    rmt_symbol_word_t symbols[8];
    encode_manchester(data, symbols);

    rmt_transmit_config_t cfg = {
        .loop_count = 0,
    };

    rmt_transmit(
        tx,
        encoder,   // no encoder (raw symbols)
        symbols,
        sizeof(symbols),
        &cfg
    );

    rmt_tx_wait_all_done(tx, portMAX_DELAY);
}

void initTaskConfig(taskConfig* conf, char* str, int num) {
    conf->configString = str;
    conf->configInt = num;
}

void task(void *pvParameters) {
    taskConfig* conf = (taskConfig*)pvParameters;

    while(1) {
        printf("tasking");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// loop send toutes les trames dans une queue.
void task_sender(void *pvParameters){
    ////RMT
}

// loop receive toutes les trames dans une queue
void task_receiver(void *pvParameters){
    ////RMT
}

// input is charge utile, crc is 2 byte array (to ensure easy concatenation)
void calc_crc(trame* input){

}

// input is message (whole ass tableau annexe) to send to other esp, manchester the binary of the message
void send(trame* input) {

}

// decodes the buffer from manchester to standard bytes
void receive(uint8_t* buffer, uint8_t start, uint8_t end) {

}

void bidonTask(void *pvParameters) {
    while(1) {
        // 1. SEND PREAMBLE (SYNC)
        send_byte(0xAA);
        send_byte(0xAA);
        send_byte(0x55);
        
        rmt_tx_wait_all_done(tx, portMAX_DELAY);

        int valid_symbols = 0;

        for (int i = 0; i < 64; i++) {
            if (rx_buffer[i].duration0 == 0 &&
                rx_buffer[i].duration1 == 0) {
                break;
            }

            valid_symbols++;
        }

        printf("valid symbols: %d\n", valid_symbols);

        if (valid_symbols >= 8) {
            uint8_t b = decode_manchester(rx_buffer);
            printf("decoded: 0x%02X\n", b);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void app_main() {
    rmt_new_rx_channel(&rx_cfg, &rx);
    rmt_new_tx_channel(&tx_cfg, &tx);
    rmt_enable(tx);
    rmt_enable(rx);

    rmt_copy_encoder_config_t copy_cfg = {};
    rmt_new_copy_encoder(&copy_cfg, &encoder);
    start_rx();

    xTaskCreatePinnedToCore(
        bidonTask, 
        "bidonTask", 
        2048, 
        NULL, 
        1, 
        NULL, 
        1
    );

    // taskConfig* conf = &(taskConfig){};
    // initTaskConfig(conf, "lol", 2);

    // xTaskCreate(task, "task 1", 2048, conf, 1, NULL);
}
