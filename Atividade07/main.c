#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

// Definições
#define TXD_PIN (GPIO_NUM_17)
#define RXD_PIN (GPIO_NUM_18)
#define LED_PIN (GPIO_NUM_4)
#define UART_PORT (UART_NUM_2)
#define RX_BUF_SIZE 1024

static const char *TAG = "UART_ISR_TIMER";
static QueueHandle_t uart_queue;
static TaskHandle_t tx_task_handle = NULL;

// 1. CALLBACK DO TIMER (Interrupção de Tempo)
// Executada a cada 2 segundos para "acordar" o envio
void periodic_timer_callback(void* arg) {
  xTaskNotifyGive(tx_task_handle); 
}

// 2. TASK DE ENVIO (TX)
void tx_task(void *arg) {
  bool estado = true;
  while (1) {
    // Aguarda o sinal do Timer
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
    const char* msg = estado ? "LIGAR" : "DESLIGAR";
    ESP_LOGI(TAG, "Enviando comando: %s", msg);
    uart_write_bytes(UART_PORT, msg, strlen(msg));
      
    estado = !estado;
  }
}

// 3. TASK DE RECEPÇÃO (RX - Baseada em Eventos)
void rx_task(void *arg) {
  uart_event_t event;
  uint8_t* dtmp = (uint8_t*) malloc(RX_BUF_SIZE);
    
  while (1) {
    // Aguarda eventos da UART (Interrupção de Hardware)
    if (xQueueReceive(uart_queue, (void *)&event, portMAX_DELAY)) {
      bzero(dtmp, RX_BUF_SIZE);
            
      switch (event.type) {
        case UART_DATA:
          uart_read_bytes(UART_PORT, dtmp, event.size, portMAX_DELAY);
          ESP_LOGW(TAG, "Eco Recebido: %s", (char*)dtmp);
                  
          if (strcmp((char*)dtmp, "LIGAR") == 0) {
            gpio_set_level(LED_PIN, 1);
          } else if (strcmp((char*)dtmp, "DESLIGAR") == 0) {
            gpio_set_level(LED_PIN, 0);
          }
          break;
        default:
          break;
        }
      }
    }
  free(dtmp);
}

void app_main(void) {
  // Configurar LED
  gpio_reset_pin(LED_PIN);
  gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

  // Configurar UART com Fila de Eventos
  const uart_config_t uart_config = {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_DEFAULT,
  };
    
  uart_driver_install(UART_PORT, RX_BUF_SIZE * 2, RX_BUF_SIZE * 2, 20, &uart_queue, 0);
  uart_param_config(UART_PORT, &uart_config);
  uart_set_pin(UART_PORT, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    // Criar as Tarefas
  xTaskCreate(rx_task, "uart_rx_task", 4096, NULL, 12, NULL);
  xTaskCreate(tx_task, "uart_tx_task", 4096, NULL, 10, &tx_task_handle);

  // Configurar e Iniciar o Timer de Hardware
  const esp_timer_create_args_t periodic_timer_args = {
    .callback = &periodic_timer_callback,
    .name = "periodic_envio"
  };
  esp_timer_handle_t periodic_timer;
  esp_timer_create(&periodic_timer_args, &periodic_timer);
  esp_timer_start_periodic(periodic_timer, 2000000); // 2 segundos (em us)
}
