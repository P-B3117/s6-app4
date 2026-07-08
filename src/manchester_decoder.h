#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "hal/rmt_types.h"
#include "message.h"

typedef struct {
  uint8_t bitIndex;
  uint8_t messageIndex;
  uint8_t message[89];
  uint8_t currentIndex;
  bool current[2];
  bool last_was_zero;
  bool finished;
} man_message;

uint8_t init_man_message(man_message *mm);

uint8_t print_man_message(man_message *mm);

void man_to_trame(man_message *mm, trame *out);

uint16_t decode_manchester(rmt_symbol_word_t *sym, man_message *mm, uint16_t i,
                           uint16_t n_sym);
