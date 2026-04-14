#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"

// Pinos
#define BOTAO_GPIO 4
#define LED_GPIO   18

// Configurações de tempo
#define DEBOUNCE_TEMPO_MS 50
#define TEMPO_LIMITE_US   10000000ULL // 10 segundos em microssegundos (10 * 1000 * 1000)

void app_main(void) {
  // Configuração do LED (Saída)
  gpio_reset_pin(LED_GPIO);
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
  gpio_set_level(LED_GPIO, 0);

  // Configuração do Botão
  gpio_reset_pin(BOTAO_GPIO);
  gpio_set_direction(BOTAO_GPIO, GPIO_MODE_INPUT);
  gpio_set_pull_mode(BOTAO_GPIO, GPIO_FLOATING); 

  int estado_led = 0;
  int ultimo_estado_botao = 1; // Inicia em 1 porque o botão possui pull-up
  int64_t tempo_acionamento = 0;

  while (1) {
    int leitura_atual = gpio_get_level(BOTAO_GPIO);

    // Debounce
    if (leitura_atual != ultimo_estado_botao) {
      vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TEMPO_MS)); 
            
      // Lê novamente para ver se o estado se manteve
      if (gpio_get_level(BOTAO_GPIO) == leitura_atual) {
                
        // Verifica se o botão acabou de ser pressionado (1 -> 0)
        if (leitura_atual == 0 && ultimo_estado_botao == 1) {
                    
          estado_led = !estado_led; // Inverte o estado atual
          gpio_set_level(LED_GPIO, estado_led);

          if (estado_led == 1) {
            tempo_acionamento = esp_timer_get_time(); // Se o LED está ligado, pega o tempo atual do sistema
          }
        }
        ultimo_estado_botao = leitura_atual;
      }
    }

    // Temporizador de segurança
    if (estado_led == 1) {
      int64_t tempo_atual = esp_timer_get_time();
            
      if ((tempo_atual - tempo_acionamento) >= TEMPO_LIMITE_US) {
        estado_led = 0; 
        gpio_set_level(LED_GPIO, 0);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}   