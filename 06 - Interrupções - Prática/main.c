#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#define BOTAO 13
#define LED 10

int counter = 0;
bool ledEstado = 0;

QueueHandle_t interruptQueue;
TimerHandle_t xTimer;

// estrutura do evento
typedef struct {
    int pin;
    int level;
} gpio_event_t;

// callback do timer
void timeout(TimerHandle_t xTimer) {
    ledEstado = 0;
    printf("Desligando LED (timer)\n");
}

// ISR (leve)
static void IRAM_ATTR gpio_isr_handler(void *params) {
    static uint32_t last_isr_time = 0;
    uint32_t current_time = xTaskGetTickCountFromISR();

    if ((current_time - last_isr_time) > pdMS_TO_TICKS(50)) {

        gpio_event_t event;
        event.pin = (int)params;
        event.level = gpio_get_level(event.pin);

        xQueueSendFromISR(interruptQueue, &event, NULL);
        last_isr_time = current_time;
    }
}

// task principal de eventos
void signalTriggered(void *params) {
    gpio_event_t event;

    uint32_t press_time = 0;
    bool pressed = false;
    bool long_press_triggered = false;

    while (1) {

        // recebe eventos da ISR (não bloqueia totalmente)
        if (xQueueReceive(interruptQueue, &event, pdMS_TO_TICKS(10))) {

            if (event.level == 0) {
                // pressionou
                press_time = xTaskGetTickCount();
                pressed = true;
                long_press_triggered = false;
            } 
            else {
                // soltou
                pressed = false;

                if (!long_press_triggered) {
                    // clique normal
                    counter++;

                    if (ledEstado) {
                        xTimerReset(xTimer, 0);
                    } else {
                        ledEstado = 1;
                        xTimerReset(xTimer, 0);
                    }

                    printf("LED: %d\n", ledEstado);
                }
            }
        }

        // detecção de long press sem soltar
        if (pressed && !long_press_triggered) {
            uint32_t now = xTaskGetTickCount();

            if ((now - press_time) >= pdMS_TO_TICKS(2000)) {
                ledEstado = 0;
                xTimerStop(xTimer, 0);
                long_press_triggered = true;

                printf("FORCADO: LED OFF (segurou 2s)\n");
            }
        }
    }
}

void app_main() {

    // botão
    gpio_config_t configPin = {
        .pin_bit_mask = (1ULL << BOTAO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&configPin);

    // LED
    gpio_config_t configLed = {
        .pin_bit_mask = (1ULL << LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&configLed);

    // timer (10s)
    xTimer = xTimerCreate("Desligamento LED",
                          pdMS_TO_TICKS(10000),
                          false,
                          NULL,
                          timeout);

    // fila
    interruptQueue = xQueueCreate(10, sizeof(gpio_event_t));

    // task
    xTaskCreate(signalTriggered, "signalTriggered", 2048, NULL, 10, NULL);

    // ISR
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOTAO, gpio_isr_handler, (void *) BOTAO);

    // loop principal
    while (true) {
        gpio_set_level(LED, ledEstado);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}