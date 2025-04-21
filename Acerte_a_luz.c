//Tarefa de revisão do módulo de capacitação
//Aluna: Maryana Souza Silveira
//Residência Tecnológica em Sistemas Embarcados - EmbarcaTech - CEPEDI

#include <stdio.h> //Biblioteca de entrada/saída padrão
#include "pico/stdlib.h" //Biblioteca padrão da Raspberry Pi Pico
#include "hardware/adc.h" //Biblioteca do conversor analógico/digital
#include "hardware/i2c.h" //Biblioteca de controle do protocolo I2C
#include "hardware/timer.h" //Biblioteca de funções de timer
#include "hardware/pio.h" //Biblioteca para controle de PIO
#include "Acerte_a_luz.pio.h" //Código do programa para controle de LEDs WS2812
#include "lib/ssd1306.h" //Biblioteca para controle do display SSD1306
#include "lib/font.h" //Biblioteca de fontes para o display SSD1306

#define Button_B 6 //Pino do botão B
#define Matrix_Pin 7 // Pino de controle dos LEDs WS2812 (matriz 5x5)
#define Green_Led 11 // Pino do LED verde RGB
#define Red_Led 13 // Pino do LED vermelho RGB
#define Buzzer 21 // Pino do buzzer
#define Joy_X_Pin 26  // GPIO para eixo X do joystick
#define Joy_Y_Pin 27  // GPIO para eixo Y do joystick

#define I2C_PORT i2c1 // Define o barramento I2C
#define I2C_SDA 14 // Define o pino SDA
#define I2C_SCL 15 // Define o pino SCL
#define endereco 0x3C // Endereço do display

#define Musical_Note_Do 132 // Frequência da nota dó
#define Musical_Note_Si 247.5 // Frequência da nota si

ssd1306_t ssd; // Inicializa a estrutura do display

uint8_t Led_Array[25] = { //Status da matriz 5x5 (leds inicialmente apagados)
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0,
};

typedef struct { //Estrutura para organização das variáveis de controle do jogo
  uint8_t wins; //Variável para contagem dos acertos (máximo 10)
  uint8_t losses; //Variável para contagem dos erros (máximo 5)
  uint8_t blue_light_posit; //Variável para controle da posição do cursor azul
  uint8_t yellow_light_posit; //Variável para controle da posição da luz amarela aleatória
  bool defeat_sound; //Variável para controle do som de erro emitido pelo buzzer
  bool victory_sound; //Variável para controle do som de acerto emitido pelo buzzer
} game_t;

game_t game; //Define a estrutura do jogo

void initial_configs(); //Função para setar as configurações iniciais
void gpio_irq_handler(uint gpio, uint32_t events); //Função da interrupção
bool move_yellow_light(struct repeating_timer *t);  //Função de callback do timer para mover a luz aleatória
uint8_t convert_joy_to_matrix(uint8_t linha, uint8_t coluna); //Função para converter valores de x e y em uma posição na matriz 5x5
void turn_on_leds(uint8_t matriz[25]); //Função para acender os leds da matriz 5x5
static inline void put_pixel(uint32_t pixel_grb); //Função para atualizar um LED
static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b); //Função para converter RGB para uint32_t
void ssd1306_square(ssd1306_t *ssd, uint8_t x_display, uint8_t y_display); //Função para desenhar um quadrado no display SSD1306
void musical_note(uint32_t frequencia, uint32_t tempo_ms); //Função para tocar a nota musical no buzzer
int64_t turn_off_greenled(alarm_id_t, void *user_data); //Função para desligar o led verde
int64_t turn_off_redled(alarm_id_t, void *user_data); //Função para desligar o led vermelho
void reset_game(const char *msg, uint8_t reaction_matrix[25]); //Função para reiniciar o jogo

//Função principal
//Realiza leitura dos eixos x e y do joystick, os converte para variações no display e na matriz 5x5,
//Aciona o buzzer, caso necessário, e reinicia o jogo quando há vitória ou derrota.
int main(){
  initial_configs(); //Inicializa as configurações
  gpio_set_irq_enabled_with_callback(Button_B, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler); //Habilita a interrupção no botão B
  struct repeating_timer timer; //Cria uma estrutura de timer
  add_repeating_timer_ms(1000, move_yellow_light, NULL, &timer); //Adiciona um timer de 1 segundo para movimento da luz amarela

  uint16_t adc_value_x; //Variável para armazenar o valor do ADC do eixo X do joystick
  uint16_t adc_value_y;  //Variável para armazenar o valor do ADC do eixo Y do joystick

  uint8_t defeat_reaction[25] = { //Matriz para ser exibida quando o usuário erra 5 vezes
    1, 0, 0, 0, 1,
    0, 1, 1, 1, 0,
    0, 0, 0, 0, 0,
    0, 1, 0, 1, 0,
    0, 0, 0, 0, 0,
  };
  
  uint8_t victory_reaction[25] = { ///Matriz para ser exibida quando o usuário acerta 10 vezes
    0, 1, 1, 1, 0,
    1, 0, 0, 0, 1,
    0, 0, 0, 0, 0,
    0, 1, 0, 1, 0,
    0, 0, 0, 0, 0,
  };

  while (true) {
    adc_select_input(1); //Seleciona o ADC para eixo X
    adc_value_x = adc_read(); //Lê o valor do ADC
    uint8_t ADX_display = (adc_value_x*119)/4080; //Converte o valor de x do joystick para o valor da linha do quadrado no display
    uint8_t ADX_matrix = (adc_value_x*4)/3600; //Converte o valor de x do joystick para o valor da linha da luz azul na matriz 5x5 

    adc_select_input(0); //Seleciona o ADC para eixo Y
    adc_value_y = adc_read(); //Lê o valor do ADC
    uint8_t ADY_display = (adc_value_y*55)/4080; //Converte o valor de y do joystick para o valor da coluna do quadrado no display
    uint8_t ADY_matrix = (adc_value_y*4)/3800; //Converte o valor de y do joystick para o valor da coluna da luz azul na matriz 5x5

    game.blue_light_posit = convert_joy_to_matrix(ADX_matrix, ADY_matrix); //Converte os valores de linha e coluna da luz azul em uma posição da matriz (de 0 a 24)

    Led_Array[game.blue_light_posit] = 1; //Modifica o valor da matriz na posição encontrada para acender a luz azul
    turn_on_leds(Led_Array); //Atualiza os leds da matriz

    ssd1306_fill(&ssd, false); // Limpa o display
    ssd1306_square(&ssd, ADX_display, 56 - ADY_display); // Desenha o quadrado que acompanha o joystick (calibrado para que o movimento em y acompanhe o joystick)
    ssd1306_send_data(&ssd); // Atualiza o display

    if (game.defeat_sound==1){ //Verifica se o usuário errou 
      musical_note(Musical_Note_Do, 500); //Toca o som de erro no buzzer
      game.defeat_sound=0; //Atualiza a variável de controle do som de erro
    }
    if (game.victory_sound==1){ //Verifica se o usuário acertou
      musical_note(Musical_Note_Si, 500); //Toca o som de acerto no buzzer
      game.victory_sound=0; //Atualiza a variável de controle do som de acerto
    }

    if(game.losses == 5){ //Verifica se o usuário errou 5 vezes
      reset_game("Que pena. Tente novamente.", defeat_reaction); //Reinicia o jogo e printa "carinha triste" na matriz 5x5
    }

    if(game.wins == 10){ //Verifica se o usuário acertou 10 vezes
      reset_game("Parabéns! Você arrasou!", victory_reaction); //Reinicia o jogo e printa "carinha feliz" na matriz 5x5
    }
    //Verifica se a luz azul e a luz amarela estão em posições diferentes para que uma não apague a outra
    if (Led_Array[game.blue_light_posit] != Led_Array[game.yellow_light_posit]){
      Led_Array[game.blue_light_posit] = 0; //Se forem diferentes, apaga a luz azul
    } else {
      Led_Array[game.blue_light_posit] = 2; //Se forem iguais, apaga a luz azul e mantém a amarela acesa
    }
    turn_on_leds(Led_Array); //Atualiza os leds da matriz
        
    sleep_ms(50); // Aguarda 50ms
  }
}

void initial_configs(){ //Função para setar as configurações iniciais
  stdio_init_all(); //Inicializa a comunicação serial

  gpio_init(Button_B); //Inicializa o pino do botão B
  gpio_set_dir(Button_B, GPIO_IN); //Define o pino do botão B como entrada
  gpio_pull_up(Button_B); //Habilita o resistor de pull-up do botão B
  gpio_init(Green_Led); //Inicializa o pino do led verde
  gpio_set_dir(Green_Led, GPIO_OUT); //Define o pino do led verde como saída
  gpio_init(Red_Led); //Inicializa o pino do led vermelho
  gpio_set_dir(Red_Led, GPIO_OUT); //Define o pino do led vermelho como saída
  gpio_init(Buzzer); //Inicializa o pino do buzzer
  gpio_set_dir(Buzzer, GPIO_OUT); //Define o pino do buzzer como saída

  adc_init(); //Inicializa o ADC
  adc_gpio_init(Joy_X_Pin); //Inicializa o pino do eixo X do joystick
  adc_gpio_init(Joy_Y_Pin); //Inicializa o pino do eixo Y do joystick

  //Inicializa o PIO
  PIO pio = pio0; 
  int sm = 0; 
  uint offset = pio_add_program(pio, &ws2812_program);
  //Inicializa o programa para controle dos LEDs WS2812
  ws2812_program_init(pio, sm, offset, Matrix_Pin, 800000, false); //Inicializa o programa para controle dos LEDs WS2812

  i2c_init(I2C_PORT, 400 * 1000); //Inicializa o barramento I2C a 400kHz
  gpio_set_function(I2C_SDA, GPIO_FUNC_I2C); //Configura a função do pino SDA
  gpio_set_function(I2C_SCL, GPIO_FUNC_I2C); //Configura a função do pino SCL
  gpio_pull_up(I2C_SDA); //Habilita o pull-up do pino SDA
  gpio_pull_up(I2C_SCL); //Habilita o pull-up do pino SCL
  ssd1306_init(&ssd, 128, 64, false, endereco, I2C_PORT); //Inicializa o display
  ssd1306_config(&ssd); //Configura o display
  ssd1306_send_data(&ssd); //Envia os dados para o display
  //Limpa o display. O display inicia com todos os pixels apagados.
  ssd1306_fill(&ssd, false);
  ssd1306_send_data(&ssd);
}

void gpio_irq_handler(uint gpio, uint32_t events) { //Função de interrupção
  static uint32_t last_interrupt_time = 0;  //Variável para armazenar o último tempo de interrupção
  uint32_t current_time = get_absolute_time(); //Variável para armazenar o tempo atual

  //Verifica se o tempo entre interrupções é maior que 200ms
  if (current_time - last_interrupt_time > 200000) {  //200ms de debounce
    if (game.yellow_light_posit ==  game.blue_light_posit){ //Caso a luz azul e a amarela estejam na mesma posição
      game.wins++; //Incrementa o número de acertos
      printf("\nVocê acertou! Pontuação: %d. ", game.wins); //Apresenta no monitor serial a pontuação do usuário
      gpio_put(Green_Led, 1); //Acende o led verde rgb
      add_alarm_in_ms(500, turn_off_greenled, NULL, false); //Adiciona um alarme para desligar o led verde em 0,5 segundos
      game.victory_sound = 1; //Atualiza variável para toque de acerto no buzzer
    } else{ //Caso as luzes não estejam na mesma posição
      game.losses++; //Incrementa o número de erros
      printf("\nVocê errou! Se errar mais %d vezes você perde! ", 5-game.losses); //Apresenta no monitor serial as chances restantes do usuário
      gpio_put(Red_Led, 1); //Acende o led vermelho rgb
      add_alarm_in_ms(500, turn_off_redled, NULL, false); //Adiciona um alarme para desligar o led vermelho em 0,5 segundos
      game.defeat_sound = 1; //Atualiza a variável para toque de erro no buzzer
    }
    last_interrupt_time = current_time;  // Atualiza o tempo da última interrupção
  }
}

bool move_yellow_light(struct repeating_timer *t){ //Função de callback do timer para mover a luz aleatória
  //Desliga o led da posição atual da luz
  Led_Array[game.yellow_light_posit] = 0;
  turn_on_leds(Led_Array);
  //Gera uma nova linha e coluna na matriz 5x5 e as converte para uma posição na matriz
  uint8_t luz_x = rand() % 5;
  uint8_t luz_y = rand() % 5;
  game.yellow_light_posit = convert_joy_to_matrix(luz_x, luz_y);
  //Acende o led da nova posição da luz
  Led_Array[game.yellow_light_posit] = 2;
  turn_on_leds(Led_Array);

  return true;
}

uint8_t convert_joy_to_matrix(uint8_t coluna, uint8_t linha){ //Função para converter valores de x e y em uma posição na matriz 5x5
  //Verifica qual a linha da matriz para retornar um valor crescente ou decrescente nas linhas
  if (linha == 0 || linha == 2 || linha == 4) { //Caso a linha tenha valores decrescentes
    return (4 - coluna) + linha * 5; //Retorna a posição na matriz
  } else { //Caso a linha tenha valores crescentes
    return coluna + linha * 5;
  }
}

void turn_on_leds(uint8_t matriz[25]){ //Função para acender os leds da matriz 5x5
  uint32_t yellow = urgb_u32(60, 60, 0); //Variável para armazenar a intensidade da luz amarela
  uint32_t blue = urgb_u32(0, 0, 60); //Variável para armazenar a intensidade da lus azuk

  for (int i = 0; i < 25; i++) { //Acende de acordo com a cor na matriz de leds, 1 - azul, 2 - amarelo, 3 - ambas, 0 - apagado
    switch (matriz[i]){ 
      case 1:
        put_pixel(blue);
      break;
      case 2:
        put_pixel(yellow);
      break;
      case 3:{
        put_pixel(blue);
        put_pixel(yellow);
      }
      break;
      default:
        put_pixel(urgb_u32(0, 0, 0));
      break;
    }
  }
}

static inline void put_pixel(uint32_t pixel_grb){ //Função para atualizar um LED
  pio_sm_put_blocking(pio0, 0, pixel_grb << 8u); //Atualiza o LED com a cor fornecida
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b){ //Função para converter RGB para uint32_t
  return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b); //Retorna a cor em formato uint32_t 
}

void ssd1306_square(ssd1306_t *ssd, uint8_t x_display, uint8_t y_display){ //Função para desenhar um quadrado no display
  for (uint8_t i = 0; i < 8; ++i){ //Desenha um quadrado de 8x8 pixels
    static uint8_t square[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}; //Define o quadrado
    uint8_t line = square[i]; //Obtém a linha do quadrado
    for (uint8_t j = 0; j < 8; ++j){ //Desenha a coluna do quadrado
      ssd1306_pixel(ssd, x_display + i, y_display + j, line & (1 << j)); //Desenha o pixel
    }
  }
}

void musical_note(uint32_t freq, uint32_t time_ms) { //Função para tocar a nota musical no buzzer
  uint32_t delay = 1000000 / (freq * 2);  //Calcula atraso em microssegundos para geração da onda quadrada
  uint32_t cicle = freq * time_ms / 1000;  //Calcula o número de ciclos necessários

  for (uint32_t i = 0; i < cicle; i++) { //Loop para geração da onda quadrada
      gpio_put(Buzzer, 1); //Liga o buzzer
      sleep_us(delay); //Espera pelo tempo de atraso calculado
      gpio_put(Buzzer, 0); //Desliga o buzzer
      sleep_us(delay); //Espera novamente pelo tempo de atraso
  }
}

int64_t turn_off_greenled(alarm_id_t, void *user_data){ // Função para desligar o led verde
  gpio_put(Green_Led, 0); // desliga o led verde
  return 0;
}

int64_t turn_off_redled(alarm_id_t, void *user_data){ // Função para desligar o led vermelho
  gpio_put(Red_Led, 0); // desliga o led vermelho
  return 0;
}

void reset_game(const char *msg, uint8_t reaction_matrix[25]){ //Função para reiniciar o jogo
  printf("\n%s", msg); //Printa a mensagem de vitória/derrota
  turn_on_leds(reaction_matrix); //Mostra uma "carinha" na matriz 5x5
  sleep_ms(2000); //Aguarda 2 segundos
  game.wins =0; //Reseta a variável de acertos
  game.losses=0; //Reseta a variável de erros
}