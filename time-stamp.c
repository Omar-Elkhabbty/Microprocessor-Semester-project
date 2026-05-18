#include "time-stamp.h"

/* الإشارة إلى متغير الـ RTC المعرف في الـ main */
extern RTC_HandleTypeDef hrtc;

/**
 * @brief دالة الحصول على الـ Timestamp
 */
uint32_t get_timestamp(void) {
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};
    struct tm t = {0};

    // 1. قراءة الوقت ثم التاريخ (الترتيب إجباري في STM32)
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    // 2. تجهيز البيانات للتحويل
    t.tm_sec  = sTime.Seconds;
    t.tm_min  = sTime.Minutes;
    t.tm_hour = sTime.Hours;
    t.tm_mday = sDate.Date;
    t.tm_mon  = sDate.Month - 1;   // الأشهر تبدأ من 0 في مكتبة <time.h>
    t.tm_year = sDate.Year + 100;  // السنوات تُحسب منذ عام 1900

    // 3. تحويل الوقت وإرجاعه بالثواني
    return (uint32_t)mktime(&t);
}