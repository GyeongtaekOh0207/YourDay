/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "clcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#ifdef __GNUC__
/* With GCC, small printf (option LD Linker->Libraries->Small printf
   set to 'Yes') calls __io_putchar() */
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif /* __GNUC__ */
#define ARR_CNT 10
#define CMD_SIZE 50
#define RX_BUFFER_SIZE 50

#define TRIG_PORT GPIOA
#define TRIG_PIN GPIO_PIN_0
#define ECHO_PORT GPIOA
#define ECHO_PIN GPIO_PIN_1
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart2;
UART_HandleTypeDef huart6;

/* USER CODE BEGIN PV */
uint8_t rx2char;
volatile unsigned char rx2Flag = 0;
volatile char rx2Data[RX_BUFFER_SIZE];
volatile unsigned char btFlag = 0;
uint8_t btchar;
char btData[RX_BUFFER_SIZE];

typedef struct {
  uint8_t hours;
  uint8_t minutes;
  uint8_t seconds;
} TimeStruct;

volatile TimeStruct currentTime = {0, 0, 0};
volatile uint8_t timeUpdated = 0;

char temperatureStr[5] = "0";
char humidityStr[5] = "0";
char lightStr[5] = "0";
volatile uint8_t sensorDataUpdated = 0;

uint32_t IC_Val1 = 0;
uint32_t IC_Val2 = 0;
uint32_t Difference = 0;
uint8_t Is_First_Captured = 0;
uint8_t Distance = 0;

volatile uint8_t startMeasurement = 0;
volatile uint32_t lastMeasurementTime = 0;
#define MEASUREMENT_INTERVAL 50

uint8_t backlightState = 0;
uint32_t backlightStartTime = 0;
#define BACKLIGHT_TIMEOUT 10000
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
void bluetooth_Event(void);
void updateTimeFromBluetooth(const char* timeStr);
void getTimeString(char* buffer, size_t bufferSize);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_USART6_UART_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart2, &rx2char, 1);
  HAL_UART_Receive_IT(&huart6, &btchar, 1);

  // LCD 초기화
  LCD_init(&hi2c1);

  // LCD 화면 클리어
  LCD_writeStringXY(0, 0, "                ");
  LCD_writeStringXY(1, 0, "                ");

  // 초기 시간과 센서 데이터 표시
  char timeBuff[17] = {0};
  char sensorBuff[17] = {0};

  // 초기 시간 표시 (Time: 00:00:00)
  getTimeString(timeBuff, sizeof(timeBuff));
  LCD_writeStringXY(0, 0, timeBuff);

  // 초기 센서 데이터 표시
  snprintf(sensorBuff, sizeof(sensorBuff), "T:%s H:%s L:%s", temperatureStr, humidityStr, lightStr);
  LCD_writeStringXY(1, 0, sensorBuff);

  // 타이머 인터럽트 시작
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if(rx2Flag)
    {
      rx2Flag = 0;
    }

    if(btFlag)
    {
      btFlag = 0;
      bluetooth_Event();
    }

    // 시간 또는 센서 데이터가 업데이트되면 LCD 갱신
    if(timeUpdated || sensorDataUpdated)
    {
      char timeBuff[17] = {0};
      char sensorBuff[17] = {0};

      // 현재 시간 가져오기
      getTimeString(timeBuff, sizeof(timeBuff));

      // LCD 첫 번째 줄에 시간 표시
      LCD_writeStringXY(0, 0, timeBuff);

      // LCD 두 번째 줄에 센서 데이터 표시
      snprintf(sensorBuff, sizeof(sensorBuff), "T:%s H:%s L:%s", temperatureStr, humidityStr, lightStr);
      LCD_writeStringXY(1, 0, sensorBuff);

      timeUpdated = 0;
      sensorDataUpdated = 0;
    }

    // 거리에 따라 백라이트 제어
    if(Distance < 80)
    {
      if(!backlightState) // 백라이트가 꺼져 있는 경우에만
      {
        LCD_backLightOn();
        backlightState = 1;
        backlightStartTime = HAL_GetTick(); // 현재 시간 기록
      }
      else
      {
        // 이미 켜져 있는 경우 타이머 갱신
        backlightStartTime = HAL_GetTick(); // 활동이 감지될 때마다 타이머 리셋
      }
    }
    else if(backlightState) // 80cm 이상이고 백라이트가 켜져 있을 때만 타임아웃 체크
    {
      uint32_t currentTick = HAL_GetTick();
      if(currentTick - backlightStartTime >= BACKLIGHT_TIMEOUT)
      {
        LCD_backLightOff();
        backlightState = 0;
      }
    }

    if(startMeasurement)
    {
        startMeasurement = 0;
        // 초음파 측정 트리거 시작
        HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_SET);
        HAL_Delay(1);  // 최소 10us 이상
        HAL_GPIO_WritePin(TRIG_PORT, TRIG_PIN, GPIO_PIN_RESET);
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 10000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_IC_InitTypeDef sConfigIC = {0};

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 84-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 84-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 10000-1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
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
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 9600;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, DHT11_Pin|TEST_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 LD2_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : DHT11_Pin TEST_LED_Pin */
  GPIO_InitStruct.Pin = DHT11_Pin|TEST_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA1 (ECHO) */
  GPIO_InitStruct.Pin = GPIO_PIN_1; // ECHO_PIN
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; // 타이머 입력 캡처를 위한 대체 기능 설정
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF1_TIM2; // TIM2의 AF 설정
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

/* USER CODE BEGIN 4 */
void bluetooth_Event()
{
  int i=0;
  char * pToken;
  char * pArray[ARR_CNT]={0};
  char recvBuf[CMD_SIZE]={0};
  char sendBuf[CMD_SIZE]={0};

  // 안전한 문자열 복사
  strncpy(recvBuf, btData, CMD_SIZE-1);
  recvBuf[CMD_SIZE-1] = '\0';

  pToken = strtok(recvBuf, "[@]");
  while(pToken != NULL)
  {
    pArray[i] = pToken;
    if(++i >= ARR_CNT)
      break;
    pToken = strtok(NULL, "[@]");
  }

  if(!strcmp(pArray[1], "TIME_SENSOR"))
  {
    // pArray[2] = 시간(HH:MM:SS), pArray[3] = 온도, pArray[4] = 습도, pArray[5] = 조도
    if(pArray[2] != NULL && pArray[3] != NULL && pArray[4] != NULL && pArray[5] != NULL)
    {
      // 블루투스로부터 받은 시간으로 내부 시간 초기화
      updateTimeFromBluetooth(pArray[2]);

      // 센서 데이터 저장 (전역 변수로 온도, 습도, 조도 저장)
      strncpy(temperatureStr, pArray[3], sizeof(temperatureStr)-1);
      temperatureStr[sizeof(temperatureStr)-1] = '\0';  // NULL 종료자 보장

      strncpy(humidityStr, pArray[4], sizeof(humidityStr)-1);
      humidityStr[sizeof(humidityStr)-1] = '\0';  // NULL 종료자 보장

      strncpy(lightStr, pArray[5], sizeof(lightStr)-1);
      lightStr[sizeof(lightStr)-1] = '\0';  // NULL 종료자 보장

      // LCD 업데이트는 메인 루프에서 처리
      sensorDataUpdated = 1;
    }
  }
  else if(!strncmp(pArray[1], " New conn", sizeof(" New conn")-1))
  {
    return;
  }
  else if(!strncmp(pArray[1], " Already log", sizeof(" Already log")-1))
  {
    return;
  }
  else
    return;

  if(pArray[0] != NULL && pArray[1] != NULL && pArray[2] != NULL)
  {
    snprintf(sendBuf, CMD_SIZE, "[%s]%s@%s\n", pArray[0], pArray[1], pArray[2]);
    HAL_UART_Transmit(&huart6, (uint8_t *)sendBuf, strlen(sendBuf), 0xFFFF);
  }
}

/**
  * @brief  Retargets the C library printf function to the USART.
  * @param  None
  * @retval None
  */
PUTCHAR_PROTOTYPE
{
  /* Place your implementation of fputc here */
  /* e.g. write a character to the USART and Loop until the end of transmission */
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 0xFFFF);

  return ch;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart->Instance == USART2)
  {
    static int i=0;

    // 버퍼 오버플로우 방지
    if(i < RX_BUFFER_SIZE-1)
    {
      rx2Data[i] = rx2char;
      if((rx2Data[i] == '\r') || (rx2Data[i] == '\n'))
      {
        rx2Data[i] = '\0';
        rx2Flag = 1;
        i = 0;
      }
      else
      {
        i++;
      }
    }
    else
    {
      // 버퍼가 가득 찼을 때 처리
      rx2Data[i] = '\0';
      rx2Flag = 1;
      i = 0;
    }

    HAL_UART_Receive_IT(&huart2, &rx2char, 1);
  }

  if(huart->Instance == USART6)
  {
    static int i=0;

    // 버퍼 오버플로우 방지
    if(i < RX_BUFFER_SIZE-1)
    {
      btData[i] = btchar;
      if((btData[i] == '\n') || (btData[i] == '\r'))
      {
        btData[i] = '\0';
        btFlag = 1;
        i = 0;
      }
      else
      {
        i++;
      }
    }
    else
    {
      // 버퍼가 가득 찼을 때 처리
      btData[i] = '\0';
      btFlag = 1;
      i = 0;
    }

    HAL_UART_Receive_IT(&huart6, &btchar, 1);
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == TIM3)
  {
    static uint8_t counter = 0;

    // 0.01초마다 카운터 증가, 100번에 1번(1초마다) 시간 업데이트
    counter++;
    if(counter >= 100)
    {
      counter = 0;

      // 1초에 한 번씩 시간 업데이트
      currentTime.seconds++;

      // 초, 분, 시간 업데이트 로직
      if(currentTime.seconds >= 60)
      {
        currentTime.seconds = 0;
        currentTime.minutes++;

        if(currentTime.minutes >= 60)
        {
          currentTime.minutes = 0;
          currentTime.hours++;

          if(currentTime.hours >= 24)
          {
            currentTime.hours = 0;
          }
        }
      }

      // 시간이 업데이트되었음을 표시하는 플래그
      timeUpdated = 1;
    }
    // 0.1초마다 거리 측정 시작
    uint32_t currentTickTime = HAL_GetTick();
    if(currentTickTime - lastMeasurementTime >= MEASUREMENT_INTERVAL && !startMeasurement)
    {
      startMeasurement = 1;
      lastMeasurementTime = currentTickTime;
    }
  }
}

// 블루투스로부터 받은 시간으로 내부 시간 초기화
void updateTimeFromBluetooth(const char* timeStr)
{
  if(timeStr != NULL)
  {
    int h = 0, m = 0, s = 0;
    if(sscanf(timeStr, "%d:%d:%d", &h, &m, &s) == 3)
    {
      // 유효한 시간 범위 확인
      if(h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60)
      {
        currentTime.hours = h;
        currentTime.minutes = m;
        currentTime.seconds = s;
      }
    }
  }
}

// 현재 시간을 문자열로 변환
void getTimeString(char* buffer, size_t bufferSize)
{
  if(buffer != NULL && bufferSize >= 9) // "HH:MM:SS" + '\0'
  {
    snprintf(buffer, bufferSize, "Time: %02d:%02d:%02d",
             currentTime.hours, currentTime.minutes, currentTime.seconds);
  }
  else if(buffer != NULL && bufferSize > 0)
  {
    buffer[0] = '\0'; // 버퍼 크기가 충분하지 않을 경우 빈 문자열 반환
  }
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if(htim->Instance == TIM2)
  {
    if(Is_First_Captured == 0) // 첫 번째 캡처 (상승 에지)
    {
      IC_Val1 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);
      Is_First_Captured = 1;

      // 캡처 모드를 하강 에지로 변경
      __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_FALLING);
    }
    else if(Is_First_Captured == 1) // 두 번째 캡처 (하강 에지)
    {
      IC_Val2 = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_2);

      // 타이머 카운터가 오버플로우된 경우
      if(IC_Val2 > IC_Val1)
      {
        Difference = IC_Val2 - IC_Val1;
      }
      else
      {
        Difference = ((0xFFFFFFFF - IC_Val1) + IC_Val2) + 1;
      }

      // 거리 계산 (microseconds)
      Distance = Difference * 0.034 / 2;

      // 센서 데이터가 업데이트되었음을 표시
      sensorDataUpdated = 1;

      Is_First_Captured = 0;

      // 캡처 모드를 상승 에지로 복원
      __HAL_TIM_SET_CAPTUREPOLARITY(htim, TIM_CHANNEL_2, TIM_INPUTCHANNELPOLARITY_RISING);
    }
  }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
