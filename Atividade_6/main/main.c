#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// Pinos
#define BOTAO_GPIO 4
#define LED_GPIO   18

// Filas e Timers
static QueueHandle_t fila_eventos_botao = NULL;
esp_timer_handle_t timer_10s;
esp_timer_handle_t timer_2s;

static void timer_10s_callback(void* arg) {
    gpio_set_level(LED_GPIO, 0); // Passaram 10s de inatividade. Desliga o LED.
}

static void timer_2s_callback(void* arg) {
    gpio_set_level(LED_GPIO, 0); // O botão foi segurado por 2s. Desligamento forçado.
    esp_timer_stop(timer_10s);  // Cancela o timer de 10s para ele não tentar apagar de novo à toa
}

static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(fila_eventos_botao, &gpio_num, NULL);  // Envia o número do pino para a fila e acorda a Task
}

static void task_controle_botao(void* arg) {
    uint32_t io_num;

    while(1) {
        if(xQueueReceive(fila_eventos_botao, &io_num, portMAX_DELAY)) {
            vTaskDelay(pdMS_TO_TICKS(50)); 
            
            int nivel = gpio_get_level(BOTAO_GPIO);
            
            xQueueReset(fila_eventos_botao); // Limpa a fila para ignorar os repiques elétricos que aconteceram nesses 50ms 

            if(nivel == 0) { 
                gpio_set_level(LED_GPIO, 1);
                
                esp_timer_stop(timer_10s); 
                esp_timer_start_once(timer_10s, 10000000ULL); // 10.000.000 us = 10s
                
                esp_timer_stop(timer_2s);
                esp_timer_start_once(timer_2s, 2000000ULL);   // 2.000.000 us = 2s
            } 
            else if (nivel == 1) { 
                esp_timer_stop(timer_2s);
            }
        }
    }
}

void app_main(void) {
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, 0);

    gpio_reset_pin(BOTAO_GPIO);
    gpio_set_direction(BOTAO_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOTAO_GPIO, GPIO_FLOATING); 
    gpio_set_intr_type(BOTAO_GPIO, GPIO_INTR_ANYEDGE); // Configura a interrupção para Qualquer Borda (ANYEDGE) - Detecta apertar e soltar 

    // Criação da Fila e da Task
    fila_eventos_botao = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(task_controle_botao, "task_botao", 2048, NULL, 10, NULL);

    // Instalação do Serviço de Interrupção
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BOTAO_GPIO, gpio_isr_handler, (void*) BOTAO_GPIO);

    // Criação dos Timers
    const esp_timer_create_args_t timer_10s_args = {
        .callback = &timer_10s_callback,
        .name = "timer_10s"
    };
    esp_timer_create(&timer_10s_args, &timer_10s);

    const esp_timer_create_args_t timer_2s_args = {
        .callback = &timer_2s_callback,
        .name = "timer_2s"
    };
    esp_timer_create(&timer_2s_args, &timer_2s);

    printf("Sistema Iniciado - Aguardando Interrupcoes...\n");
}