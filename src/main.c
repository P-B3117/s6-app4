#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "hal/rmt_types.h"

#include "crc.h"
#include "rmt.h"
typedef struct
{
  char *configString;
  int configInt;
} taskConfig;

const uint8_t preambule = 0x55;
const uint8_t start = 0x7E;
const uint8_t end = 0x7E;
const uint8_t enteteLength = 4;
const uint8_t crc = 2;

typedef enum
{
  debut = 0x01,
  data = 0x02,
  fin = 0x03,
  nack = 0x04,
} com_typ;

typedef enum
{
  preambule_pos = 0,
  start_pos = 1,
  entete_pos = 2,
  charge_pos = 6,
} trame_pos; // crc and end pos depends on charge longueur

#pragma pack(push, 1)
typedef struct
{
  union
  {
    struct
    {
      uint8_t entete[4]; // type de comm, num sequence, longueur charge, volume
                         // dynamique
      uint8_t chargeUtile[80];
    } fields;

    uint8_t flat_buffer[84];
  };
  uint8_t chargeLength;
  uint8_t crc[2];
} trame;
#pragma pack(pop)

void init_trame(trame *t)
{
  if (t != NULL)
  {
    memset(t, 0, sizeof(trame));
  }
}

uint8_t crc_pos(trame *t) { return (charge_pos + t->chargeLength); }

uint8_t end_pos(trame *t) { return (charge_pos + t->chargeLength + 2); }

typedef struct
{
  uint8_t bitIndex;
  uint8_t messageIndex;
  uint8_t message[89];
  uint8_t currentIndex;
  bool current[2];
  bool finished;
} man_message;

uint8_t print_man_message(man_message *mm)
{
  printf("MM:\n  bitIndex: %hhu\n  messageIndex: %hhu\n ", mm->bitIndex,
         mm->messageIndex);
  printf("  Message[]:");
  for (uint8_t i = 0; i < mm->messageIndex; i++)
  {
    if (i % 8 == 0)
    {
      printf("\n    ");
    }
    printf(" %hhu,", mm->message[i]);
  }
  printf("\n  currentIndex: %hhu", mm->currentIndex);
  printf("\n  current[]: %hhu, %hhu", mm->current[0], mm->current[1]);
  printf("\n  finished: %s\n", mm->finished ? "true" : "false");

  return 0;
}

uint8_t init_man_message(man_message *mm)
{
  if (mm != NULL)
  {
    memset(mm, 0, sizeof(man_message));
  }

  return 0;
}

void follow(man_message *mm, uint8_t duration, bool level)
{

  if (mm->finished)
    return;

  // if long pulse, treated as 2 short pulses
  if (duration > RMT_FULL_BIT_THRESHOLD)
  {
    follow(mm, duration / 2, level);
    follow(mm, duration / 2, level);
    return;
  }

  // function logic
  mm->current[mm->currentIndex] = level;

  if (mm->currentIndex == 1)
  {
    // logic
    if (mm->current[1])
    {
      mm->message[mm->messageIndex] |= (1 << (7 - mm->bitIndex)); // MSB
    }

    // close
    mm->bitIndex++;
    mm->currentIndex = 0;
    mm->current[0] = 0;
    mm->current[1] = 0;
  }
  else
  {
    mm->currentIndex++;
  }

  if (mm->bitIndex >= 8)
  {
    if (mm->message[mm->messageIndex] == end && mm->messageIndex >= 8)
    {
      mm->finished = true;
    }

    // close
    mm->bitIndex = 0;
    mm->messageIndex++;
  }
}

// TODO currently copies the mm to the trame, could be improved.
void man_to_trame(man_message *mm, trame *out)
{
  out->chargeLength = mm->messageIndex - 7;
  memcpy(out->flat_buffer, &mm->message[2], mm->messageIndex - 4);
  memcpy(out->crc, &mm->message[mm->messageIndex - 3], 2);
}

static void decode_manchester_charles(rmt_symbol_word_t *sym, trame *out,
                                      man_message *mm)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    follow(mm, sym[i].duration0, sym[i].level0);
    follow(mm, sym[i].duration1, sym[i].level1);
  }
}

static uint8_t decode_manchester(rmt_symbol_word_t *sym)
{
  uint8_t out = 0;

  for (int i = 0; i < 8; i++)
  {
    rmt_symbol_word_t s = sym[i];
    bool bit;

    // LOW→HIGH = 1
    if (s.level0 == 0 && s.level1 == 1)
    {
      bit = 1;
    }
    // HIGH→LOW = 0
    else
    {
      bit = 0;
    }

    out = (out << 1) | bit;
  }

  return out;
}

void initTaskConfig(taskConfig *conf, char *str, int num)
{
  conf->configString = str;
  conf->configInt = num;
}

void task(void *pvParameters)
{
  taskConfig *conf = (taskConfig *)pvParameters;

  while (1)
  {
    printf("tasking");
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

// loop send toutes les trames dans une queue.
void task_sender(void *pvParameters)
{
  ////RMT
}

// loop receive toutes les trames dans une queue
void task_receiver(void *pvParameters)
{
  ////RMT
}

// crc is 2 byte array (to ensure easy concatenation)
void calc_crc(trame *input)
{
  uint16_t crc = crc16(input->flat_buffer, input->chargeLength + 4);
  input->crc[0] = crc & 0xFF;
  input->crc[1] = (crc >> 8) & 0xFF;
}

// input is message (whole ass tableau annexe) to send to other esp,
// manchester the binary of the message
void send(trame *input) {}

// decodes the buffer from manchester to standard bytes
void receive(uint8_t *buffer, uint8_t start, uint8_t end) {}

void rx_task(void *arg)
{
  SemaphoreHandle_t sem = get_rx_semaphore();
  rmt_symbol_word_t local_symbols[BUFFER_SIZE];

  for (;;)
  {
    if (xSemaphoreTake(sem, portMAX_DELAY) == pdTRUE)
    {
      get_rx_symbols(local_symbols, BUFFER_SIZE);

      for (int i = 0; i < BUFFER_SIZE; i++)
      {
        printf("Symbol %d: level0=%d duration0=%d, level1=%d duration1=%d\n",
               i, local_symbols[i].level0, local_symbols[i].duration0,
               local_symbols[i].level1, local_symbols[i].duration1);
      }

      start_rx(); // re-arm for next reception
    }
    vTaskDelay(0);
  }
}

void tx_task(void *arg)
{
  uint8_t data = 0;

  for (;;)
  {
    send_byte(0x55);
    send_byte(data);
    data++;
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void app_main()
{
  setup_transmission();

  printf("Starting tasks rx...\n");
  xTaskCreatePinnedToCore(rx_task, "receive", 2048, NULL, 1, NULL, 1);
  printf("Starting tasks tx...\n");
  xTaskCreatePinnedToCore(tx_task, "send", 2048, NULL, 1, NULL, 1);

  // while (1)
  // {
  //   vTaskDelay(3000);
  // }

  // taskConfig* conf = &(taskConfig){};
  // initTaskConfig(conf, "lol", 2);

  // xTaskCreate(task, "task 1", 2048, conf, 1, NULL);
}
