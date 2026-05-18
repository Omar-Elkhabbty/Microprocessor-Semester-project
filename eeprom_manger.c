/*
 * eeprom_manager.c
 */

#include "eeprom_manger.h"

// جلب معرف الـ I2C الرئيسي من ملف main.c تلقائياً
extern I2C_HandleTypeDef hi2c3;

// مصفوفة الـ RAM المركزية والعداد (مخفيين داخل ملف الـ c لحماية البيانات static)
static TemperatureSample samples[MAX_SAMPLES];
static uint16_t sample_count = 0;


/**
 * @brief  دالة داخلية (خاصة بالمكتبة) تقرأ عداد العينات مباشرة من العنوان 0x00 في الـ EEPROM.
 * @return العداد الفعلي المخزن. (نوع البيانات: uint16_t).
 */
/*static uint16_t Internal_GetCount(void) {
    uint16_t count = 0;
    // قراءة بايتين من العنوان 0x00 لقراءة قيمة العداد
    HAL_I2C_Mem_Read(&hi2c3, EEPROM_I2C_ADDR, COUNT_ADDR, EEPROM_ADDR_SIZE, (uint8_t*)&count, 2, HAL_MAX_DELAY);
    
    // حماية: إذا كانت الذاكرة جديدة كلياً، ستكون قيمتها الافتراضية الفيزيائية هي 0xFFFF
    if (count == 0xFFFF) {
        count = 0;
    }
    return count;
}*/


static uint16_t Internal_GetCount(void) {
    uint16_t count = 0;
    
    // نقوم بحفظ حالة الاتصال في متغير
    HAL_StatusTypeDef status = HAL_I2C_Mem_Read(&hi2c3, EEPROM_I2C_ADDR, COUNT_ADDR, EEPROM_ADDR_SIZE, (uint8_t*)&count, 2, HAL_MAX_DELAY);
    
    // إذا فشل الاتصال الفيزيائي بالكامل، لا نغير شيئاً ونخرج لحماية البيانات
    if (status != HAL_OK) {
        return 0; 
    }
    
    if (count == 0xFFFF) {
        count = 0;
    }
    return count;
}

// تنفيذ دالة التهيئة عند الإقلاع
void EEPROM_Manager_Init(void) {
    sample_count = Internal_GetCount();
    
    // حماية لضمان عدم تخطي حجم المصفوفة المحجوزة في الـ RAM
    if (sample_count > MAX_SAMPLES) sample_count = MAX_SAMPLES;
    
    // سحب العينات عينة تلو الأخرى من الـ EEPROM وضخها في مصفوفة الـ RAM
    for (uint16_t i = 0; i < sample_count; i++) {
        uint16_t read_addr = DATA_START_ADDR + (i * sizeof(TemperatureSample));
        HAL_I2C_Mem_Read(&hi2c3, EEPROM_I2C_ADDR, read_addr, EEPROM_ADDR_SIZE, (uint8_t*)&samples[i], sizeof(TemperatureSample), HAL_MAX_DELAY);
    }
}

// تنفيذ دالة إضافة عينة جديدة
HAL_StatusTypeDef EEPROM_Manager_AddSample(float temp, uint32_t timestamp) {
    HAL_StatusTypeDef status;
    
    // ميكانيزم الـ Circular Buffer: إذا امتلأت المصفوفة نبدأ الكتابة من الصفر فوق البيانات القديمة
    if (sample_count >= MAX_SAMPLES) {
        sample_count = 0; 
    }
    
    // 1. التحديث الفوري في مصفوفة الـ RAM بسرعة النانو ثانية
    samples[sample_count].temperature_value = temp;
    samples[sample_count].timestamp = timestamp;
    
    // 2. حساب العنوان الفيزيائي والكتابة بايت تلو الآخر في الـ EEPROM لضمان استقرار الصفحات
    uint16_t write_addr = DATA_START_ADDR + (sample_count * sizeof(TemperatureSample));
    uint8_t *data_ptr = (uint8_t*)&samples[sample_count];
    
    for (uint16_t i = 0; i < sizeof(TemperatureSample); i++) {
        status = HAL_I2C_Mem_Write(&hi2c3, EEPROM_I2C_ADDR, write_addr + i, EEPROM_ADDR_SIZE, &data_ptr[i], 1, HAL_MAX_DELAY);
        if (status != HAL_OK) return status; // إرجاع خطأ فوراً إذا فشل السلك الفيزيائي
        HAL_Delay(5); // الانتظار الفيزيائي الإلزامي لصفحة الـ EEPROM
    }
    
    // 3. زيادة العداد برمجياً وحفظ القيمة الجديدة في أول عنوان بالذاكرة
    sample_count++;
    status = HAL_I2C_Mem_Write(&hi2c3, EEPROM_I2C_ADDR, COUNT_ADDR, EEPROM_ADDR_SIZE, (uint8_t*)&sample_count, 2, HAL_MAX_DELAY);
    HAL_Delay(5);
    
    return status;
}

// تنفيذ دالة معرفة العدد الحالي
uint16_t EEPROM_Manager_GetCount(void) {
    return sample_count;
}

// تنفيذ دالة تصفير الذاكرة (الزر 5)
void EEPROM_Manager_ClearAll(void) {
    uint16_t zero = 0;
    // كتابة 0 في خانة العداد الرئيسي بالـ EEPROM
    HAL_I2C_Mem_Write(&hi2c3, EEPROM_I2C_ADDR, COUNT_ADDR, EEPROM_ADDR_SIZE, (uint8_t*)&zero, 2, HAL_MAX_DELAY);
    HAL_Delay(5);
    
    // تصفير العداد في الـ RAM
    sample_count = 0;
}

// تنفيذ دالة البحث عن أعلى قيمة مطلقة (الزر 1)
TemperatureSample EEPROM_Manager_GetAbsoluteMax(void) {
    // فرض قيمة ابتدائية منخفضة جداً للمقارنة
    TemperatureSample max_sample = { .temperature_value = -100.0f, .timestamp = 0 };
    
    if (sample_count == 0) return max_sample; // إذا كانت المصفوفة فارغة نعود بأوبجكت صفري
    
    // عمل اللوب البرمجي السريع داخل الـ RAM
    for (uint16_t i = 0; i < sample_count; i++) {
        if (samples[i].temperature_value > max_sample.temperature_value) {
            max_sample = samples[i]; // الاحتفاظ بالأوبجكت الأكبر
        }
    }
    return max_sample;
}

// تنفيذ دالة البحث عن أقل قيمة مطلقة (الزر 2)
TemperatureSample EEPROM_Manager_GetAbsoluteMin(void) {
    // فرض قيمة ابتدائية مرتفعة جداً للمقارنة
    TemperatureSample min_sample = { .temperature_value = 200.0f, .timestamp = 0 };
    
    if (sample_count == 0) return min_sample;
    
    for (uint16_t i = 0; i < sample_count; i++) {
        if (samples[i].temperature_value < min_sample.temperature_value) {
            min_sample = samples[i]; // الاحتفاظ بالأوبجكت الأصغر
        }
    }
    return min_sample;
}

/**
 * @brief  دالة رياضية داخلية مخفية لحساب نطاق بداية ونهاية "يوم أمس" بالثواني بناءً على الوقت الحالي.
 */
static void GetYesterdayRange(uint32_t current_time, uint32_t *start_out, uint32_t *end_out) {
    // تصفير الساعات والدقائق الحالية للوصول لـ 12:00 AM الخاصة باليوم الحالي
    uint32_t start_of_today = current_time - (current_time % SECONDS_IN_DAY);
    // طرح يوم كامل بالثواني للحصول على بداية الأمس
    *start_out = start_of_today - SECONDS_IN_DAY;
    // نهاية الأمس هي قبل بداية اليوم الحالي بثانية واحدة
    *end_out = start_of_today - 1;
}

// تنفيذ دالة أعلى قيمة ليوم أمس (الزر 3)
TemperatureSample EEPROM_Manager_GetYesterdayMax(uint32_t current_time) {
    TemperatureSample max_sample = { .temperature_value = -100.0f, .timestamp = 0 };
    uint32_t start_yesterday, end_yesterday;
    
    // جلب النطاق الزمني الدقيق ليوم أمس بالثواني
    GetYesterdayRange(current_time, &start_yesterday, &end_yesterday);
    
    for (uint16_t i = 0; i < sample_count; i++) {
        // بوابة الفلترة: التحقق من أن العينة كتبت داخل نطاق الـ 24 ساعة الخاصة بأمس
        if (samples[i].timestamp >= start_yesterday && samples[i].timestamp <= end_yesterday) {
            if (samples[i].temperature_value > max_sample.temperature_value) {
                max_sample = samples[i];
            }
        }
    }
    return max_sample;
}

// تنفيذ دالة أقل قيمة ليوم أمس (الزر 4)
TemperatureSample EEPROM_Manager_GetYesterdayMin(uint32_t current_time) {
    TemperatureSample min_sample = { .temperature_value = 200.0f, .timestamp = 0 };
    uint32_t start_yesterday, end_yesterday;
    
    GetYesterdayRange(current_time, &start_yesterday, &end_yesterday);
    
    for (uint16_t i = 0; i < sample_count; i++) {
        // بوابة الفلترة الزمنية
        if (samples[i].timestamp >= start_yesterday && samples[i].timestamp <= end_yesterday) {
            if (samples[i].temperature_value < min_sample.temperature_value) {
                min_sample = samples[i];
            }
        }
    }
    return min_sample;
}

// تنفيذ دالة طباعة المصفوفة كاملة عبر السيريال
void EEPROM_Manager_PrintAll(void) {
    printf("\r\n--- START OF RAM ARRAY SAMPLES (Count: %d) ---\r\n", sample_count);
    for (uint16_t i = 0; i < sample_count; i++) {
        // طباعة منسقة: عرض رقم الـ Index، ثم الحرارة برقمين بعد العلامة العشرية، ثم الوقت الصافي
        printf("Index [%d] -> Temp: %.2f C | Timestamp: %lu\r\n", i, samples[i].temperature_value, samples[i].timestamp);
    }
    printf("--- END OF RAM ARRAY SAMPLES ---\r\n");
}