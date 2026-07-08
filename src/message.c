#include "message.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "crc.h"

const uint8_t preambule = 0x55;
const uint8_t start = 0x7E;
const uint8_t end = 0x7E;
const uint8_t enteteLength = 4;
const uint8_t crc = 2;

uint8_t crc_pos(trame *t) { return (charge_pos + t->chargeLength); }

uint8_t end_pos(trame *t) { return (charge_pos + t->chargeLength + 2); }

uint8_t trame_size(trame *t) { return (t->chargeLength + 9); }

void init_trame(trame *t) {
  if (t != NULL) {
    memset(t, 0, sizeof(trame));
  }
}

trame_err create_trame(trame *t, uint8_t type, com_typ sequence, uint16_t sequenceLength,
                       uint8_t chargeLength, uint8_t *chargeUtile) {

  if (type != data && chargeLength != 0) {
    return trame_no_charge_permitted;
  }

  if (type == data) {
    sequenceLength = 0;

    if (chargeLength == 0) {
      return trame_data_empty_charge;
    }

    if (chargeUtile == NULL) {
      return trame_charge_utile_is_null;
    }
    memcpy(t->fields.chargeUtile, chargeUtile, chargeLength);
    t->chargeLength = chargeLength;
  }

  if (t != NULL) {
    init_trame(t);
    t->fields.entete[0] = type;
    t->fields.entete[1] = sequence;
    t->fields.entete[2] = chargeLength;
    t->fields.entete[3] = sequenceLength;
    calc_trame_crc(t);
  } else {
      return trame_init_failed;
  }

  return trame_ok;
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
  printf("\n  crc: %02X%02X\n", t->crc[0], t->crc[1]);
}

// pass empty buffer with already allocated memory and will fill it
void trame_to_buffer(trame *t, uint8_t *buffer) {
  buffer[preambule_pos] = preambule;
  buffer[start_pos] = start;
  memcpy(&buffer[entete_pos], t->flat_buffer, t->chargeLength + 4);
  memcpy(&buffer[crc_pos(t)], t->crc, 2);
  buffer[end_pos(t)] = end;
}

void calc_trame_crc(trame *input) {
  uint16_t crc_c = crc16(input->flat_buffer, input->chargeLength + 4);
  input->crc[0] = crc_c & 0xFF;
  input->crc[1] = (crc_c >> 8) & 0xFF;
}
