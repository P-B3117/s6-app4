#include "manchester_decoder.h"
#include <stdio.h>
#include <string.h>

// static const uint8_t preambule = 0x55;
// static const uint8_t start = 0x7E;
static const uint8_t end = 0x7E;
// static const uint8_t enteteLength = 4;
// static const uint8_t crc = 2;

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
  // if ((_follow_count % 2) == 0) {
  //   printf("%hhu%s", level, ((_follow_count % 16) == 0) ? "\n" : "");
  // }
  // _follow_count++;
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
uint16_t decode_manchester(rmt_symbol_word_t *sym, man_message *mm,
                                  uint16_t i, uint16_t n_sym) {

  for (; i < n_sym; i++) {
    follow(mm, sym[i].duration0, sym[i].level0);
    follow(mm, sym[i].duration1, sym[i].level1);
    if (mm->finished == true) {
      return i;
    }
  }

  return i;
}
