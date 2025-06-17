/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body - Improved version with MQ2 integration
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "dht11.h"
#include "mq2.h"
#include "ssd1306.h"
#include <stdio.h>  // Để sử dụng printf (nếu có UART debug)
#include <string.h> // Để sử dụng strlen
#include "ssd1306_fonts.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DHT11_READ_INTERVAL 2000  // Đọc DHT11 mỗi 2 giây (theo datasheet)
#define OLED_UPDATE_INTERVAL 200  // Cập nhật OLED mỗi 200ms
#define MQ2_READ_INTERVAL 1000    // Đọc MQ2 mỗi 1 giây
#define UART_SEND_INTERVAL 2000   // Gửi dữ liệu qua UART mỗi 2 giây
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim4;

UART_HandleTypeDef huart5;

/* USER CODE BEGIN PV */
DHT11_Data dht11Data;
MQ2_Data mq2Data;

/* Debug variables - global để dễ theo dõi trong Live Expressions */
volatile float currentTemperature = 0.0f;
volatile float currentHumidity = 0.0f;
volatile uint8_t isChecksumValid = 0;
volatile DHT11_StatusTypeDef lastStatus = DHT11_OK;
volatile uint32_t readCount = 0;
volatile uint32_t errorCount = 0;

/* MQ2 debug variables */
volatile float currentGasValue = 0.0f;
volatile float currentLPGValue = 0.0f;
volatile float currentSmokeValue = 0.0f;
volatile MQ2_GasLevelTypeDef currentGasLevel = MQ2_LEVEL_NORMAL;
volatile MQ2_StatusTypeDef mq2Status = MQ2_OK;

/* UART variables */
uint32_t lastUartSendTime = 0;  // Biến theo dõi thời gian gửi UART
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM4_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC1_Init(void);
static void MX_UART5_Init(void);
/* USER CODE BEGIN PFP */
void DHT11_ProcessReading(uint32_t currentTime);
void OLED_ProcessUpdate(uint32_t currentTime);
void MQ2_ProcessReading(uint32_t currentTime);
void UART_SendSensorData(uint32_t currentTime);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
  * @brief  Xử lý đọc dữ liệu DHT11
  * @param  currentTime: thời gian hiện tại từ HAL_GetTick()
  * @retval None
  */
void DHT11_ProcessReading(uint32_t currentTime) {
    static uint32_t lastReadTime = 0;

    /* Kiểm tra interval 2 giây */
    if (currentTime - lastReadTime >= DHT11_READ_INTERVAL) {
        lastReadTime = currentTime;
        readCount++;

        /* Đọc dữ liệu từ DHT11 */
        DHT11_StatusTypeDef status = DHT11_ReadData(&dht11Data);
        lastStatus = status;

        if (status == DHT11_OK) {
            /* Dữ liệu hợp lệ - cập nhật variables */
            currentTemperature = dht11Data.Temperature;
            currentHumidity = dht11Data.Humidity;
            isChecksumValid = dht11Data.CheckSum_OK;
        } else {
            /* Có lỗi khi đọc */
            errorCount++;
            isChecksumValid = 0;
        }
    }
}

/**
  * @brief  Xử lý đọc dữ liệu MQ2
  * @param  currentTime: thời gian hiện tại từ HAL_GetTick()
  * @retval None
  */
void MQ2_ProcessReading(uint32_t currentTime) {
    static uint32_t lastReadTime = 0;
    static uint8_t isFirstRead = 1;

    /* Kiểm tra interval 1 giây */
    if (currentTime - lastReadTime >= MQ2_READ_INTERVAL) {
        lastReadTime = currentTime;

        /* Đọc dữ liệu từ MQ2 */
        MQ2_StatusTypeDef status = MQ2_ReadAllValues(&mq2Data);
        mq2Status = status;

        if (status == MQ2_OK) {
            /* Dữ liệu hợp lệ - cập nhật variables */
            currentGasValue = mq2Data.GasConcentration;
            currentLPGValue = mq2Data.LPGConcentration;
            currentSmokeValue = mq2Data.SmokeConcentration;
            currentGasLevel = mq2Data.Level;

            /* Lần đọc đầu tiên hoặc cần hiệu chuẩn */
            if (isFirstRead) {
                isFirstRead = 0;

                /* Hiệu chuẩn cảm biến nếu chưa được hiệu chuẩn */
                if (!mq2Data._isCalibrated) {
                    MQ2_Calibrate(&mq2Data);
                }
            }
        }
    }
}

/**
  * @brief  Cập nhật màn hình OLED
  * @param  currentTime: thời gian hiện tại từ HAL_GetTick()
  * @retval None
  */
void OLED_ProcessUpdate(uint32_t currentTime) {
    static uint32_t lastUpdateTime = 0;
    char oled_buffer[32];

    //  Kiểm tra interval 200ms
    if (currentTime - lastUpdateTime >= OLED_UPDATE_INTERVAL) {
        lastUpdateTime = currentTime;

        // Clear OLED screen
        ssd1306_Fill(Black);

        // Display Temperature - Dùng integer thay vì float
        ssd1306_SetCursor(1, 0);
        if (readCount > 1 && lastStatus == DHT11_OK) {
            // Hiển thị giá trị với integer
            int temp_whole = (int)currentTemperature;
            int temp_frac = (int)((currentTemperature - temp_whole) * 10);
            snprintf(oled_buffer, sizeof(oled_buffer), "Nhiet Do: %d.%d C", temp_whole, temp_frac);
        } else {
            if (readCount <= 1) {
                snprintf(oled_buffer, sizeof(oled_buffer), "Nhiet Do: Init...");
            } else {
                snprintf(oled_buffer, sizeof(oled_buffer), "Nhiet Do: Error");
            }
        }
        ssd1306_WriteString(oled_buffer, Font_7x10, White);

        // Display Humidity - Dùng integer thay vì float
        ssd1306_SetCursor(1, 15);
        if (readCount > 1 && lastStatus == DHT11_OK) {
            // Hiển thị giá trị với integer
            int hum_whole = (int)currentHumidity;
            int hum_frac = (int)((currentHumidity - hum_whole) * 10);
            snprintf(oled_buffer, sizeof(oled_buffer), "Do Am:  %d.%d %%", hum_whole, hum_frac);
        } else {
            if (readCount <= 1) {
                snprintf(oled_buffer, sizeof(oled_buffer), "Do Am:  Init...");
            } else {
                snprintf(oled_buffer, sizeof(oled_buffer), "Do Am:  Error");
            }
        }
        ssd1306_WriteString(oled_buffer, Font_7x10, White);

        // Display Gas Level - Thêm dòng thứ 3 cho giá trị gas
        ssd1306_SetCursor(1, 30);
        if (mq2Status == MQ2_OK) {
            int gas_whole = (int)currentGasValue;
            int gas_frac = (int)((currentGasValue - gas_whole) * 10);

            // Thêm icon hoặc marker cho mức nguy hiểm
            const char* levelMarker = "";
            if (currentGasLevel == MQ2_LEVEL_DANGER) {
                levelMarker = "! ";
            } else if (currentGasLevel == MQ2_LEVEL_WARNING) {
                levelMarker = "* ";
            }

            snprintf(oled_buffer, sizeof(oled_buffer), "%sGas:  %d.%d ppm", levelMarker, gas_whole, gas_frac);
        } else {
            snprintf(oled_buffer, sizeof(oled_buffer), "Gas:  Cal...");
        }
        ssd1306_WriteString(oled_buffer, Font_7x10, White);

        // Update OLED display
        ssd1306_UpdateScreen();
    }
}

/**
  * @brief  Gửi dữ liệu cảm biến qua UART5 đến ESP
  * @param  currentTime: thời gian hiện tại từ HAL_GetTick()
  * @retval None
  */
void UART_SendSensorData(uint32_t currentTime) {
    static uint32_t lastSendTime = 0;
    char uart_buffer[64];

    /* Gửi dữ liệu cảm biến mỗi 2 giây */
    if (currentTime - lastSendTime >= UART_SEND_INTERVAL) {
        lastSendTime = currentTime;

        /* Chỉ gửi khi đọc cảm biến thành công */
        if (lastStatus == DHT11_OK && mq2Status == MQ2_OK) {
            /* Định dạng chuỗi giống như mẫu: "DATA: TEMP=XX°C, HUMID=XX%, GAS=XXXppm" */
            int temp_whole = (int)currentTemperature;
            int temp_frac = (int)((currentTemperature - temp_whole) * 10);

            int hum_whole = (int)currentHumidity;
            int hum_frac = (int)((currentHumidity - hum_whole) * 10);

            int gas_whole = (int)currentGasValue;
            int gas_frac = (int)((currentGasValue - gas_whole) * 10);

            /* Tạo chuỗi dữ liệu */
            sprintf(uart_buffer, "DATA: TEMP=%d.%d°C, HUMID=%d.%d%%, GAS=%d.%dppm\r\n",
                    temp_whole, temp_frac,
                    hum_whole, hum_frac,
                    gas_whole, gas_frac);

            /* Gửi chuỗi qua UART5 */
            HAL_UART_Transmit(&huart5, (uint8_t*)uart_buffer, strlen(uart_buffer), HAL_MAX_DELAY);

            /* Hiển thị LED báo đã gửi (tùy chọn) */
            HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_13);  // Đèn báo UART (nếu có)
        }
    }
}
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
  MX_TIM4_Init();
  MX_I2C1_Init();
  MX_ADC1_Init();
  MX_UART5_Init();
  /* USER CODE BEGIN 2 */

  /* Initialize DHT11 with proper parameters */
  DHT11_Init(&dht11Data, GPIOA, GPIO_PIN_3, &htim4);
  HAL_TIM_Base_Start(&htim4);

  /* Initialize MQ2 with proper parameters */
  MQ2_Init(&mq2Data, &hadc1, ADC_CHANNEL_2);

  /* Initialize OLED display */
  ssd1306_Init();

  /* Initial LED states */
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_15, GPIO_PIN_RESET);  // DHT11 LED
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_RESET);  // MQ2 Alarm LED

  /* Khởi tạo UART cho giao tiếp ESP */
  HAL_UART_Transmit(&huart5, (uint8_t*)"STM32 đã khởi động với cảm biến thực\r\n", 40, 1000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint32_t currentTime = HAL_GetTick();

    /* Gọi hàm xử lý DHT11 */
    DHT11_ProcessReading(currentTime);

    /* Gọi hàm xử lý MQ2 */
    MQ2_ProcessReading(currentTime);

    /* Cập nhật OLED */
    OLED_ProcessUpdate(currentTime);

    /* Gửi dữ liệu qua UART đến ESP */
    UART_SendSensorData(currentTime);

    /* Điều khiển các LED */
    DHT11_ControlLED(&dht11Data, currentTime);
    MQ2_ControlAlarm(&mq2Data, currentTime);

    /* Delay ngắn để không làm quá tải CPU */
    HAL_Delay(50);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.ScanConvMode = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel its corresponding rank in the sequencer and its sample time.
  */
  sConfig.Channel = ADC_CHANNEL_2;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
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
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM4_Init(void)
{

  /* USER CODE BEGIN TIM4_Init 0 */

  /* USER CODE END TIM4_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM4_Init 1 */

  /* USER CODE END TIM4_Init 1 */
  htim4.Instance = TIM4;
  htim4.Init.Prescaler = 7;
  htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim4.Init.Period = 65535;
  htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM4_Init 2 */

  /* USER CODE END TIM4_Init 2 */

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : PA3 */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PD13 (UART LED) PD14 (MQ2 Alarm LED) PD15 (DHT11 LED) */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
