#include "message.h"
#include <string.h>
#include <stdio.h>

static const uint8_t preambule = 0x55;
static const uint8_t start = 0x7E;
static const uint8_t end = 0x7E;
// static const uint8_t enteteLength = 4;
// static const uint8_t crc = 2;

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
