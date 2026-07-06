#pragma once

#include <stdatomic.h>

typedef struct {
  atomic_char *buffer;
  atomic_char16_t head;
  atomic_char16_t tail;
  uint16_t size;
} Atomic_ringbuffer;

typedef enum {
  ARB_OK,
  ARB_FULL,
  ARB_EMPTY,
} Arb_err;


void Arb_init(Atomic_ringbuffer *arb, uint16_t size);

void Arb_deinit(Atomic_ringbuffer *arb);

bool Arb_is_empty(Atomic_ringbuffer *arb);

bool Arb_is_full(Atomic_ringbuffer *arb);

Arb_err Arb_put(Atomic_ringbuffer *arb, char item);

Arb_err Arb_get(Atomic_ringbuffer *arb, char *item);
