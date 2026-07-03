#include <stdlib.h>

#include "./ringbuffer.h"
#include <stdint.h>

void Arb_init(Atomic_ringbuffer *arb, uint16_t size) {
  arb->buffer = (atomic_char *)malloc(size * sizeof(atomic_char));
  arb->head = 0;
  arb->tail = 0;
  arb->size = size;
}

bool Arb_is_empty(Atomic_ringbuffer *arb) {
  // If head equals tail, no elements are waiting to be read
  return atomic_load(&arb->head) == atomic_load(&arb->tail);
}

bool Arb_is_full(Atomic_ringbuffer *arb) {
  uint16_t head = atomic_load(&arb->head);
  uint16_t tail = atomic_load(&arb->tail);

  return ((head + 1) % arb->size) == tail;
}

Arb_err Arb_put(Atomic_ringbuffer *arb, char item) {
  uint16_t head = atomic_load_explicit(&arb->head, memory_order_relaxed);
  uint16_t tail = atomic_load_explicit(&arb->tail, memory_order_acquire);

  if (((head + 1) % arb->size) == tail) {
    return ARB_FULL;
  }

  atomic_store_explicit(&arb->buffer[head], item, memory_order_relaxed);

  atomic_store_explicit(&arb->head, (head + 1) % arb->size,
                        memory_order_release);

  return ARB_OK;
}

Arb_err Arb_get(Atomic_ringbuffer *arb, char *item) {
  uint16_t tail = atomic_load_explicit(&arb->tail, memory_order_relaxed);
  uint16_t head = atomic_load_explicit(&arb->head, memory_order_acquire);

  if (head == tail) {
    return ARB_EMPTY;
  }

  *item = atomic_load_explicit(&arb->buffer[tail], memory_order_relaxed);

  atomic_store_explicit(&arb->tail, (tail + 1) % arb->size,
                        memory_order_release);

  return ARB_OK;
}
