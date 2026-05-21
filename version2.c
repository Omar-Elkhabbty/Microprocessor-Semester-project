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

#include "i2c-lcd.h"             // Driver for the I2C LCD 16x2 screen
#include "stdio.h"              // Standard C library for string handling (e.g., sprintf)
#include "time-stamp.h"        // Custom library to retrieve Unix Epoch Timestamps from the RTC
#include "eeprom_manger.h"    // Custom library for Hybrid RAM/EEPROM Data Logging Management

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
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c3;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

// --- Non-blocking Timer Variables (SysTick) ---
uint32_t last_sensor_tick = 0;

// --- UART Communication Variables ---
uint8_t rx_byte = 0;

// --- Sensor 1 Data Variables (Channel 0 - Pin PA0) ---
uint32_t adc_value_ch0 = 0;
float voltage_ch0 = 0.0f;
float tempK_ch0 = 0.0f;
float tempC_ch0 = 0.0f;

// --- Sensor 2 Data Variables (Channel 1 - Pin PA1) ---
uint32_t adc_value_ch1 = 0;
float voltage_ch1 = 0.0f;
float tempK_ch1 = 0.0f;
float tempC_ch1 = 0.0f;

// --- Structures to hold Analytical/Statistical Logs ---
TemperatureSample abs_max_record;
TemperatureSample abs_min_record;
TemperatureSample yest_max_record;
TemperatureSample yest_min_record;

// --- String Buffers for LCD screen output ---
char buffer_1[20];
char buffer_2[20];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C3_Init(void);
static void MX_RTC_Init(void);
static void MX_USART2_UART_Init(void);
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
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_I2C3_Init();
  MX_RTC_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
	
  // --- Initialize LCD Screen ---
  lcd_init();
  lcd_clear();
	
  lcd_put_cur(0, 0);
  lcd_send_string("System Ready");
	
  // --- Initialize EEPROM Manager (Restores previous logs from EEPROM to RAM) ---
  EEPROM_Manager_Init();
 
  // --- Set baseline timestamp for non-blocking timer ---
  last_sensor_tick = HAL_GetTick();
	
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		
    /* ----------------------------------------------------------------- */
    /* Non-blocking Timer Window: Samples data every 15 seconds          */
    /* ----------------------------------------------------------------- */
    if (HAL_GetTick() - last_sensor_tick >= 15000)
    {
        ADC_ChannelConfTypeDef sConfig = {0};

        // =================================================================
        // 1. Read Sensor 1 (ADC_CHANNEL_0 connected to PA0)
        // =================================================================
        sConfig.Channel = ADC_CHANNEL_0;                // Select channel 0
        sConfig.Rank = 1;                               // Set first in conversion rank
        sConfig.SamplingTime = ADC_SAMPLETIME_15CYCLES; // Internal capacitor charging time
        HAL_ADC_ConfigChannel(&hadc1, &sConfig);        // Apply configuration to Multiplexer
        
        HAL_ADC_Start(&hadc1);                          // Enable ADC peripheral
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) 
        {
            adc_value_ch0 = HAL_ADC_GetValue(&hadc1);   // Retrieve raw 12-bit digital value (0-4095)
            
            // Mathematical formulas converting raw ADC data to Celsius
            voltage_ch0 = (adc_value_ch0 * 3.3f) / 4095.0f;
            tempK_ch0 = voltage_ch0 * 100.0f;
            tempC_ch0 = tempK_ch0 - 273.15f;
        }
        HAL_ADC_Stop(&hadc1);                           // Stop conversion and free data register

        // =================================================================
        // 2. Read Sensor 2 (ADC_CHANNEL_1 connected to PA1)
        // =================================================================
        sConfig.Channel = ADC_CHANNEL_1;                // Switch internal multiplexer to channel 1
        HAL_ADC_ConfigChannel(&hadc1, &sConfig);        // Apply the new channel config
        
        HAL_ADC_Start(&hadc1);                          // Restart ADC for channel 1
        if (HAL_ADC_PollForConversion(&hadc1, 10) == HAL_OK) 
        {
            adc_value_ch1 = HAL_ADC_GetValue(&hadc1);   // Retrieve raw 12-bit data for channel 1
            
            // Mathematical formulas for the second channel
            voltage_ch1 = (adc_value_ch1 * 3.3f) / 4095.0f;
            tempK_ch1 = voltage_ch1 * 100.0f;
            tempC_ch1 = tempK_ch1 - 273.15f;
        }
        HAL_ADC_Stop(&hadc1);                          // Stop ADC after channel 1 conversion

        // =================================================================
        // 3. Data Storage & Management
        // =================================================================
        uint32_t current_time = get_timestamp();        // Fetch current Unix Timestamp from RTC
        
        // Save the primary sensor logs into the hybrid storage tracking architecture
        EEPROM_Manager_AddSample(tempC_ch0, tempC_ch0, current_time);
        
        // Note: tempC_ch1 can now be displayed on the LCD or transmitted over UART concurrently.

        last_sensor_tick = HAL_GetTick();              // Reset baseline timer for the next interval
    }
		
    // =====================================================================
    // Blue button "PC13" Event Gate: Extracts Absolute MAX and MIN
    // =====================================================================
    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_SET) {
			
        HAL_Delay(50); // Software debouncing delay
			
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET) {
				
            // Run instantaneous analysis within RAM array
            abs_max_record = EEPROM_Manager_GetAbsoluteMax();
            abs_min_record = EEPROM_Manager_GetAbsoluteMin();
            
            lcd_clear();
					
            if (abs_max_record.timestamp != 0)
            {
                sprintf(buffer_1, "Abs Max: %.1f C", abs_max_record.temperature_value);
                lcd_put_cur(0, 0);
                lcd_send_string(buffer_1);
							
                sprintf(buffer_2, "Abs Min: %.1f C", abs_min_record.temperature_value);
                lcd_put_cur(1, 0);
                lcd_send_string(buffer_2);
							
            } else {
                lcd_put_cur(0, 0);
                lcd_send_string("No data found");
            }
            
            // Optional: execution lock until button is released
            // while(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13) == GPIO_PIN_RESET);
        }
    }
		
    // =====================================================================
    // Black button "PC0" Event Gate: Extracts Yesterday's MAX and MIN
    // =====================================================================
    if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_PIN_SET)
    {
        HAL_Delay(50); // Software debouncing delay
        if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_PIN_RESET)
        {
            uint32_t now_time = get_timestamp(); // Fetch active baseline timeline
            
            // Filter array data across yesterday's 24-hour window boundaries
            yest_max_record = EEPROM_Manager_GetYesterdayMax(now_time);
            yest_min_record = EEPROM_Manager_GetYesterdayMin(now_time);
            
            lcd_clear();
            if (yest_max_record.timestamp != 0)
            {
                sprintf(buffer_1, "YestMax: %.1f C", yest_max_record.temperature_value);
                lcd_put_cur(0, 0);
                lcd_send_string(buffer_1);
                
                sprintf(buffer_2, "YestMin: %.1f C", yest_min_record.temperature_value);
                lcd_put_cur(1, 0);
                lcd_send_string(buffer_2);
            }
            else
            {
                lcd_put_cur(0, 0);
                lcd_send_string("No D yesterday");
            }
            
            // Optional: release lock
            // while(HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_0) == GPIO_PIN_RESET);
        }
    }
		
    // =====================================================================
    // UART CLI Parser Gate: Command Line Interface parsing section
    // =====================================================================
    static char command_char = 0; // Static variable to preserve pending command symbol

    if (HAL_UART_Receive(&huart2, &rx_byte, 1, 0) == HAL_OK)
    {
        // 1. If byte received is an alphanumeric character (not Enter control key)
        if (rx_byte != '\r' && rx_byte != '\n')
        {
            // Echo Mode: Mirror the character back to terminal interface
            HAL_UART_Transmit(&huart2, &rx_byte, 1, HAL_MAX_DELAY);
            command_char = rx_byte; // Cache the character (e.g., 'r' or 'c')
        }
        // 2. If byte received corresponds to Enter terminator control sequence (\r or \n)
        else if (rx_byte == '\r' || rx_byte == '\n')
        {
            // Only parse if an active cached character is ready to evaluate
            if (command_char == 'c' || command_char == 'C')
            {
                EEPROM_Manager_ClearAll(); // Clear log pointer in EEPROM/RAM
                HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n[SUCCESS] Memory Cleared!\r\n", 28, HAL_MAX_DELAY);
                
                lcd_clear();
                lcd_put_cur(0, 0);
                lcd_send_string("Memory Cleared");
                
                command_char = 0; // Reset state machine upon completion
            }
            else if (command_char == 'r' || command_char == 'R')
            {
                HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n[INFO] Printing All Data...\r\n", 31, HAL_MAX_DELAY);
                
                EEPROM_Manager_PrintAll(); // Stream data table over serial interface
                
                command_char = 0; // Reset state machine upon completion
            }
            else if (command_char != 0)
            {
                // Fallback for unrecognized control commands
                HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n[ERROR] Unknown Command!\r\n", 28, HAL_MAX_DELAY);
                command_char = 0; 
            }
            
            // Safety Note: If command_char is already 0, the second Enter sequence character (\n) 
            // passes silently through here, preventing command trigger duplication.
        }
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
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
  hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
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
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = 1;
  sConfig.SamplingTime = ADC_SAMPLETIME_3CYCLES;
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
  hi2c1.Init.ClockSpeed = 100000;
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
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.ClockSpeed = 100000;
  hi2c3.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 1;
  sTime.Minutes = 10;
  sTime.Seconds = 0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_MAY;
  sDate.Date = 18;
  sDate.Year = 26;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
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
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC0 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/**
  * @brief  Retargets standard library printf/fputc output to UART2 peripheral
  */
int fputc(int ch, FILE *f)
{
    // Transmit one byte synchronously over UART2 line
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
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
  * where the assert_param error has occurred.
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