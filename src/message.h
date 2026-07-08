#pragma once

#include <stdint.h>

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

uint8_t crc_pos(trame *t);

uint8_t end_pos(trame *t);

uint8_t trame_size(trame *t);

void init_trame(trame *t);

void print_trame(trame *t);

void trame_to_buffer(trame *t, uint8_t *buffer);
