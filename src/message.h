#pragma once

#include <stdint.h>

extern const uint8_t preambule;
extern const uint8_t start;
extern const uint8_t end;
extern const uint8_t enteteLength;
extern const uint8_t crc;

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

typedef enum {
  trame_ok = 0,
  trame_no_charge_permitted = 1,
  trame_init_failed = 2,
  trame_data_empty_charge = 3,
  trame_charge_utile_is_null = 4,
} trame_err;

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

void init_trame(trame *t);

trame_err create_trame(trame *t, uint8_t type, com_typ sequence, uint16_t sequenceLength,
                       uint8_t chargeLength, uint8_t *chargeUtile);

uint8_t trame_size(trame *t);

void print_trame(trame *t);

void trame_to_buffer(trame *t, uint8_t *buffer);

void calc_trame_crc(trame *input);
