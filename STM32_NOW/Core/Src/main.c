/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Minesweeper via UART
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes*/
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Private variables*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t rx_byte;
char rx_buffer[16];
uint8_t rx_index = 0;
/* USER CODE END PV */

/* Private function prototypes*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);

/* USER CODE BEGIN PFP */
uint8_t Calculate_XOR_Checksum(uint8_t *data, uint16_t length);
/* USER CODE END PFP */

/* USER CODE BEGIN 0 */

/* ===== Minesweeper definitions */
#define MAX_SIZE 15
#define MINE     -1
#define IS_VALID(x, y) ((x) < fieldSize && (y) < fieldSize)

typedef enum { EASY, MEDIUM, HARD } Difficulty;

/* Field state */
int8_t minefield[MAX_SIZE][MAX_SIZE];
uint8_t fieldSize;
uint8_t mineCount;

/* ===== RNG ===== */
void RNG_Init(void)
{
    srand(HAL_GetTick());
}

/* ===== Clear field */
void ClearField(void)
{
    for (uint8_t i = 0; i < fieldSize; i++)
        for (uint8_t j = 0; j < fieldSize; j++)
            minefield[i][j] = 0;
}

/* ===== Place mines */
void PlaceMines(void)
{
    uint8_t placed = 0;

    while (placed < mineCount)
    {
        uint8_t x = rand() % fieldSize;
        uint8_t y = rand() % fieldSize;

        if (minefield[x][y] != MINE)
        {
            minefield[x][y] = MINE;
            placed++;
        }
    }
}

/* Count adjacent mines */
uint8_t CountAdjacentMines(uint8_t x, uint8_t y)
{
    int8_t dx, dy;
    uint8_t count = 0;

    for (dx = -1; dx <= 1; dx++)
    {
        for (dy = -1; dy <= 1; dy++)
        {
            if (dx == 0 && dy == 0) continue;

            int8_t nx = x + dx;
            int8_t ny = y + dy;

            if (nx >= 0 && ny >= 0 && IS_VALID(nx, ny))
            {
                if (minefield[nx][ny] == MINE)
                    count++;
            }
        }
    }
    return count;
}

/* Calculate numbers */
void CalculateNumbers(void)
{
    for (uint8_t i = 0; i < fieldSize; i++)
    {
        for (uint8_t j = 0; j < fieldSize; j++)
        {
            if (minefield[i][j] != MINE)
                minefield[i][j] = CountAdjacentMines(i, j);
        }
    }
}

/* Generate field */
void GenerateMinefield(Difficulty level)
{
    switch (level)
    {
        case EASY:   fieldSize = 5;  mineCount = 5;  break;
        case MEDIUM: fieldSize = 10; mineCount = 30; break;
        case HARD:   fieldSize = 15; mineCount = 50; break;
    }

    ClearField();
    PlaceMines();
    CalculateNumbers();
}

/* Print field via UART */
void PrintMinefield(void)
{
    char c;

    for (uint8_t i = 0; i < fieldSize; i++)
    {
        for (uint8_t j = 0; j < fieldSize; j++)
        {
            if (minefield[i][j] == MINE)
            {
                HAL_UART_Transmit(&huart2, (uint8_t*)" *", 2, 100);
            }
            else
            {
                c = '0' + minefield[i][j];   // 0..8 → ASCII
                HAL_UART_Transmit(&huart2, (uint8_t*)" ", 1, 100);
                HAL_UART_Transmit(&huart2, (uint8_t*)&c, 1, 100);
            }
        }
        HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 100);
    }
}


/* Process UART command */
void ProcessMineCommand(char* cmd)
{
    if (strcmp(cmd, "EASY") == 0)
        GenerateMinefield(EASY);
    else if (strcmp(cmd, "MEDIUM") == 0)
        GenerateMinefield(MEDIUM);
    else if (strcmp(cmd, "HARD") == 0)
        GenerateMinefield(HARD);
    else
    {
        HAL_UART_Transmit(&huart2, (uint8_t*)"Unknown level\r\n", 15, 100);
        return;
    }

    HAL_UART_Transmit(&huart2, (uint8_t*)"Minefield:\r\n", 12, 100);
    PrintMinefield();
}

/* USER CODE END 0 */

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    RNG_Init();

    while (1)
    {
        if (HAL_UART_Receive(&huart2, &rx_byte, 1, HAL_MAX_DELAY) == HAL_OK)
        {
            HAL_UART_Transmit(&huart2, &rx_byte, 1, 100);

            if (rx_byte == '\r' || rx_byte == '\n')
            {
                HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n", 2, 100);
                rx_buffer[rx_index] = '\0';

                uint8_t checksum = Calculate_XOR_Checksum(
                    (uint8_t*)rx_buffer, strlen(rx_buffer));

                char tx_buf[32];
                snprintf(tx_buf, sizeof(tx_buf),
                         "%s*%02X\r\n", rx_buffer, checksum);

                HAL_UART_Transmit(&huart2,
                    (uint8_t*)tx_buf, strlen(tx_buf), 100);

                if (strncmp(rx_buffer, "MINE ", 5) == 0)
                    ProcessMineCommand(&rx_buffer[5]);

                rx_index = 0;
            }
            else
            {
                if (rx_index < sizeof(rx_buffer) - 1)
                    rx_buffer[rx_index++] = rx_byte;
            }
        }
    }
}

/* USER CODE BEGIN 4 */
uint8_t Calculate_XOR_Checksum(uint8_t *data, uint16_t length)
{
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < length; i++)
        checksum ^= data[i];
    return checksum;
}
/* USER CODE END 4 */

/*System / HAL init */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1;

    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) {

    }
}
