/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    main.c
  * @brief   UART Minesweeper (STM32) compatible with VS SFML client
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ================= DEFINES ================= */
#define RX_BUFFER_SIZE 64
#define TX_BUFFER_SIZE 256

#define CMD_MINEFIELD 'M'
#define CMD_CLICK     'C'
#define CMD_ABORT     'A'

#define DIFF_EASY     'E'
#define DIFF_MEDIUM   'M'
#define DIFF_HARD     'H'

#define STATUS_OK     0x00
#define STATUS_LOSE   0x01
#define STATUS_WIN    0x02
#define STATUS_ERR    0xFF

#define MAX_SIZE 15
#define MINE     -1
#define FLAG     254

/* ================= HANDLES ================= */
UART_HandleTypeDef huart2;
TIM_HandleTypeDef htim2;

/* ================= BUFFERS ================= */
uint8_t rxBuf[RX_BUFFER_SIZE];
uint8_t txBuf[TX_BUFFER_SIZE];
uint8_t rxIndex = 0;

/* ================= GAME DATA ================= */
int8_t  minefield[MAX_SIZE][MAX_SIZE];
uint8_t opened[MAX_SIZE][MAX_SIZE];

uint8_t fieldSize = 0;
uint8_t mineCount = 0;
uint16_t openedTotal = 0;
uint8_t gameOver = 1;

/* ================= TIMER ================= */
uint32_t timerSeconds = 0;
uint8_t timerRunning = 0;

/* ================= PROTOTYPES ================= */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);

uint8_t XOR_Checksum(uint8_t *data, uint16_t len);
void UART_Task(void);
void ProcessPacket(uint8_t *packet, uint8_t totalLen);

void GenerateMinefield(uint8_t level);
void ClearOpened(void);
void ClearField(void);
void PlaceMines(void);
uint8_t CountAdjacent(uint8_t x, uint8_t y);
void FloodOpen(uint8_t x, uint8_t y);

void HandleMinefield(uint8_t *packet);
void HandleClick(uint8_t *packet);
void HandleAbort(void);

void SendMinefieldResponse(void);
void SendClickResponse(uint8_t status);
void SendError(uint8_t cmd, uint8_t err);
void SendTimer(void);

/* ================= CHECKSUM ================= */
uint8_t XOR_Checksum(uint8_t *data, uint16_t len)
{
    uint8_t c = 0;
    for(uint16_t i=0;i<len;i++) c ^= data[i];
    return c;
}

/* ================= RNG ================= */
void RNG_Init(void)
{
    srand(HAL_GetTick());
}

/* ================= FIELD ================= */
void ClearOpened(void)
{
    openedTotal = 0;
    for(uint8_t i=0;i<fieldSize;i++)
        for(uint8_t j=0;j<fieldSize;j++)
            opened[i][j] = 0;
}

void ClearField(void)
{
    for(uint8_t i=0;i<fieldSize;i++)
        for(uint8_t j=0;j<fieldSize;j++)
            minefield[i][j] = 0;
}

void PlaceMines(void)
{
    uint8_t placed = 0;
    while(placed < mineCount)
    {
        uint8_t x = rand() % fieldSize;
        uint8_t y = rand() % fieldSize;
        if(minefield[x][y] != MINE)
        {
            minefield[x][y] = MINE;
            placed++;
        }
    }
}

uint8_t CountAdjacent(uint8_t x, uint8_t y)
{
    uint8_t cnt = 0;
    for(int8_t dx=-1; dx<=1; dx++)
        for(int8_t dy=-1; dy<=1; dy++)
        {
            if(!dx && !dy) continue;
            int8_t nx = x + dx;
            int8_t ny = y + dy;
            if(nx>=0 && ny>=0 && nx<fieldSize && ny<fieldSize)
                if(minefield[nx][ny] == MINE)
                    cnt++;
        }
    return cnt;
}

void GenerateMinefield(uint8_t level)
{
    gameOver = 0;
    timerSeconds = 0;
    timerRunning = 1;

    switch(level)
    {
        case DIFF_EASY:   fieldSize = 5;  mineCount = 5;  break;
        case DIFF_MEDIUM: fieldSize = 10; mineCount = 20; break;
        case DIFF_HARD:   fieldSize = 15; mineCount = 30; break;
        default:          fieldSize = 5;  mineCount = 5;  break;
    }

    ClearField();
    ClearOpened();
    PlaceMines();

    for(uint8_t i=0;i<fieldSize;i++)
        for(uint8_t j=0;j<fieldSize;j++)
            if(minefield[i][j] != MINE)
                minefield[i][j] = CountAdjacent(i,j);
}

void FloodOpen(uint8_t x, uint8_t y)
{
    if(x>=fieldSize || y>=fieldSize) return;
    if(opened[x][y]) return;

    opened[x][y] = 1;
    openedTotal++;

    if(minefield[x][y] != 0) return;

    for(int8_t dx=-1; dx<=1; dx++)
        for(int8_t dy=-1; dy<=1; dy++)
            if(dx || dy)
            {
                int8_t nx=x+dx, ny=y+dy;
                if(nx>=0 && ny>=0 && nx<fieldSize && ny<fieldSize)
                    FloodOpen(nx,ny);
            }
}

/* ================= RESPONSES ================= */
void SendError(uint8_t cmd, uint8_t err)
{
    txBuf[0]=cmd;
    txBuf[1]=err;
    txBuf[2]=0;
    txBuf[3]=XOR_Checksum(txBuf,3);
    HAL_UART_Transmit(&huart2,txBuf,4,100);
}

void SendMinefieldResponse(void)
{
    uint16_t idx=0;
    txBuf[idx++]=CMD_MINEFIELD;
    txBuf[idx++]=STATUS_OK;
    txBuf[idx++]=fieldSize*fieldSize;

    for(uint8_t i=0;i<fieldSize;i++)
        for(uint8_t j=0;j<fieldSize;j++)
            txBuf[idx++] = (minefield[i][j]==MINE)?9:minefield[i][j];

    txBuf[idx]=XOR_Checksum(txBuf,idx);
    HAL_UART_Transmit(&huart2,txBuf,idx+1,100);
}

void SendClickResponse(uint8_t status)
{
    uint16_t idx=0;
    txBuf[idx++]=CMD_CLICK;
    txBuf[idx++]=status;

    uint16_t lenPos = idx++;
    uint8_t payloadLen = 0;

    for(uint8_t i=0;i<fieldSize;i++)
        for(uint8_t j=0;j<fieldSize;j++)
            if(opened[i][j])
            {
                txBuf[idx++]=i;
                txBuf[idx++]=j;
                txBuf[idx++]=(minefield[i][j]==MINE)?9:minefield[i][j];
                payloadLen+=3;
            }

    txBuf[lenPos]=payloadLen;
    txBuf[idx]=XOR_Checksum(txBuf,idx);
    HAL_UART_Transmit(&huart2,txBuf,idx+1,100);
}

void SendTimer(void)
{
    if(!timerRunning) return;
    char buf[16];
    int n = sprintf(buf,"T%lu\n",timerSeconds);
    HAL_UART_Transmit(&huart2,(uint8_t*)buf,n,10);
}

/* ================= HANDLERS ================= */
void HandleMinefield(uint8_t *packet)
{
    GenerateMinefield(packet[2]);
    SendMinefieldResponse();
}

void HandleClick(uint8_t *packet)
{
    if(packet[1]!=2 || gameOver) return;

    uint8_t x = packet[2];
    uint8_t y = packet[3];
    if(x>=fieldSize || y>=fieldSize) return;

    FloodOpen(x,y);

    uint8_t status = STATUS_OK;

    if(minefield[x][y] == MINE)
    {
        status = STATUS_LOSE;
        gameOver = 1;
        timerRunning = 0;
    }
    else if(openedTotal >= fieldSize*fieldSize - mineCount)
    {
        status = STATUS_WIN;
        gameOver = 1;
        timerRunning = 0;
    }

    SendClickResponse(status);
}

void HandleAbort(void)
{
    gameOver = 1;
    timerRunning = 0;
    openedTotal = 0;
}

/* ================= PACKET ================= */
void ProcessPacket(uint8_t *packet, uint8_t totalLen)
{
    uint8_t payloadLen = packet[1];
    uint8_t chkIndex = payloadLen + 2;

    if(totalLen != payloadLen + 3) return;
    if(packet[chkIndex] != XOR_Checksum(packet, chkIndex)) return;

    switch(packet[0])
    {
        case CMD_MINEFIELD: HandleMinefield(packet); break;
        case CMD_CLICK:     HandleClick(packet);     break;
        case CMD_ABORT:     HandleAbort();           break;
        default:            SendError(packet[0], STATUS_ERR);
    }
}

/* ================= UART ================= */
void UART_Task(void)
{
    uint8_t b;
    if(HAL_UART_Receive(&huart2,&b,1,HAL_MAX_DELAY) == HAL_OK)
    {
        rxBuf[rxIndex++] = b;

        if(rxIndex >= 3)
        {
            uint8_t totalLen = rxBuf[1] + 3;
            if(rxIndex == totalLen)
            {
                ProcessPacket(rxBuf,totalLen);
                rxIndex = 0;
            }
            else if(rxIndex > totalLen)
                rxIndex = 0;
        }

        if(rxIndex >= RX_BUFFER_SIZE) rxIndex = 0;
    }
}

/* ================= TIMER ================= */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == TIM2 && timerRunning)
    {
        timerSeconds++;
        SendTimer();
    }
}

/* ================= MAIN ================= */
int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART2_UART_Init();
    MX_TIM2_Init();

    HAL_TIM_Base_Start_IT(&htim2);
    RNG_Init();

    while(1)
    {
        UART_Task();
    }
}

/* ================= INIT ================= */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct={0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct={0};

    RCC_OscInitStruct.OscillatorType=RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState=RCC_HSI_ON;
    RCC_OscInitStruct.PLL.PLLState=RCC_PLL_NONE;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    RCC_ClkInitStruct.ClockType=RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource=RCC_SYSCLKSOURCE_HSI;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct,FLASH_LATENCY_0);
}

static void MX_USART2_UART_Init(void)
{
    huart2.Instance=USART2;
    huart2.Init.BaudRate=115200;
    huart2.Init.WordLength=UART_WORDLENGTH_8B;
    huart2.Init.StopBits=UART_STOPBITS_1;
    huart2.Init.Parity=UART_PARITY_NONE;
    huart2.Init.Mode=UART_MODE_TX_RX;
    HAL_UART_Init(&huart2);
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

static void MX_TIM2_Init(void)
{
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 8000-1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 1000-1;
    HAL_TIM_Base_Init(&htim2);
}

void Error_Handler(void)
{
    __disable_irq();
    while(1){}
}
