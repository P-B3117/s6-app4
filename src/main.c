#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "driver/gpio.h"

typedef struct {
    char* configString;
    int configInt;
} taskConfig;


const uint8_t preambule = 0x55;
const uint8_t start = 0x7E;
const uint8_t end = 0x7E;
const uint8_t enteteLength = 4;
const uint8_t crc = 2;

enum com_typ {
    debut = 0x01,
    data = 0x02,
    fin = 0x03,
    nack = 0x04,
};

enum trame_pos {
    preambule_pos = 0,
    start_pos = 1,
    entete_pos = 2,
    charge_pos = 6,
}; // crc and end pos depends on charge longueur


typedef struct {
    uint8_t* entete; // type de comm, num sequence, longueur charge, volume dynamique
    uint8_t* chargeUtile;
    uint8_t chargeLength;
    uint8_t* crc;
} trame;

uint8_t crc_pos(trame* t) {
    return (charge_pos + t->chargeLength);
}

uint8_t end_pos(trame* t) {
    return (charge_pos + t->chargeLength + 2);
}

void initTaskConfig(taskConfig* conf, char* str, int num) {
    conf->configString = str;
    conf->configInt = num;
}

void task(void *pvParameters) {
    taskConfig* conf = (taskConfig*)pvParameters;

    while(1) {
        printf("tasking");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// loop send toutes les trames dans une queue.
void task_sender(void *pvParameters){

}

// loop receive toutes les trames dans une queue
void task_receiver(void *pvParameters){

}

// input is charge utile, crc is 2 byte array (to ensure easy concatenation)
void calc_crc(trame* input){

}

// input is message (whole ass tableau annexe) to send to other esp, manchester the binary of the message
void send(trame* input) {

}

// decodes the buffer from manchester to standard bytes
void receive(uint8_t* buffer, uint8_t start, uint8_t end) {

}

void app_main() {
    taskConfig* conf = &(taskConfig){};
    initTaskConfig(conf, "lol", 2);

    xTaskCreate(task, "task 1", 2048, conf, 1, NULL);
}
