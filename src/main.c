#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hal/rmt_types.h"

#include "message.h"
#include "manchester_decoder.h"
#include "crc.h"
#include "rmt.h"

// static const uint8_t preambule = 0x55;
// static const uint8_t start = 0x7E;
// static const uint8_t end = 0x7E;
// static const uint8_t enteteLength = 4;
// static const uint8_t crc = 2;

typedef struct {
  char *configString;
  int configInt;
} taskConfig;

void example_trame_first_message(trame *t) {
  t->fields.entete[0] = 0x01;
  t->fields.entete[1] = 0x01;
  t->fields.entete[2] = 0x00;
  t->fields.entete[3] = 0x01;

  t->chargeLength = 0;

  t->crc[0] = 0xD5;
  t->crc[1] = 0x65;
}

void example_trame_second_message(trame *t) {
  t->fields.entete[0] = 0x02;
  t->fields.entete[1] = 0x02;
  t->fields.entete[2] = 0x03;
  t->fields.entete[3] = 0x00;

  t->chargeLength = 3;

  t->fields.chargeUtile[0] = 0x07;
  t->fields.chargeUtile[1] = 0x04;
  t->fields.chargeUtile[2] = 0x09;

  t->crc[0] = 0x2C;
  t->crc[1] = 0xC2;
}

void example_trame_third_message(trame *t) {
  t->fields.entete[0] = 0x03;
  t->fields.entete[1] = 0x03;
  t->fields.entete[2] = 0x00;
  t->fields.entete[3] = 0x00;

  t->chargeLength = 0;

  t->crc[0] = 0x46;
  t->crc[1] = 0x4C;
}

void initTaskConfig(taskConfig *conf, char *str, int num) {
  conf->configString = str;
  conf->configInt = num;
}

// crc 2 byte
void calc_crc(trame *input) {
  uint16_t crc_c = crc16(input->flat_buffer, input->chargeLength + 4);
  input->crc[0] = crc_c & 0xFF;
  input->crc[1] = (crc_c >> 8) & 0xFF;
}

void rx_task(void *arg) {
  SemaphoreHandle_t sem = get_rx_semaphore();
  rmt_symbol_word_t local_symbols[BUFFER_SIZE];
  trame tr;
  man_message mm;
  init_trame(&tr);
  init_man_message(&mm);
  uint16_t decoder_pos = 0;

  printf("RX: now looping\n");

  for (;;) {
    if (xSemaphoreTake(sem, portMAX_DELAY) == pdTRUE) {
      get_rx_symbols(local_symbols, BUFFER_SIZE);
      // printf("RX: existing");

      // while (decoder_pos < BUFFER_SIZE) {
      // printf("%d", decoder_pos);
      decode_manchester(&local_symbols[decoder_pos],&mm, 0, BUFFER_SIZE);
      // print_man_message(&mm);
      // for (int i = 0; i < pos; i++) {
      //   printf("Symbol %d: level0=%d duration0=%d, level1=%d duration1=%d\n",
      //   i,
      //          local_symbols[i].level0, local_symbols[i].duration0,
      //          local_symbols[i].level1, local_symbols[i].duration1);
      // }

      // print_man_message(&mm);
      mm.currentIndex = 0;
      mm.current[0] = 0;
      mm.current[1] = 0;
      if (mm.bitIndex != 0) {
        mm.bitIndex = 0;
        mm.message[mm.messageIndex] = 0x00;
      }

      if (mm.finished) {
        printf("\n\nRX: received full message\n");
        print_man_message(&mm);
        man_to_trame(&mm, &tr);
        printf("\n");
        print_trame(&tr);

        printf("\nFlushing and going again\n");
        init_trame(&tr);
        init_man_message(&mm);
      }
    }

    start_rx(); // re-arm for next reception ??????
  }
  vTaskDelay(0);
}

void tx_task(void *arg) {
  uint8_t datastr[1024] = {0};
  uint8_t i = 0;
  trame tr;
  init_trame(&tr);

  printf("\nTX: now looping\n");

  for (;;) {
    memset(&datastr, 0, 1024);
    i = i % 3;
    if (i == 0) {
      example_trame_first_message(&tr);
      trame_to_buffer(&tr, datastr);
    } else if (i == 1) {
      example_trame_second_message(&tr);
      trame_to_buffer(&tr, datastr);
    } else if (i == 2) {
      example_trame_third_message(&tr);
      trame_to_buffer(&tr, datastr);
    }
    printf("sending message: %hhu\n", i);
    uint16_t size = trame_size(&tr);
    for (int j = 0; j < size; j++) {
      printf("0x%02X ", datastr[j]);
    }
    printf("\n");
    // vTaskDelay(pdMS_TO_TICKS(500));
    send_msg(datastr, size);
    vTaskDelay(pdMS_TO_TICKS(100));
    i++;
  }
}

void app_main() {
  setup_transmission();
  vTaskDelay(pdMS_TO_TICKS(1000));

  printf("Starting tasks rx...\n");
  xTaskCreatePinnedToCore(rx_task, "receive", 8192, NULL,
                          configMAX_PRIORITIES - 1, NULL, 1);
  printf("Starting tasks tx...\n");
  xTaskCreatePinnedToCore(tx_task, "send", 2048, NULL, configMAX_PRIORITIES - 2,
                          NULL, 1);
}
