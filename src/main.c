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

typedef struct
{
  char *configString;
  int configInt;
} taskConfig;

void initTaskConfig(taskConfig *conf, char *str, int num)
{
  conf->configString = str;
  conf->configInt = num;
}

#define QUEUE_SIZE 256

// task queues
QueueHandle_t send_queue;
QueueHandle_t recv_queue;

void rx_task(void *arg)
{
  SemaphoreHandle_t sem = get_rx_semaphore();
  rmt_symbol_word_t local_symbols[BUFFER_SIZE];
  trame tr;
  man_message mm;
  init_trame(&tr);
  init_man_message(&mm);
  uint16_t decoder_pos = 0;

  printf("RX: now looping\n");

  for (;;)
  {
    if (xSemaphoreTake(sem, portMAX_DELAY) == pdTRUE)
    {

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
      if (mm.bitIndex != 0)
      {
        mm.bitIndex = 0;
        mm.message[mm.messageIndex] = 0x00;
      }

      if (mm.finished)
      {
        trame tramemsg;
        man_to_trame(&mm, &tramemsg);

        uint16_t receivedcrc = (tramemsg.crc[1] << 8) | tramemsg.crc[0];
        uint16_t calculatedcrc = crc16(tramemsg.flat_buffer, tramemsg.chargeLength + 4);
        
        if (receivedcrc == calculatedcrc)
        {
          printf("\n\nRX: received full message with valid CRC\n");
        }
        else
        {
          printf("\n\nRX: received full message with INVALID CRC\n");
          printf("Received CRC: 0x%04X, Calculated CRC: 0x%04X\n", receivedcrc, calculatedcrc);
        }
      }

      if (mm.finished)
      {
        printf("\n\nRX: received full message\n");
        print_man_message(&mm);
        man_to_trame(&mm, &tr);
        printf("\n");
        print_trame(&tr);

        while (xQueueSend(recv_queue, &tr, portMAX_DELAY) == pdFALSE)
        {
          vTaskDelay(0);
        }

        printf("\nFlushing and going again\n");
        init_trame(&tr);
        init_man_message(&mm);
      }
    }

    vTaskDelay(0);
    start_rx(); // re-arm for next reception ??????
  }
  vTaskDelay(0);
}

void tx_task(void *arg)
{
  uint8_t datastr[89] = {0};
  trame tr;
  init_trame(&tr);

  printf("\nTX: now looping\n");

  for (;;)
  {

    int queue_ret = xQueueReceive(send_queue, &tr, portMAX_DELAY);
    if (queue_ret == pdFALSE)
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }
    else
    {
      trame_to_buffer(&tr, datastr);
      uint16_t size = trame_size(&tr);

      printf("sending message: ");
      for (int j = 0; j < size; j++)
      {
        printf("0x%02X ", datastr[j]);
      }
      printf("\n\n");

      send_msg(datastr, trame_size(&tr));
    }

    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void app_main()
{
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
