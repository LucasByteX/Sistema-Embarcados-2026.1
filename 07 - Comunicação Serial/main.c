#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// Definições de Pinos
#define TXD_PIN (GPIO_NUM_11)
#define RXD_PIN (GPIO_NUM_1)
#define LED_PIN (GPIO_NUM_6) 

// Configurações da UART
#define UART_PORT_NUM      (UART_NUM_1)
#define UART_BAUD_RATE     (115200)
#define BUF_SIZE           (1024)

void init_hw() {
    // 1. Configuração da UART1
    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    
    uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_PORT_NUM, &uart_config);
    uart_set_pin(UART_PORT_NUM, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // 2. Configuração do LED
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    
    printf("Hardware inicializado. TX:10, RX:11, LED:6\n");
}

void uart_task(void *arg) {
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    bool estado = false;

    while (1) {
        // Alterna entre as strings
        const char *msg = estado ? "LIGAR" : "DESLIGAR";
        estado = !estado;

        // Envia a string pela UART2
        uart_write_bytes(UART_PORT_NUM, msg, strlen(msg));
        printf("\n[ENVIADO]: %s\n", msg);

        // Aguarda a recepção (Loopback via Jumper)
        // O timeout de 500ms é suficiente para capturar o retorno
        int len = uart_read_bytes(UART_PORT_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(500));
        
        if (len > 0) {
            data[len] = '\0'; // Finaliza a string
            printf("[RECEBIDO]: %s\n", (char *)data);

            // Lógica de controle do LED baseada no recebimento
            if (strcmp((char *)data, "LIGAR") == 0) {
                gpio_set_level(LED_PIN, 1);
                printf(">> Ação: LED Aceso!\n");
            } 
            else if (strcmp((char *)data, "DESLIGAR") == 0) {
                gpio_set_level(LED_PIN, 0);
                printf(">> Ação: LED Apagado!\n");
            }
            else {
                printf(">> Ação: LIGAR/DESLIGAR Nao detectado, nenhuma acao tomada!\n");
            }
        } else {
            printf("[ERRO]: Nada recebido. Verifique os jumpers!\n");
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); // Espera 2 segundos para a próxima iteração
    }
    free(data);
}

void app_main(void) {
    init_hw();
    
    // Cria a tarefa de comunicação
    xTaskCreate(uart_task, "uart_task", 4096, NULL, 10, NULL);
}