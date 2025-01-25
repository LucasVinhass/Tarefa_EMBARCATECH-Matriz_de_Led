#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"

// arquivo .pio
#include "main.pio.h"

// Definições de linhas e colunas do teclado
#define rows 4 // O teclado tem 4 linhas
#define cols 4 // O teclado tem 4 colunas
// Mapeamento dos pinos do teclado (linhas e colunas do teclado matricial)
const uint8_t row_pins[rows] = {8, 1, 6, 5};  // Pinos das linhas (R1, R2, R3, R4)
const uint8_t col_pins[cols] = {4, 3, 2, 27}; // Pinos das colunas (C1, C2, C3, C4)

// Mapeamento do teclado matricial (associa as teclas aos caracteres)
const char key_map[rows][cols] = {
    {'1', '2', '3', 'A'}, // Primeira linha
    {'4', '5', '6', 'B'}, // Segunda linha
    {'7', '8', '9', 'C'}, // Terceira linha
    {'*', '0', '#', 'D'}  // Quarta linha
};

// Inicialização do teclado matricial
void keypad_init()
{
    // Inicializa as linhas do teclado (como saídas)
    for (int i = 0; i < rows; i++)
    {
        gpio_init(row_pins[i]);
        gpio_set_dir(row_pins[i], GPIO_OUT);
        gpio_put(row_pins[i], false); // Inicializa as linhas como LOW
    }

    // Inicializa as colunas do teclado (como entradas) com pull-down
    for (int i = 0; i < cols; i++)
    {
        gpio_init(col_pins[i]);
        gpio_set_dir(col_pins[i], GPIO_IN);
        gpio_pull_down(col_pins[i]); // Garante que a leitura seja 0 quando não pressionado
    }
}

// Leitura das teclas pressionadas no teclado matricial
char read_keypad()
{
    for (int row = 0; row < rows; row++)
    {
        gpio_put(row_pins[row], 1); // Ativa uma linha de cada vez

        for (int col = 0; col < cols; col++)
        {
            if (gpio_get(col_pins[col]))
            {                               // Se a tecla correspondente for pressionada
                gpio_put(row_pins[row], 0); // Desativa a linha após detectar a tecla
                sleep_ms(20);               // Espera um curto período para estabilizar a leitura
                return key_map[row][col];   // Retorna o valor da tecla pressionada
            }
        }
        gpio_put(row_pins[row], 0); // Desativa a linha
    }
    return '\0'; // Retorna '\0' se nenhuma tecla for pressionada
}

// número de LEDs
#define NUM_PIXELS 25

// pino de saída
#define OUT_PIN 7

// botão de interupção
const uint button_0 = 5;
const uint button_1 = 6;

// vetor para criar imagem na matriz de led - 1
double desenho[25] = {0.0, 0.3, 0.3, 0.3, 0.0,
                      0.0, 0.3, 0.0, 0.3, 0.0,
                      0.0, 0.3, 0.3, 0.3, 0.0,
                      0.0, 0.3, 0.0, 0.3, 0.0,
                      0.0, 0.3, 0.3, 0.3, 0.0};

// vetor para criar imagem na matriz de led - 2
double desenho2[25] = {1.0, 0.0, 0.0, 0.0, 1.0,
                       0.0, 1.0, 0.0, 1.0, 0.0,
                       0.0, 0.0, 1.0, 0.0, 0.0,
                       0.0, 1.0, 0.0, 1.0, 0.0,
                       1.0, 0.0, 0.0, 0.0, 1.0};

// imprimir valor binário
void imprimir_binario(int num)
{
    int i;
    for (i = 31; i >= 0; i--)
    {
        (num & (1 << i)) ? printf("1") : printf("0");
    }
}

// rotina da interrupção
static void gpio_irq_handler(uint gpio, uint32_t events)
{
    printf("Interrupção ocorreu no pino %d, no evento %d\n", gpio, events);
    printf("HABILITANDO O MODO GRAVAÇÃO");
    reset_usb_boot(0, 0); // habilita o modo de gravação do microcontrolador
}

// rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(double b, double r, double g)
{
    unsigned char R, G, B;
    R = r * 255;
    G = g * 255;
    B = b * 255;
    return (G << 24) | (R << 16) | (B << 8);
}

// rotina para acionar a matrix de leds - ws2812b
void desenho_pio(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b)
{

    for (int16_t i = 0; i < NUM_PIXELS; i++)
    {
        if (i % 2 == 0)
        {
            valor_led = matrix_rgb(desenho[24 - i], r = 0.0, g = 0.0);
            pio_sm_put_blocking(pio, sm, valor_led);
        }
        else
        {
            valor_led = matrix_rgb(b = 0.0, desenho[24 - i], g = 0.0);
            pio_sm_put_blocking(pio, sm, valor_led);
        }
    }
    imprimir_binario(valor_led);
}

// função principal
int main()
{
    printf("Iniciando o programa\n");
    PIO pio = pio0;
    bool ok;
    uint16_t i;
    uint32_t valor_led;
    double r = 0.0, b = 0.0, g = 0.0;
    //
    stdio_init_all(); // Inicializa a comunicação com o terminal
    keypad_init();    // Inicializa o teclado matricial

    // coloca a frequência de clock para 128 MHz, facilitando a divisão pelo clock
    ok = set_sys_clock_khz(128000, false);

    // Inicializa todos os códigos stdio padrão que estão ligados ao binário.
    stdio_init_all();

    printf("iniciando a transmissão PIO");
    if (ok)
        printf("clock set to %ld\n", clock_get_hz(clk_sys));

    // configurações da PIO
    uint offset = pio_add_program(pio, &main_program);
    uint sm = pio_claim_unused_sm(pio, true);
    main_program_init(pio, sm, offset, OUT_PIN);

    // // inicializar o botão de interrupção - GPIO5
    // gpio_init(button_0);
    // gpio_set_dir(button_0, GPIO_IN);
    // gpio_pull_up(button_0);

    // // inicializar o botão de interrupção - GPIO5
    // gpio_init(button_1);
    // gpio_set_dir(button_1, GPIO_IN);
    // gpio_pull_up(button_1);

    // // interrupção da gpio habilitada
    // gpio_set_irq_enabled_with_callback(button_0, GPIO_IRQ_EDGE_FALL, 1, &gpio_irq_handler);
    //
    // looop infinito
    while (true)
    {
        char key = read_keypad(); // Lê a tecla pressionada no teclado

        if (key != '\0')
        { // Se uma tecla foi pressionada
            switch (key)
            {
            case '0':
                break;
            case '1':
                break;
            case '2':
                break;
            case '3':
                break;
            case '4':
                break;
            case '5':             
                break;
            case '6':
                break;
            case 'A':
                break;
            case 'B':
                break;
            case 'C':
                break;
            case 'D':
                break;
            case '#':
                break;
            case '*':
                break;
            }
            printf("Tecla pressionada: %c\n", key); // Exibe a tecla pressionada no terminal
            sleep_ms(200);                          // Espera 200ms para evitar múltiplas leituras da mesma tecla
        }
    }
    return 0;

    /*while (true)
    {

        if (gpio_get(button_1)) // botão em nível alto
        {
            // rotina para escrever na matriz de leds com o emprego de PIO - desenho 2
            desenho_pio(desenho, valor_led, pio, sm, r, g, b);
        }
        else
        {
            // rotina para escrever na matriz de leds com o emprego de PIO - desenho 1
            desenho_pio(desenho2, valor_led, pio, sm, r, g, b);
        }

        sleep_ms(500);
        printf("\nfrequeência de clock %ld\r\n", clock_get_hz(clk_sys));
    }*/
}
