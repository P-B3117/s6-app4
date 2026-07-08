#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "hal/rmt_types.h"

#include "crc.h"
#include "examples.h"
#include "manchester_decoder.h"
#include "message.h"
#include "rmt.h"

#include "xtensa/hal.h"  // timestamp include

typedef struct {
  char *configString;
  int configInt;
} taskConfig;

void initTaskConfig(taskConfig *conf, char *str, int num) {
  conf->configString = str;
  conf->configInt = num;
}

#define QUEUE_SIZE 256

// task queues
QueueHandle_t send_queue;
QueueHandle_t recv_queue;

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
      decode_manchester(&local_symbols[decoder_pos], &mm, 0, BUFFER_SIZE);

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
        // printf("\n\nRX: received full message\n");
        // print_man_message(&mm);

        man_to_trame(&mm, &tr);

        // printf("\n");
        // print_trame(&tr);

        while (xQueueSend(recv_queue, &tr, portMAX_DELAY) == pdFALSE) {
          vTaskDelay(0);
        }

        // printf("\nFlushing and going again\n");
        init_trame(&tr);
        init_man_message(&mm);
      }
    }

    vTaskDelay(0);
    start_rx(); // re-arm for next reception ??????
  }
  vTaskDelay(0);
}

void tx_task(void *arg) {
  uint8_t datastr[89] = {0};
  trame tr;
  init_trame(&tr);
  uint64_t timestamp_start = 0;
  uint64_t timestamp_stop = 0;
  uint8_t message_count = 0;
  uint16_t byte_sent = 0;
  uint64_t messages_timestamp[TRAME_EXAMPLE_SIZE * 2] = {0};

  vTaskDelay(pdMS_TO_TICKS(3000));

  printf("\nTX: now looping\n");

  for (;;) {

    int queue_ret = xQueueReceive(send_queue, &tr, portMAX_DELAY);
    if (queue_ret == pdFALSE) {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    } else {

      messages_timestamp[message_count * 2 - 2] = xthal_get_ccount();

      if (message_count == 0) {
        timestamp_start = xthal_get_ccount();
      }

      trame_to_buffer(&tr, datastr);
      uint16_t size = trame_size(&tr);
      byte_sent += size;

      // printf("sending message: ");
      // for (int j = 0; j < size; j++) {
      //   printf("0x%02X ", datastr[j]);
      // }
      // printf("\n\n");

      send_msg(datastr, trame_size(&tr));
      messages_timestamp[message_count * 2 - 1] = xthal_get_ccount();
      message_count++;

      if (message_count == TRAME_EXAMPLE_SIZE) {
        timestamp_stop = xthal_get_ccount();

        uint32_t cpu_freq_hz = 240000000;

        printf("Time between messages: %" PRId64 " clock cycles (%.2f ms)\n", timestamp_stop - timestamp_start, ((timestamp_stop - timestamp_start) / (double)cpu_freq_hz) * 1000);

        printf("Sent %u bytes\n", byte_sent);

        printf("%.2f kbits/s\n", (byte_sent * 8) / ((timestamp_stop - timestamp_start) / (double)cpu_freq_hz) / 1000);

        printf("Message timestamps: ");
        for (int i = 1; i < message_count * 2; i += 2) {
          printf("%" PRId64 " ", messages_timestamp[i] - messages_timestamp[i - 1]);
        }
        printf("\n");
      }
    }

    // vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void app_main() {
  send_queue = xQueueCreate(QUEUE_SIZE, sizeof(trame));
  recv_queue = xQueueCreate(QUEUE_SIZE, sizeof(trame));

  setup_transmission();
  vTaskDelay(pdMS_TO_TICKS(1000));

  printf("Starting task rx...\n");
  xTaskCreatePinnedToCore(rx_task, "receive", 8192, NULL,
                          configMAX_PRIORITIES - 1, NULL, 1);

  printf("Starting task tx...\n");
  xTaskCreatePinnedToCore(tx_task, "send", 2048, NULL, configMAX_PRIORITIES - 2,
                          NULL, 1);

  printf("Starting task example...\n");
  xTaskCreatePinnedToCore(example_task, "example", 24000, NULL, configMAX_PRIORITIES - 3,
                          NULL, 1);
}
