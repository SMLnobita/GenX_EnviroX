/**
  ******************************************************************************
  * @file           : mq2.h
  * @brief          : Header cho MQ2 gas sensor driver
  * @created        : May 18, 2025
  * @author         : NguyenHoa
  * @version        : 1.0.0
  ******************************************************************************
  */

#ifndef INC_MQ2_H_
#define INC_MQ2_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Version defines -----------------------------------------------------------*/
#define MQ2_VER_MAJOR 1
#define MQ2_VER_MINOR 0
#define MQ2_VER_PATCH 0

/* Exported types ------------------------------------------------------------*/
typedef enum {
    MQ2_OK = 0,
    MQ2_ERROR,
    MQ2_ADC_TIMEOUT,
    MQ2_CALIBRATION_ERROR
} MQ2_StatusTypeDef;

typedef enum {
    MQ2_LEVEL_NORMAL = 0,  // Mức độ an toàn
    MQ2_LEVEL_WARNING,     // Mức độ cảnh báo
    MQ2_LEVEL_DANGER       // Mức độ nguy hiểm
} MQ2_GasLevelTypeDef;

typedef struct {
    float RawValue;              // Giá trị ADC thô (0-4095)
    float Voltage;               // Điện áp (0-3.3V)
    float GasConcentration;      // Nồng độ khí gas (ppm)
    float SmokeConcentration;    // Nồng độ khói (ppm)
    float LPGConcentration;      // Nồng độ LPG (ppm)
    MQ2_GasLevelTypeDef Level;   // Mức độ báo động
    MQ2_StatusTypeDef Status;    // Trạng thái đọc cuối cùng
    // Private members
    ADC_HandleTypeDef *_hadc;    // Handle của ADC
    uint32_t _channel;           // Kênh ADC
    float _R0;                   // Giá trị điện trở cảm biến trong không khí sạch
    uint8_t _isCalibrated;       // Trạng thái hiệu chuẩn
} MQ2_Data;

/* Exported constants --------------------------------------------------------*/
#define MQ2_ADC_PORT       GPIOA
#define MQ2_ADC_PIN        GPIO_PIN_0      // Chọn chân PA0 để đọc ADC từ MQ2
#define MQ2_ADC_CHANNEL    ADC_CHANNEL_0   // Kênh ADC tương ứng
#define MQ2_ALARM_PORT     GPIOD
#define MQ2_ALARM_PIN      GPIO_PIN_14     // Đèn báo cảnh báo khí gas
#define MQ2_ADC_TIMEOUT    100             // Timeout cho ADC (ms)

#define MQ2_WARNING_THRESHOLD  300         // Ngưỡng cảnh báo (ppm)
#define MQ2_DANGER_THRESHOLD   700         // Ngưỡng nguy hiểm (ppm)
#define MQ2_RL_VALUE           5.0f        // Giá trị điện trở tải (kΩ)
#define MQ2_CALIB_SAMPLES      10          // Số mẫu cho hiệu chuẩn
#define MQ2_CLEAN_AIR_RATIO    9.83f       // Rs/R0 trong không khí sạch

/* Exported functions prototypes ---------------------------------------------*/
// Initialization and cleanup
void MQ2_Init(MQ2_Data *mq2, ADC_HandleTypeDef *hadc, uint32_t channel);
void MQ2_DeInit(MQ2_Data *mq2);

// Calibration
MQ2_StatusTypeDef MQ2_Calibrate(MQ2_Data *mq2);
void MQ2_SetR0(MQ2_Data *mq2, float r0_value);

// Reading functions
MQ2_StatusTypeDef MQ2_ReadRaw(MQ2_Data *mq2);
MQ2_StatusTypeDef MQ2_ReadVoltage(MQ2_Data *mq2);
MQ2_StatusTypeDef MQ2_ReadGasConcentration(MQ2_Data *mq2);
MQ2_StatusTypeDef MQ2_ReadAllValues(MQ2_Data *mq2);

// Gas type specific readings
float MQ2_GetSmokeConcentration(MQ2_Data *mq2);
float MQ2_GetLPGConcentration(MQ2_Data *mq2);

// Status and control functions
MQ2_GasLevelTypeDef MQ2_GetGasLevel(MQ2_Data *mq2);
const char* MQ2_GetStatusMessage(MQ2_StatusTypeDef status);
const char* MQ2_GetLevelMessage(MQ2_GasLevelTypeDef level);
void MQ2_ControlAlarm(MQ2_Data *mq2, uint32_t currentTime);

/* Private declares ----------------------------------------------------------*/
extern uint32_t lastAlarmBlinkTime;

#ifdef __cplusplus
}
#endif

#endif /* INC_MQ2_H_ */
