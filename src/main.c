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
typedef struct {
  char *configString;
  int configInt;
} taskConfig;

const uint8_t preambule = 0x55;
const uint8_t start = 0x7E;
const uint8_t end = 0x7E;
const uint8_t enteteLength = 4;
const uint8_t crc = 2;

typedef enum {
  debut = 0x01,
  data = 0x02,
  fin = 0x03,
  nack = 0x04,
} com_typ;

typedef enum {
  preambule_pos = 0,
  start_pos = 1,
  entete_pos = 2,
  charge_pos = 6,
} trame_pos; // crc and end pos depends on charge longueur

#pragma pack(push, 1)
typedef struct {
  union {
    struct {
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

uint8_t crc_pos(trame *t) { return (charge_pos + t->chargeLength); }

uint8_t end_pos(trame *t) { return (charge_pos + t->chargeLength + 2); }

uint8_t trame_size(trame *t) { return (t->chargeLength + 9); }

void init_trame(trame *t) {
  if (t != NULL) {
    memset(t, 0, sizeof(trame));
  }
}

void print_trame(trame *t) {
  if (t == NULL) {
    return;
  }

  printf("Trame:\n  Entete: ");
  printf("%02X, %02X, %02X, %02X, ", t->fields.entete[0], t->fields.entete[1],
         t->fields.entete[2], t->fields.entete[3]);
  printf("\n  ChargeUtile[]:");
  for (uint8_t i = 0; i < t->chargeLength; i++) {
    if (i % 8 == 0) {
      printf("\n    ");
    }
    printf(" %02X,", t->fields.chargeUtile[i]);
  }
  printf("\n  crc: %02X%02X", t->crc[0], t->crc[1]);
}

// pass empty buffer with already allocated memory and will fill it
void trame_to_buffer(trame *t, uint8_t *buffer) {
  buffer[preambule_pos] = preambule;
  buffer[start_pos] = start;
  memcpy(&buffer[entete_pos], t->flat_buffer, t->chargeLength + 4);
  memcpy(&buffer[crc_pos(t)], t->crc, 2);
  buffer[end_pos(t)] = end;
}

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

typedef struct {
  uint8_t bitIndex;
  uint8_t messageIndex;
  uint8_t message[89];
  uint8_t currentIndex;
  bool current[2];
  bool last_was_zero;
  bool finished;
} man_message;

uint8_t print_man_message(man_message *mm) {
  printf("MM:\n  bitIndex: %hhu\n  messageIndex: %hhu\n ", mm->bitIndex,
         mm->messageIndex);
  printf("  Message[]:");
  for (uint8_t i = 0; i < mm->messageIndex; i++) {
    if (i % 8 == 0) {
      printf("\n    ");
    }
    printf(" %02X,", mm->message[i]);
  }
  printf("\n  currentIndex: %hhu", mm->currentIndex);
  printf("\n  current[]: 0x%02X, 0x%02X", mm->current[0], mm->current[1]);
  printf("\n  finished: %s\n", mm->finished ? "true" : "false");

  return 0;
}

uint8_t init_man_message(man_message *mm) {
  if (mm != NULL) {
    memset(mm, 0, sizeof(man_message));
  }

  return 0;
}

uint8_t _follow_count = 1;

void follow(man_message *mm, uint8_t duration, bool level) {

  if (mm->finished)
    return;

  // if long pulse, treated as 2 short pulses
  if (duration == 1) {
    // mm->last_was_zero = false;
  } else if (duration > 2) {
      duration = 1;
  } else if (duration == 2) // TODO define long and short durations
  {
    follow(mm, 1, level);
    follow(mm, 1, level);
    return;
  } else if (duration == 0) {
    if (mm->currentIndex == 0 || mm->last_was_zero == true) {
      mm->last_was_zero = true;
      return;
    }
  }
  if ((_follow_count % 2) == 0) {
    printf("%hhu%s", level, ((_follow_count % 16) == 0) ? "\n" : "");
  }
  _follow_count++;
  // if very long pulse, its the first value of the next (edge case?)

  // printf("duration: %hhu, level: %d\n", duration, level);

  // function logic
  mm->current[mm->currentIndex] = level;

  if (mm->currentIndex == 1) {
    // logic
    if (mm->current[1]) {
      mm->message[mm->messageIndex] |= (1 << (7 - mm->bitIndex)); // MSB
    }

    // close
    mm->bitIndex++;
    mm->currentIndex = 0;
    mm->current[0] = 0;
    mm->current[1] = 0;
  } else {
    mm->currentIndex++;
  }

  if (mm->bitIndex >= 8) {
    if (mm->messageIndex >= 5) {
      uint8_t supposed_end = 2 + 4 + mm->message[4] + 2;
      if (mm->messageIndex >= supposed_end &&
          mm->message[mm->messageIndex] == end) {
        mm->finished = true;
      }
    }

    // close
    mm->bitIndex = 0;
    mm->messageIndex++;
    _follow_count = 1;
    mm->last_was_zero = false;
  }
}

// TODO currently copies the mm to the trame, could be improved.
void man_to_trame(man_message *mm, trame *out) {
  out->chargeLength = mm->message[4];
  memcpy(out->flat_buffer, &mm->message[2], mm->message[4] + 4);
  memcpy(out->crc, &mm->message[2 + 4 + mm->message[4]], 2);
}

// returns position it was at when the message was finished. n_sym - 1 means
// looped everything;
static uint16_t decode_manchester(rmt_symbol_word_t *sym, trame *out,
                                  man_message *mm, uint16_t i, uint16_t n_sym) {

  for (; i < n_sym; i++) {
    follow(mm, sym[i].duration0, sym[i].level0);
    follow(mm, sym[i].duration1, sym[i].level1);
    if (mm->finished == true) {
      return i;
    }
  }

  return i;
}

void initTaskConfig(taskConfig *conf, char *str, int num) {
  conf->configString = str;
  conf->configInt = num;
}

// crc 2 byte
void calc_crc(trame *input) {
  uint16_t crc = crc16(input->flat_buffer, input->chargeLength + 4);
  input->crc[0] = crc & 0xFF;
  input->crc[1] = (crc >> 8) & 0xFF;
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
      int pos = decode_manchester(&local_symbols[decoder_pos], &tr, &mm, 0, BUFFER_SIZE);
      print_man_message(&mm);
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
        printf("\n\n\n\n\nRX: received full message\n");
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
    vTaskDelay(pdMS_TO_TICKS(500));
    send_msg(datastr, size);
    vTaskDelay(pdMS_TO_TICKS(5000));
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
