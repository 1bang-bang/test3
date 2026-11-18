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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "OLED.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t  run_state = 1;      // 0=暂停 1=运行
volatile uint8_t  flow_dir  = 0;      // 0=正向 1=反向
volatile uint16_t step_ms   =300;    // 切换间隔
uint8_t  current_led = 0;
const uint16_t LED_PINS[3] = {GPIO_PIN_0, GPIO_PIN_3, GPIO_PIN_5};

volatile uint8_t  key_dir_flag = 0, key_pause_flag = 0;
volatile uint32_t key_dir_tick = 0, key_pause_tick = 0;
volatile uint8_t  need_oled = 1, need_feedback = 1;

uint8_t rx_byte, rx_buf[6];
volatile uint8_t rx_len = 0, packet_ready = 0;
uint32_t last_tick = 0;

typedef struct {
  uint8_t header, run, dir, step_l, step_h, check;
} LedPacket_t;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_UART_Receive_IT(&huart1, &rx_byte, 1);   // 开串口接收中断

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);    // OLED 供电 VCC
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);  // OLED 供电 GND
  HAL_Delay(100);
  OLED_Init();

  HAL_GPIO_WritePin(GPIOA, LED_PINS[0], GPIO_PIN_RESET); // 点亮第1颗
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* 按键消抖处理 */
    if (key_dir_flag && HAL_GetTick() - key_dir_tick > 30) {
      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET)
      { flow_dir ^= 1; need_oled = 1; need_feedback = 1; }
      key_dir_flag = 0;
    }
    if (key_pause_flag && HAL_GetTick() - key_pause_tick > 30) {
      if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET)
      { run_state ^= 1; need_oled = 1; need_feedback = 1; }
      key_pause_flag = 0;
    }

    /* 串口数据包解析 */
    if (packet_ready) {
      LedPacket_t *p = (LedPacket_t *)rx_buf;
      uint8_t sum = p->header + p->run + p->dir + p->step_l + p->step_h;
      if (p->header == 0xAA && sum == p->check) {
        run_state = p->run;
        flow_dir  = p->dir;
        uint16_t s = p->step_l | (p->step_h << 8);
        if (s >= 100 && s <= 2000) step_ms = s;
        need_oled = 1; need_feedback = 1;
      }
      packet_ready = 0;
    }

    /* 流水灯 */
    if (run_state && HAL_GetTick() - last_tick >= step_ms) {
      last_tick = HAL_GetTick();
      HAL_GPIO_WritePin(GPIOA, LED_PINS[current_led], GPIO_PIN_SET);
      if (flow_dir == 0) current_led = (current_led + 1) % 3;
      else               current_led = (current_led == 0) ? 2 : current_led - 1;
      HAL_GPIO_WritePin(GPIOA, LED_PINS[current_led], GPIO_PIN_RESET);
      need_oled = 1;
    }

    /* 串口反馈 */
    if (need_feedback && huart1.gState == HAL_UART_STATE_READY) {
      char msg[64];
      int n = snprintf(msg, sizeof(msg), "STATE=%s,DIR=%s,STEP=%u,LED=%d\r\n",
                       run_state ? "RUN" : "PAUSE", flow_dir ? "REV" : "FWD",
                       step_ms, current_led + 1);
      HAL_UART_Transmit_IT(&huart1, (uint8_t *)msg, n);
      need_feedback = 0;
    }

    /* OLED 刷新 */
    if (need_oled) {
      OLED_Clear();
      OLED_ShowString(1, 1, run_state ? "STATE: RUN" : "STATE: PAUSE");
      OLED_ShowString(2, 1, flow_dir ? "DIR: REV" : "DIR: FWD");
      OLED_ShowString(3, 1, "STEP:"); OLED_ShowNum(3, 6, step_ms, 4); OLED_ShowString(3, 10, "ms");
      OLED_ShowString(4, 1, "LED:");  OLED_ShowNum(4, 5, current_led + 1, 1);
      need_oled = 0;
    }
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == GPIO_PIN_12)      { key_dir_flag = 1;   key_dir_tick = HAL_GetTick(); }
  else if (GPIO_Pin == GPIO_PIN_13) { key_pause_flag = 1; key_pause_tick = HAL_GetTick(); }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1) {
    if (rx_len == 0 && rx_byte != 0xAA) { /* 等帧头 */ }
    else {
      rx_buf[rx_len++] = rx_byte;
      if (rx_len >= 6) { packet_ready = 1; rx_len = 0; }
    }
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
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
