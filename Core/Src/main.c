/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* Application state kept for each hardware-backed parking place. The firmware
   keeps this separate from the raw GPIO value so short sensor glitches do not
   immediately become user-visible JSON events. */
typedef struct
{
  /* Stable logical state of one parking place after debounce processing. */
  uint8_t occupied;

  /* Set after the delayed "used" event was sent once for this occupation. */
  uint8_t usedReported;

  /* HAL tick timestamp from the moment the place became occupied. */
  uint32_t occupiedStartedAt;
} ParkingPlaceState;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* Timing and protocol limits for the parking-place state machine. */
#define SENSOR_DEBOUNCE_MS 20U                 /* Two equal samples are required before a sensor is accepted as active. */
#define PARKING_USED_REPORT_DELAY_MS 3000U     /* A car must remain detected for this time before "used" is reported. */
#define PARKING_COMMAND_BUFFER_SIZE 16U        /* Small line buffer for commands such as "STATUS". */
#define PARKING_STATE_FREE "free"
#define PARKING_STATE_USED "used"

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
static uint8_t uartRxByte;

/* The UART interrupt fills this line buffer byte by byte; the main loop
   copies and handles complete commands outside the interrupt context. */
static volatile char commandBuffer[PARKING_COMMAND_BUFFER_SIZE];
static volatile uint8_t commandLength;
static volatile uint8_t commandReady;
static volatile uint8_t commandOverflow;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static uint8_t IR_SensorDetected(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
static uint8_t IR_SensorDebounced(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
static void ParkingStation_UpdatePlace(const char *placeId, uint8_t occupied, ParkingPlaceState *placeState);
static void ParkingStation_PrintPlaceJson(const char *placeId, const char *state);
static void ParkingStation_PollCommand(uint8_t a12Occupied, uint8_t a41Occupied);
static void ParkingStation_HandleCommand(const char *command, uint8_t a12Occupied, uint8_t a41Occupied);
static void ParkingStation_PrintAllPlacesJson(uint8_t a12Occupied, uint8_t a41Occupied);
static void ParkingStation_UpdateFreeLeds(uint8_t a12Occupied, uint8_t a41Occupied);

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
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Initialize COM1 as the ST-LINK Virtual COM Port. This is the link used by
     serial terminals and by parking-station.html through the Web Serial API. */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }
  HAL_NVIC_SetPriority(LPUART1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(LPUART1_IRQn);
  if (HAL_UART_Receive_IT(&hcom_uart[COM1], &uartRxByte, 1U) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN BSP */

  /* USER CODE END BSP */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  ParkingPlaceState a12State = {0};
  ParkingPlaceState a41State = {0};

  /* Capture the start state so the firmware does not emit a false change event
     immediately after reset. A full snapshot can still be requested via STATUS. */
  uint8_t a12Occupied = IR_SensorDebounced(PARKING_PLACE_A12_GPIO_Port, PARKING_PLACE_A12_Pin);
  uint8_t a41Occupied = IR_SensorDebounced(PARKING_PLACE_A41_GPIO_Port, PARKING_PLACE_A41_Pin);

  a12State.occupied = a12Occupied;
  a41State.occupied = a41Occupied;
  a12State.occupiedStartedAt = a12Occupied ? HAL_GetTick() : 0U;
  a41State.occupiedStartedAt = a41Occupied ? HAL_GetTick() : 0U;

  /* Bring the physical availability LEDs into sync before entering the loop. */
  ParkingStation_UpdateFreeLeds(a12Occupied, a41Occupied);

  while (1)
  {
    /* Each cycle reads the live sensors, updates the event state machines,
       handles a possible serial command, and refreshes the free-place LEDs. */
    a12Occupied = IR_SensorDebounced(PARKING_PLACE_A12_GPIO_Port, PARKING_PLACE_A12_Pin);
    a41Occupied = IR_SensorDebounced(PARKING_PLACE_A41_GPIO_Port, PARKING_PLACE_A41_Pin);

    ParkingStation_UpdatePlace("A12", a12Occupied, &a12State);
    ParkingStation_UpdatePlace("A41", a41Occupied, &a41State);
    ParkingStation_PollCommand(a12Occupied, a41Occupied);
    ParkingStation_UpdateFreeLeds(a12Occupied, a41Occupied);

    HAL_Delay(10U);

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
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* The IR modules are wired as active-low occupancy inputs. Internal pull-ups
     keep the raw input at a defined "free" level when the sensor is inactive. */
  GPIO_InitStruct.Pin = PARKING_PLACE_A12_Pin | PARKING_PLACE_A41_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* The external green LEDs are active-high outputs. They are reset first and
     then switched to the correct free/used state after the first read. */
  HAL_GPIO_WritePin(PARKING_PLACE_A12_LED_GPIO_Port, PARKING_PLACE_A12_LED_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(PARKING_PLACE_A41_LED_GPIO_Port, PARKING_PLACE_A41_LED_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = PARKING_PLACE_A12_LED_Pin | PARKING_PLACE_A41_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/**
  * @brief  Reads the raw active-low IR sensor signal.
  * @retval 1 if the sensor currently reports an object, otherwise 0.
  */
static uint8_t IR_SensorDetected(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
  return HAL_GPIO_ReadPin(GPIOx, GPIO_Pin) == GPIO_PIN_RESET;
}

/**
  * @brief  Accepts a detection only when two samples remain active.
  * @note   This filters short spikes, but it also blocks the loop for
  *         SENSOR_DEBOUNCE_MS per sensor.
  */
static uint8_t IR_SensorDebounced(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
  uint8_t firstSample = IR_SensorDetected(GPIOx, GPIO_Pin);

  HAL_Delay(SENSOR_DEBOUNCE_MS);

  return firstSample && IR_SensorDetected(GPIOx, GPIO_Pin);
}

/**
  * @brief  Updates one parking-place state machine and emits JSON events.
  *
  * A place becomes "used" only after the configured delay. A "free" event is
  * emitted only if a "used" event was already sent for the current occupation.
  */
static void ParkingStation_UpdatePlace(const char *placeId, uint8_t occupied, ParkingPlaceState *placeState)
{
  uint32_t now = HAL_GetTick();

  if (occupied != placeState->occupied)
  {
    placeState->occupied = occupied;

    if (occupied)
    {
      placeState->occupiedStartedAt = now;
      placeState->usedReported = 0U;
    }
    else
    {
      if (placeState->usedReported)
      {
        ParkingStation_PrintPlaceJson(placeId, PARKING_STATE_FREE);
      }

      placeState->usedReported = 0U;
      placeState->occupiedStartedAt = 0U;
    }
  }

  if (placeState->occupied &&
      !placeState->usedReported &&
      ((now - placeState->occupiedStartedAt) >= PARKING_USED_REPORT_DELAY_MS))
  {
    ParkingStation_PrintPlaceJson(placeId, PARKING_STATE_USED);
    placeState->usedReported = 1U;
  }
}

/**
  * @brief  Prints one compact JSON line for the dashboard or a serial terminal.
  */
static void ParkingStation_PrintPlaceJson(const char *placeId, const char *state)
{
  printf("{\"place\":\"%s\",\"state\":\"%s\",\"timestamp_ms\":%lu}\r\n",
         placeId,
         state,
         HAL_GetTick());
}

/**
  * @brief  Moves complete commands from the interrupt buffer into local memory.
  *
  * Interrupts are disabled only for the short copy/reset section. Parsing and
  * printing happen afterwards so the UART interrupt stays lightweight.
  */
static void ParkingStation_PollCommand(uint8_t a12Occupied, uint8_t a41Occupied)
{
  char command[PARKING_COMMAND_BUFFER_SIZE];
  uint8_t hasCommand = 0U;
  uint8_t hasOverflow = 0U;

  __disable_irq();
  if (commandReady)
  {
    strncpy(command, (const char *)commandBuffer, PARKING_COMMAND_BUFFER_SIZE);
    command[PARKING_COMMAND_BUFFER_SIZE - 1U] = '\0';
    commandReady = 0U;
    hasCommand = 1U;
  }

  if (commandOverflow)
  {
    commandOverflow = 0U;
    hasOverflow = 1U;
  }
  __enable_irq();

  if (hasOverflow)
  {
    printf("{\"error\":\"command_too_long\"}\r\n");
  }

  if (hasCommand)
  {
    ParkingStation_HandleCommand(command, a12Occupied, a41Occupied);
  }
}

/**
  * @brief  Handles the line-oriented command protocol.
  */
static void ParkingStation_HandleCommand(const char *command, uint8_t a12Occupied, uint8_t a41Occupied)
{
  if (strcmp(command, "STATUS") == 0)
  {
    ParkingStation_PrintAllPlacesJson(a12Occupied, a41Occupied);
  }
  else
  {
    printf("{\"error\":\"unknown_command\"}\r\n");
  }
}

/**
  * @brief  Sends a complete snapshot of all live hardware places.
  */
static void ParkingStation_PrintAllPlacesJson(uint8_t a12Occupied, uint8_t a41Occupied)
{
  uint32_t now = HAL_GetTick();

  printf("[{\"place\":\"A12\",\"state\":\"%s\",\"timestamp_ms\":%lu},{\"place\":\"A41\",\"state\":\"%s\",\"timestamp_ms\":%lu}]\r\n",
         a12Occupied ? PARKING_STATE_USED : PARKING_STATE_FREE,
         now,
         a41Occupied ? PARKING_STATE_USED : PARKING_STATE_FREE,
         now);
}

/**
  * @brief  Controls the per-place green LEDs.
  *
  * The LEDs show availability, not the serial event delay: ON means the place
  * is currently free. Occupied places keep their LEDs OFF.
  */
static void ParkingStation_UpdateFreeLeds(uint8_t a12Occupied, uint8_t a41Occupied)
{
  HAL_GPIO_WritePin(PARKING_PLACE_A12_LED_GPIO_Port,
                    PARKING_PLACE_A12_LED_Pin,
                    !a12Occupied ? GPIO_PIN_SET : GPIO_PIN_RESET);

  HAL_GPIO_WritePin(PARKING_PLACE_A41_LED_GPIO_Port,
                    PARKING_PLACE_A41_LED_Pin,
                    !a41Occupied ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
  * @brief  Collects received UART bytes until CR or LF terminates a command.
  *
  * The callback does not execute commands directly. It only marks a complete
  * line as ready so the main loop can handle it deterministically.
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == &hcom_uart[COM1])
  {
    if ((uartRxByte == '\r') || (uartRxByte == '\n'))
    {
      if ((commandLength > 0U) && !commandReady)
      {
        commandBuffer[commandLength] = '\0';
        commandReady = 1U;
      }

      commandLength = 0U;
    }
    else if (!commandReady)
    {
      if (commandLength < (PARKING_COMMAND_BUFFER_SIZE - 1U))
      {
        commandBuffer[commandLength] = (char)uartRxByte;
        commandLength++;
      }
      else
      {
        commandLength = 0U;
        commandOverflow = 1U;
      }
    }

    (void)HAL_UART_Receive_IT(&hcom_uart[COM1], &uartRxByte, 1U);
  }
}

/**
  * @brief  Restarts reception after a UART error and drops the partial command.
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart == &hcom_uart[COM1])
  {
    commandLength = 0U;
    (void)HAL_UART_Receive_IT(&hcom_uart[COM1], &uartRxByte, 1U);
  }
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          :
  * Description        :
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/**
  * @}
  */

/**
  * @}
  */

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
#ifdef USE_FULL_ASSERT
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
