/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"
#include "oled1_init.h"

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <stdio.h>

#define MAX_LENGTH 24

const int TRIGGER_PIN = 16;
const int ECHO_PIN = 17;

QueueHandle_t queue_echo_time;
QueueHandle_t queue_distance;
SemaphoreHandle_t semaphore_trigger;

void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == ECHO_PIN && (events == 0x8 || events == 0x4)) {
        int time = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(queue_echo_time, &time, 0);
    }
}

void trigger_task() {
    gpio_init(TRIGGER_PIN);
    gpio_set_dir(TRIGGER_PIN, GPIO_OUT);

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        gpio_put(TRIGGER_PIN, true);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_put(TRIGGER_PIN, false);
        xSemaphoreGive(semaphore_trigger);
    }
}

void echo_task() {
    gpio_init(ECHO_PIN);
    gpio_set_dir(ECHO_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(ECHO_PIN, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_callback);

    int start_time = 0;
    int end_time = 0;

    while (true) {
        if (xQueueReceive(queue_echo_time, &start_time, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (xQueueReceive(queue_echo_time, &end_time, pdMS_TO_TICKS(10)) == pdTRUE) {
                double distance = ((end_time - start_time)*340)/20000.0;
                xQueueSend(queue_distance, &distance, 0);
            } 
        }
        start_time = 0;
        end_time = 0;
    }
}

void oled_task() {
    
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GFX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    printf("Inicializando btn and LEDs\n");
    oled1_btn_led_init();

    double distance = 0;
    char text[MAX_LENGTH] = "";

    while (true) {
        if (xSemaphoreTake(semaphore_trigger, 0) == pdTRUE) {
            gfx_clear_buffer(&disp);
            if (xQueueReceive(queue_distance, &distance, pdMS_TO_TICKS(1000)) == pdTRUE) {
                snprintf(text, MAX_LENGTH, "Distancia: %.3lf cm", distance);
                vTaskDelay(pdMS_TO_TICKS(50));
                gfx_draw_string(&disp, 0, 0, 1, text);
                gfx_draw_line(&disp, 0, 27, (int) ((distance/100)*128), 27);
            } else {
                snprintf(text, MAX_LENGTH, "Falha");
                vTaskDelay(pdMS_TO_TICKS(50));
                gfx_draw_string(&disp, 0, 0, 1, text);
                distance = 0;
            }
            gfx_show(&disp);
        }
    }
}

int main() {
    stdio_init_all();

    queue_echo_time = xQueueCreate(32, sizeof(int));
    queue_distance = xQueueCreate(32, sizeof(double));
    semaphore_trigger = xSemaphoreCreateBinary();

    xTaskCreate(trigger_task, "Trigger", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "Oled", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true);
}
