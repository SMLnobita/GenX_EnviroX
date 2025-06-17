/**
  ******************************************************************************
  * @file           : mq2.c
  * @brief          : MQ2 gas sensor driver implementation
  * @created        : May 18, 2025
  * @author         : NguyenHoa
  * @version        : 1.0.0
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "mq2.h"
#include <math.h>

/* Private defines -----------------------------------------------------------*/
#define MQ2_ADC_RESOLUTION  4096.0f    // 12-bit ADC resolution
#define MQ2_VREF            3.3f       // Reference voltage
#define MQ2_BLINK_INTERVAL  500        // Alarm blink interval (ms)
#define MQ2_RAPID_BLINK     200        // Rapid blink for danger level (ms)

/* Private variables ---------------------------------------------------------*/
uint32_t lastAlarmBlinkTime = 0;

/* Status and level messages arrays ------------------------------------------*/
const char* const StatusMsg[] = {
    "OK",
    "ERROR",
    "ADC TIMEOUT",
    "CALIBRATION ERROR"
};

const char* const LevelMsg[] = {
    "NORMAL",
    "WARNING",
    "DANGER"
};

/* Private function prototypes -----------------------------------------------*/
static float MQ2_CalculateResistance(float adc_value);
static float MQ2_CalculateRatio(float rs_value, float r0_value);
static float MQ2_CalculatePPM(float rs_ro_ratio, float curve_a, float curve_b);

/* Public Functions ----------------------------------------------------------*/

/**
  * @brief  Khởi tạo MQ2
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @param  hadc: con trỏ đến ADC handle
  * @param  channel: kênh ADC
  * @retval None
  */
void MQ2_Init(MQ2_Data *mq2, ADC_HandleTypeDef *hadc, uint32_t channel) {
    if (!mq2 || !hadc) {
        return;
    }

    // Khởi tạo các thành phần
    mq2->_hadc = hadc;
    mq2->_channel = channel;
    mq2->RawValue = 0.0f;
    mq2->Voltage = 0.0f;
    mq2->GasConcentration = 0.0f;
    mq2->SmokeConcentration = 0.0f;
    mq2->LPGConcentration = 0.0f;
    mq2->Level = MQ2_LEVEL_NORMAL;
    mq2->Status = MQ2_OK;
    mq2->_R0 = 10.0f;  // Giá trị mặc định, nên hiệu chuẩn
    mq2->_isCalibrated = 0;

    // Khởi tạo LED báo động
    HAL_GPIO_WritePin(MQ2_ALARM_PORT, MQ2_ALARM_PIN, GPIO_PIN_RESET);
    lastAlarmBlinkTime = HAL_GetTick();
}

/**
  * @brief  Dọn dẹp MQ2
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval None
  */
void MQ2_DeInit(MQ2_Data *mq2) {
    if (!mq2) return;

    HAL_GPIO_WritePin(MQ2_ALARM_PORT, MQ2_ALARM_PIN, GPIO_PIN_RESET);
}

/**
  * @brief  Hiệu chuẩn cảm biến MQ2 trong không khí sạch
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval MQ2_StatusTypeDef: trạng thái hiệu chuẩn
  * @note   Nên gọi hàm này khi cảm biến đã được làm nóng (2-3 phút)
  *         và đặt trong môi trường không khí sạch
  */
MQ2_StatusTypeDef MQ2_Calibrate(MQ2_Data *mq2) {
    if (!mq2) return MQ2_ERROR;

    float rs_sum = 0.0f;
    uint8_t valid_samples = 0;

    // Lấy nhiều mẫu để tăng độ chính xác
    for (uint8_t i = 0; i < MQ2_CALIB_SAMPLES; i++) {
        if (MQ2_ReadRaw(mq2) == MQ2_OK) {
            float rs = MQ2_CalculateResistance(mq2->RawValue);
            if (rs > 0.0f) { // Tránh giá trị không hợp lệ
                rs_sum += rs;
                valid_samples++;
            }
        }
        HAL_Delay(100); // Chờ giữa các mẫu
    }

    // Kiểm tra lỗi
    if (valid_samples < MQ2_CALIB_SAMPLES/2) {
        mq2->Status = MQ2_CALIBRATION_ERROR;
        return MQ2_CALIBRATION_ERROR;
    }

    // Tính giá trị trung bình
    float rs_avg = rs_sum / valid_samples;

    // R0 = Rs / 9.83 (trong không khí sạch)
    mq2->_R0 = rs_avg / MQ2_CLEAN_AIR_RATIO;
    mq2->_isCalibrated = 1;
    mq2->Status = MQ2_OK;

    return MQ2_OK;
}

/**
  * @brief  Thiết lập giá trị R0 từ bên ngoài
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @param  r0_value: giá trị R0 (kΩ)
  * @retval None
  */
void MQ2_SetR0(MQ2_Data *mq2, float r0_value) {
    if (!mq2 || r0_value <= 0.0f) return;

    mq2->_R0 = r0_value;
    mq2->_isCalibrated = 1;
}

/**
  * @brief  Đọc giá trị ADC thô từ cảm biến MQ2
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval MQ2_StatusTypeDef: trạng thái đọc
  */
MQ2_StatusTypeDef MQ2_ReadRaw(MQ2_Data *mq2) {
    if (!mq2 || !mq2->_hadc) return MQ2_ERROR;

    // Cấu hình ADC channel
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = mq2->_channel;
    sConfig.Rank = 1;
    sConfig.SamplingTime = ADC_SAMPLETIME_480CYCLES;
    if (HAL_ADC_ConfigChannel(mq2->_hadc, &sConfig) != HAL_OK) {
        mq2->Status = MQ2_ERROR;
        return MQ2_ERROR;
    }

    // Bắt đầu chuyển đổi ADC
    HAL_ADC_Start(mq2->_hadc);

    // Đợi chuyển đổi hoàn thành
    if (HAL_ADC_PollForConversion(mq2->_hadc, MQ2_ADC_TIMEOUT) != HAL_OK) {
        mq2->Status = MQ2_ADC_TIMEOUT;
        return MQ2_ADC_TIMEOUT;
    }

    // Đọc giá trị
    uint32_t adc_value = HAL_ADC_GetValue(mq2->_hadc);
    mq2->RawValue = (float)adc_value;

    // Dừng ADC
    HAL_ADC_Stop(mq2->_hadc);

    mq2->Status = MQ2_OK;
    return MQ2_OK;
}

/**
  * @brief  Đọc điện áp từ cảm biến MQ2
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval MQ2_StatusTypeDef: trạng thái đọc
  */
MQ2_StatusTypeDef MQ2_ReadVoltage(MQ2_Data *mq2) {
    MQ2_StatusTypeDef status = MQ2_ReadRaw(mq2);
    if (status != MQ2_OK) return status;

    // Tính điện áp
    mq2->Voltage = (mq2->RawValue / MQ2_ADC_RESOLUTION) * MQ2_VREF;

    return MQ2_OK;
}

/**
  * @brief  Đọc nồng độ khí gas từ cảm biến MQ2
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval MQ2_StatusTypeDef: trạng thái đọc
  */
MQ2_StatusTypeDef MQ2_ReadGasConcentration(MQ2_Data *mq2) {
    if (!mq2->_isCalibrated) {
        // Nếu chưa hiệu chuẩn, thử hiệu chuẩn tự động
        if (MQ2_Calibrate(mq2) != MQ2_OK) {
            return MQ2_CALIBRATION_ERROR;
        }
    }

    // Đọc điện áp
    MQ2_StatusTypeDef status = MQ2_ReadVoltage(mq2);
    if (status != MQ2_OK) return status;

    // Tính điện trở của cảm biến (Rs)
    float rs = MQ2_CalculateResistance(mq2->RawValue);

    // Tính tỷ lệ Rs/R0
    float rs_ro_ratio = MQ2_CalculateRatio(rs, mq2->_R0);

    // Các hệ số đường cong cho khí gas tổng hợp (từ datasheet)
    // Công thức: ppm = a * (Rs/R0)^b
    float curve_a = 658.31f;
    float curve_b = -2.07f;

    mq2->GasConcentration = MQ2_CalculatePPM(rs_ro_ratio, curve_a, curve_b);

    // Tính nồng độ cho các loại khí cụ thể
    // Hệ số cho khói
    float smoke_a = 776.56f;
    float smoke_b = -2.23f;
    mq2->SmokeConcentration = MQ2_CalculatePPM(rs_ro_ratio, smoke_a, smoke_b);

    // Hệ số cho LPG
    float lpg_a = 591.87f;
    float lpg_b = -1.95f;
    mq2->LPGConcentration = MQ2_CalculatePPM(rs_ro_ratio, lpg_a, lpg_b);

    // Xác định mức độ
    mq2->Level = MQ2_GetGasLevel(mq2);

    return MQ2_OK;
}

/**
  * @brief  Đọc tất cả các giá trị từ cảm biến MQ2
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval MQ2_StatusTypeDef: trạng thái đọc
  */
MQ2_StatusTypeDef MQ2_ReadAllValues(MQ2_Data *mq2) {
    return MQ2_ReadGasConcentration(mq2);
}

/**
  * @brief  Lấy nồng độ khói
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval float: giá trị nồng độ khói (ppm)
  */
float MQ2_GetSmokeConcentration(MQ2_Data *mq2) {
    if (!mq2) return 0.0f;
    MQ2_ReadGasConcentration(mq2);
    return mq2->SmokeConcentration;
}

/**
  * @brief  Lấy nồng độ LPG
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval float: giá trị nồng độ LPG (ppm)
  */
float MQ2_GetLPGConcentration(MQ2_Data *mq2) {
    if (!mq2) return 0.0f;
    MQ2_ReadGasConcentration(mq2);
    return mq2->LPGConcentration;
}

/**
  * @brief  Lấy mức độ cảnh báo khí gas
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @retval MQ2_GasLevelTypeDef: mức độ cảnh báo
  */
MQ2_GasLevelTypeDef MQ2_GetGasLevel(MQ2_Data *mq2) {
    if (!mq2) return MQ2_LEVEL_NORMAL;

    if (mq2->GasConcentration >= MQ2_DANGER_THRESHOLD) {
        return MQ2_LEVEL_DANGER;
    } else if (mq2->GasConcentration >= MQ2_WARNING_THRESHOLD) {
        return MQ2_LEVEL_WARNING;
    } else {
        return MQ2_LEVEL_NORMAL;
    }
}

/**
  * @brief  Lấy thông báo trạng thái
  * @param  status: trạng thái
  * @retval const char*: chuỗi thông báo trạng thái
  */
const char* MQ2_GetStatusMessage(MQ2_StatusTypeDef status) {
    if (status >= sizeof(StatusMsg)/sizeof(StatusMsg[0])) {
        return "UNKNOWN STATUS";
    }
    return StatusMsg[status];
}

/**
  * @brief  Lấy thông báo mức độ
  * @param  level: mức độ cảnh báo
  * @retval const char*: chuỗi thông báo mức độ
  */
const char* MQ2_GetLevelMessage(MQ2_GasLevelTypeDef level) {
    if (level >= sizeof(LevelMsg)/sizeof(LevelMsg[0])) {
        return "UNKNOWN LEVEL";
    }
    return LevelMsg[level];
}

/**
  * @brief  Điều khiển LED báo động dựa trên mức độ khí gas
  * @param  mq2: con trỏ đến cấu trúc MQ2_Data
  * @param  currentTime: thời gian hiện tại từ HAL_GetTick()
  * @retval None
  */
void MQ2_ControlAlarm(MQ2_Data *mq2, uint32_t currentTime) {
    if (!mq2) return;

    switch (mq2->Level) {
        case MQ2_LEVEL_NORMAL:
            // Mức bình thường -> LED tắt
            HAL_GPIO_WritePin(MQ2_ALARM_PORT, MQ2_ALARM_PIN, GPIO_PIN_RESET);
            break;

        case MQ2_LEVEL_WARNING:
            // Mức cảnh báo -> LED nhấp nháy chậm (500ms)
            if (currentTime - lastAlarmBlinkTime >= MQ2_BLINK_INTERVAL) {
                HAL_GPIO_TogglePin(MQ2_ALARM_PORT, MQ2_ALARM_PIN);
                lastAlarmBlinkTime = currentTime;
            }
            break;

        case MQ2_LEVEL_DANGER:
            // Mức nguy hiểm -> LED nhấp nháy nhanh (200ms)
            if (currentTime - lastAlarmBlinkTime >= MQ2_RAPID_BLINK) {
                HAL_GPIO_TogglePin(MQ2_ALARM_PORT, MQ2_ALARM_PIN);
                lastAlarmBlinkTime = currentTime;
            }
            break;

        default:
            // Mặc định -> LED tắt
            HAL_GPIO_WritePin(MQ2_ALARM_PORT, MQ2_ALARM_PIN, GPIO_PIN_RESET);
            break;
    }
}

/* Private Functions ---------------------------------------------------------*/

/**
  * @brief  Tính toán điện trở cảm biến (Rs)
  * @param  adc_value: giá trị ADC đọc được
  * @retval float: giá trị điện trở (kΩ)
  */
static float MQ2_CalculateResistance(float adc_value) {
    // Tránh chia cho 0
    if (adc_value >= MQ2_ADC_RESOLUTION - 1) {
        return 0.0f;
    }

    // Công thức: Rs = RL * (Vin - Vout) / Vout
    // Vout = adc_value * Vref / ADC_resolution
    // Vin = Vref
    float vout = (adc_value / MQ2_ADC_RESOLUTION) * MQ2_VREF;

    // Tránh chia cho 0
    if (vout < 0.1f) {
        return 999999.0f; // Giá trị lớn để biểu thị điện trở rất cao
    }

    return MQ2_RL_VALUE * ((MQ2_VREF - vout) / vout);
}

/**
  * @brief  Tính toán tỷ lệ Rs/R0
  * @param  rs_value: giá trị điện trở cảm biến
  * @param  r0_value: giá trị điện trở chuẩn
  * @retval float: tỷ lệ Rs/R0
  */
static float MQ2_CalculateRatio(float rs_value, float r0_value) {
    // Tránh chia cho 0
    if (r0_value < 0.1f) {
        return 0.0f;
    }

    return rs_value / r0_value;
}

/**
  * @brief  Tính toán nồng độ khí gas (ppm)
  * @param  rs_ro_ratio: tỷ lệ Rs/R0
  * @param  curve_a: hằng số a của đường cong
  * @param  curve_b: hằng số b của đường cong
  * @retval float: nồng độ khí gas (ppm)
  * @note   Công thức: ppm = a * (Rs/R0)^b
  */
static float MQ2_CalculatePPM(float rs_ro_ratio, float curve_a, float curve_b) {
    // Tránh giá trị không hợp lệ
    if (rs_ro_ratio <= 0.0f) {
        return 0.0f;
    }

    // Áp dụng công thức từ datasheet
    return curve_a * powf(rs_ro_ratio, curve_b);
}
