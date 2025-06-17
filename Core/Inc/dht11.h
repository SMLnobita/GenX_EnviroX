/**
  ******************************************************************************
  * @file           : dht11.h
  * @brief          : Header cho DHT11 driver - Improved version
  * @created        : May 14, 2025
  * @author         : NguyenHoa
  * @version        : 2.0.0
  ******************************************************************************
  */

#ifndef INC_DHT11_H_
#define INC_DHT11_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Version defines -----------------------------------------------------------*/
#define DHT11_VER_MAJOR 2
#define DHT11_VER_MINOR 0
#define DHT11_VER_PATCH 0

/* Exported types ------------------------------------------------------------*/
typedef enum {
    DHT11_OK = 0,
    DHT11_TIMEOUT,
    DHT11_ERROR,
    DHT11_CHECKSUM_MISMATCH,
    DHT11_INIT_ERROR
} DHT11_StatusTypeDef;

typedef struct {
    float Temperature;          // Temperature in Celsius
    float Humidity;             // Humidity in %
    DHT11_StatusTypeDef Status; // Last operation status
    uint8_t CheckSum_OK;        // Checksum verification result
    // Private members
    GPIO_TypeDef *_GPIOx;
    uint16_t _Pin;
    TIM_HandleTypeDef *_Tim;
} DHT11_Data;

/* Exported constants --------------------------------------------------------*/
#define DHT11_PORT GPIOA
#define DHT11_PIN GPIO_PIN_3       // Chọn chân PA3 để đọc DHT11
#define DHT11_LED_PORT GPIOD
#define DHT11_LED_PIN GPIO_PIN_15  // Đèn báo trạng thái
#define DHT11_TIMEOUT 150          // Timeout tối đa cho mỗi bit (μs)

/* Exported functions prototypes ---------------------------------------------*/
// Initialization and cleanup
void DHT11_Init(DHT11_Data *dht11, GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, TIM_HandleTypeDef *htim);
void DHT11_DeInit(DHT11_Data *dht11);

// Data reading functions
DHT11_StatusTypeDef DHT11_ReadData(DHT11_Data *data);
float DHT11_ReadTemperatureC(DHT11_Data *data);
float DHT11_ReadTemperatureF(DHT11_Data *data);
float DHT11_ReadHumidity(DHT11_Data *data);

// Status and control functions
const char* DHT11_GetErrorMsg(DHT11_StatusTypeDef status);
void DHT11_ControlLED(DHT11_Data *data, uint32_t currentTime);

/* Private declares ----------------------------------------------------------*/
extern uint32_t lastBlinkTime;

#ifdef __cplusplus
}
#endif

#endif /* INC_DHT11_H_ */
