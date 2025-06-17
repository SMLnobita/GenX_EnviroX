/**
  ******************************************************************************
  * @file           : dht11.c
  * @brief          : DHT11 driver implementation - Improved version
  * @created        : May 14, 2025
  * @author         : NguyenHoa
  * @version        : 2.0.0
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "dht11.h"
#include <stdbool.h>

/* Private defines -----------------------------------------------------------*/
#define DHT11_PIN_OUTPUT 0
#define DHT11_PIN_INPUT 1
#define DHT11_MAX_DATA_BITS 40
#define DHT11_MAX_BYTE_PACKETS 5
#define DHT11_MAX_TIMEOUT 100
#define DHT11_BIT_THRESHOLD 50  // 50us làm ngưỡng phân biệt bit 0 và 1

/* Private variables ---------------------------------------------------------*/
uint32_t lastBlinkTime = 0;

/* Error messages array ------------------------------------------------------*/
const char* const ErrorMsg[] = {
    "OK",
    "TIMEOUT",
    "ERROR",
    "CHECKSUM MISMATCH",
    "INIT ERROR"
};

/* Private function prototypes -----------------------------------------------*/
static void DHT11_DelayUs(TIM_HandleTypeDef *tim, uint16_t us);
static bool DHT11_ObserveState(DHT11_Data *dht11, uint8_t FinalState);
static void DHT11_SetPinMode(DHT11_Data *dht11, uint8_t MODE);
static DHT11_StatusTypeDef DHT11_Start(DHT11_Data *dht11);
static DHT11_StatusTypeDef DHT11_ReadBits(DHT11_Data *dht11, uint8_t *packets);
static uint8_t DHT11_CheckSum_Verify(uint8_t *packets);

/* Public Functions ----------------------------------------------------------*/

/**
  * @brief  Khởi tạo DHT11
  * @param  dht11: con trỏ đến cấu trúc DHT11_Data
  * @param  GPIOx: GPIO port (GPIOA, GPIOB, etc.)
  * @param  GPIO_Pin: GPIO pin (GPIO_PIN_0, GPIO_PIN_1, etc.)
  * @param  htim: con trỏ đến timer handle
  * @retval None
  */
void DHT11_Init(DHT11_Data *dht11, GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin, TIM_HandleTypeDef *htim) {
    if (!dht11 || !GPIOx || !htim) {
        return;
    }

    // Khởi tạo các thành phần
    dht11->_GPIOx = GPIOx;
    dht11->_Pin = GPIO_Pin;
    dht11->_Tim = htim;
    dht11->Temperature = 0.0f;
    dht11->Humidity = 0.0f;
    dht11->Status = DHT11_OK;
    dht11->CheckSum_OK = 0;

    // Khởi tạo biến lastBlinkTime
    lastBlinkTime = HAL_GetTick();

    // Đảm bảo LED tắt khi khởi động
    HAL_GPIO_WritePin(DHT11_LED_PORT, DHT11_LED_PIN, GPIO_PIN_RESET);

    // Khởi động timer
    HAL_TIM_Base_Start(dht11->_Tim);
}

/**
  * @brief  Dọn dẹp DHT11
  * @param  dht11: con trỏ đến cấu trúc DHT11_Data
  * @retval None
  */
void DHT11_DeInit(DHT11_Data *dht11) {
    if (!dht11) return;

    HAL_GPIO_DeInit(dht11->_GPIOx, dht11->_Pin);
    HAL_TIM_Base_Stop(dht11->_Tim);
    HAL_GPIO_WritePin(DHT11_LED_PORT, DHT11_LED_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  Đọc dữ liệu từ DHT11
  * @param  data: con trỏ đến cấu trúc dữ liệu DHT11_Data
  * @retval DHT11_StatusTypeDef: trạng thái đọc dữ liệu
  */
DHT11_StatusTypeDef DHT11_ReadData(DHT11_Data *data) {
    if (!data) return DHT11_ERROR;

    uint8_t packets[DHT11_MAX_BYTE_PACKETS] = {0};
    DHT11_StatusTypeDef status;

    // Bắt đầu giao tiếp với DHT11
    status = DHT11_Start(data);
    if (status != DHT11_OK) {
        data->Status = status;
        data->CheckSum_OK = 0;
        return status;
    }

    // Đọc 40 bits dữ liệu
    status = DHT11_ReadBits(data, packets);
    if (status != DHT11_OK) {
        data->Status = status;
        data->CheckSum_OK = 0;
        return status;
    }

    // Kiểm tra checksum
    data->CheckSum_OK = DHT11_CheckSum_Verify(packets);
    if (!data->CheckSum_OK) {
        data->Status = DHT11_CHECKSUM_MISMATCH;
        return DHT11_CHECKSUM_MISMATCH;
    }

    // Chuyển đổi dữ liệu sang giá trị thực
    data->Humidity = packets[0] + (packets[1] * 0.1f);
    data->Temperature = packets[2] + (packets[3] * 0.1f);
    data->Status = DHT11_OK;

    return DHT11_OK;
}

/**
  * @brief  Đọc nhiệt độ theo độ C
  * @param  data: con trỏ đến cấu trúc DHT11_Data
  * @retval float: giá trị nhiệt độ (°C)
  */
float DHT11_ReadTemperatureC(DHT11_Data *data) {
    DHT11_ReadData(data);
    return data->Temperature;
}

/**
  * @brief  Đọc nhiệt độ theo độ F
  * @param  data: con trỏ đến cấu trúc DHT11_Data
  * @retval float: giá trị nhiệt độ (°F)
  */
float DHT11_ReadTemperatureF(DHT11_Data *data) {
    float tempC = DHT11_ReadTemperatureC(data);
    return (tempC * 1.8f) + 32.0f;
}

/**
  * @brief  Đọc độ ẩm
  * @param  data: con trỏ đến cấu trúc DHT11_Data
  * @retval float: giá trị độ ẩm (%)
  */
float DHT11_ReadHumidity(DHT11_Data *data) {
    DHT11_ReadData(data);
    return data->Humidity;
}

/**
  * @brief  Lấy thông báo lỗi
  * @param  status: mã lỗi
  * @retval const char*: chuỗi thông báo lỗi
  */
const char* DHT11_GetErrorMsg(DHT11_StatusTypeDef status) {
    if (status >= sizeof(ErrorMsg)/sizeof(ErrorMsg[0])) {
        return "UNKNOWN ERROR";
    }
    return ErrorMsg[status];
}

/**
  * @brief  Điều khiển LED dựa trên trạng thái checksum
  * @param  data: con trỏ đến cấu trúc dữ liệu DHT11_Data
  * @param  currentTime: thời gian hiện tại từ HAL_GetTick()
  * @retval None
  */
void DHT11_ControlLED(DHT11_Data *data, uint32_t currentTime) {
    if (!data) return;

    if (data->CheckSum_OK && data->Status == DHT11_OK) {
        // Checksum đúng -> LED sáng liên tục
        HAL_GPIO_WritePin(DHT11_LED_PORT, DHT11_LED_PIN, GPIO_PIN_SET);
    } else {
        // Có lỗi -> LED nhấp nháy với chu kỳ 200ms
        if (currentTime - lastBlinkTime >= 200) {
            HAL_GPIO_TogglePin(DHT11_LED_PORT, DHT11_LED_PIN);
            lastBlinkTime = currentTime;
        }
    }
}

/* Private Functions ---------------------------------------------------------*/

/**
  * @brief  Tạo độ trễ microsecond chính xác
  * @param  tim: con trỏ timer
  * @param  us: thời gian trễ (microsecond)
  * @retval None
  */
static void DHT11_DelayUs(TIM_HandleTypeDef *tim, uint16_t us) {
    __HAL_TIM_SET_COUNTER(tim, 0);
    while(__HAL_TIM_GET_COUNTER(tim) < us);
}

/**
  * @brief  Quan sát trạng thái chân GPIO với timeout
  * @param  dht11: con trỏ đến cấu trúc DHT11_Data
  * @param  FinalState: trạng thái cần đợi (0 hoặc 1)
  * @retval bool: true nếu thành công, false nếu timeout
  */
static bool DHT11_ObserveState(DHT11_Data *dht11, uint8_t FinalState) {
    __HAL_TIM_SET_COUNTER(dht11->_Tim, 0);
    while(__HAL_TIM_GET_COUNTER(dht11->_Tim) < DHT11_MAX_TIMEOUT) {
        if(HAL_GPIO_ReadPin(dht11->_GPIOx, dht11->_Pin) == FinalState) return true;
    }
    return false;
}

/**
  * @brief  Cấu hình chế độ chân GPIO
  * @param  dht11: con trỏ đến cấu trúc DHT11_Data
  * @param  MODE: DHT11_PIN_OUTPUT hoặc DHT11_PIN_INPUT
  * @retval None
  */
static void DHT11_SetPinMode(DHT11_Data *dht11, uint8_t MODE) {
    GPIO_InitTypeDef GPIO_InitStruct = {
        .Mode = MODE ? GPIO_MODE_INPUT : GPIO_MODE_OUTPUT_PP,
        .Pin = dht11->_Pin,
        .Pull = GPIO_NOPULL,
        .Speed = GPIO_SPEED_FREQ_LOW
    };
    HAL_GPIO_Init(dht11->_GPIOx, &GPIO_InitStruct);
}

/**
  * @brief  Khởi động quá trình giao tiếp với DHT11
  * @param  dht11: con trỏ đến cấu trúc DHT11_Data
  * @retval DHT11_StatusTypeDef: trạng thái khởi động
  */
static DHT11_StatusTypeDef DHT11_Start(DHT11_Data *dht11) {
    // Cấu hình chân thành output và gửi tín hiệu khởi động
    DHT11_SetPinMode(dht11, DHT11_PIN_OUTPUT);
    HAL_GPIO_WritePin(dht11->_GPIOx, dht11->_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);  // Kéo xuống LOW trong 20ms
    HAL_GPIO_WritePin(dht11->_GPIOx, dht11->_Pin, GPIO_PIN_SET);
    DHT11_DelayUs(dht11->_Tim, 40);  // Kéo lên HIGH trong 40us

    // Chuyển sang chế độ input và disable interrupt
    __disable_irq();
    DHT11_SetPinMode(dht11, DHT11_PIN_INPUT);

    // Kiểm tra phản hồi từ DHT11
    if(HAL_GPIO_ReadPin(dht11->_GPIOx, dht11->_Pin) == GPIO_PIN_SET) {
        __enable_irq();
        return DHT11_ERROR;
    }

    // Đợi DHT11 kéo lên HIGH
    if(!DHT11_ObserveState(dht11, GPIO_PIN_SET)) {
        __enable_irq();
        return DHT11_TIMEOUT;
    }

    // Đợi DHT11 kéo xuống LOW (kết thúc handshake)
    if(!DHT11_ObserveState(dht11, GPIO_PIN_RESET)) {
        __enable_irq();
        return DHT11_TIMEOUT;
    }

    return DHT11_OK;
}

/**
  * @brief  Đọc 40 bits dữ liệu từ DHT11
  * @param  dht11: con trỏ đến cấu trúc DHT11_Data
  * @param  packets: mảng lưu dữ liệu đọc được
  * @retval DHT11_StatusTypeDef: trạng thái đọc
  */
static DHT11_StatusTypeDef DHT11_ReadBits(DHT11_Data *dht11, uint8_t *packets) {
    uint8_t bits = 0;
    uint8_t packetIndex = 0;

    while(bits < DHT11_MAX_DATA_BITS) {
        // Đợi DHT11 kéo lên HIGH để bắt đầu truyền bit
        if(!DHT11_ObserveState(dht11, GPIO_PIN_SET)) {
            __enable_irq();
            return DHT11_TIMEOUT;
        }

        // Đếm thời gian HIGH để phân biệt bit 0/1
        // 28us = bit 0, 70us = bit 1
        __HAL_TIM_SET_COUNTER(dht11->_Tim, 0);
        while(HAL_GPIO_ReadPin(dht11->_GPIOx, dht11->_Pin) == GPIO_PIN_SET) {
            if(__HAL_TIM_GET_COUNTER(dht11->_Tim) > DHT11_MAX_TIMEOUT) {
                __enable_irq();
                return DHT11_TIMEOUT;
            }
        }

        // Lưu bit vào packet
        packets[packetIndex] = packets[packetIndex] << 1;
        packets[packetIndex] |= (__HAL_TIM_GET_COUNTER(dht11->_Tim) > DHT11_BIT_THRESHOLD);

        bits++;
        if(!(bits % 8)) packetIndex++;
    }

    __enable_irq();
    return DHT11_OK;
}

/**
  * @brief  Kiểm tra tính đúng đắn của checksum
  * @param  packets: mảng dữ liệu 5 bytes
  * @retval uint8_t: 1 nếu checksum đúng, 0 nếu sai
  */
static uint8_t DHT11_CheckSum_Verify(uint8_t *packets) {
    uint8_t sum = packets[0] + packets[1] + packets[2] + packets[3];
    return (sum == packets[4]) ? 1 : 0;
}
