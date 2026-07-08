#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "examples.h"
#include "crc.h"
#include "message.h"
#include <stdio.h>

extern QueueHandle_t send_queue;
extern QueueHandle_t recv_queue;

void example_trame_first_message(trame *t) {

  create_trame(t, debut, 1, 3, 0, NULL);
}

void example_trame_second_message(trame *t) {
  create_trame(t, data, 2, 0, 3, (uint8_t[]){0x07, 0x04, 0x09});
}

void example_trame_third_message(trame *t) {
  create_trame(t, fin, 3, 0, 0, NULL);
}

void example_task(void *arg) {
    trame trame_list[TRAME_EXAMPLE_SIZE];

    int err = create_trame(&trame_list[0], debut, 1, TRAME_EXAMPLE_SIZE, 0, NULL);
    if (err != trame_ok) {
        printf("failed to create trame: %d\n", err);
    }

    // print_trame(&trame_list[0]);

    for (int i = 1; i < TRAME_EXAMPLE_SIZE - 1; i++) {
        uint8_t payload[3] = {7 + i, 4 + i, 9 + i};
        create_trame(&trame_list[i], data, i + 1, 0, 3, payload);
    }

    create_trame(&trame_list[TRAME_EXAMPLE_SIZE - 1], fin, TRAME_EXAMPLE_SIZE, 0, 0, NULL);

    // print_trame(&trame_list[1]);

    printf("sending big chunk of %d trames\n", TRAME_EXAMPLE_SIZE);

    // send big chunk to be sent
    for (int i = 0; i < TRAME_EXAMPLE_SIZE; i++) {
        xQueueSend(send_queue, &trame_list[i], portMAX_DELAY);
    }

    printf("big chunk sent\n");

    vTaskDelay(0);

    // receive big chunk sent
    for (int i = 0; i < TRAME_EXAMPLE_SIZE; i++) {
        xQueueReceive(recv_queue, &trame_list[i], portMAX_DELAY);
    }

    printf("received big chunk of %d trames\n", TRAME_EXAMPLE_SIZE);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }

}
