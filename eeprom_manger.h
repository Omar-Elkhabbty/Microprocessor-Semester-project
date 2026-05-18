/*
 * eeprom_manager.h
 * مكتبة إدارة مَسجّل بيانات الحرارة عبر الـ EEPROM والـ RAM الهجينة
 */

#ifndef EEPROM_MANAGER_H
#define EEPROM_MANAGER_H

#include "stm32f4xx_hal.h"
#include <stdio.h>

// --- الإعدادات العامة للذاكرة ---
#define EEPROM_I2C_ADDR       0xA0                  // العنوان الفيزيائي الافتراضي لموديول الـ EEPROM
#define EEPROM_ADDR_SIZE      I2C_MEMADD_SIZE_8BIT   // حجم عنوان الذاكرة الداخلي (اجعله 16BIT للذواكر الكبيرة مثل 24C256)
#define MAX_SAMPLES           500                    // الحد الأقصى للعينات التي يمكن تخزينها في المصفوفة

#define COUNT_ADDR            0x00                  // عنوان بايتات العداد في الميموري
#define DATA_START_ADDR       0x04                  // عنوان بداية رص البيانات في الميموري
#define SECONDS_IN_DAY        86400                 // عدد الثواني في اليوم الواحد (يستخدم لفلترة يوم أمس)

/**
 * @brief هيكل البيانات (Struct) المخصص لحفظ العينة الواحدة
 */
typedef struct {
    float temperature_value;   // قيمة درجة الحرارة (حجمها 4 بايت - نوع كسر عشري Float)
    uint32_t timestamp;        // الطابع الزمني بالثواني Unix Timestamp (حجمها 4 بايت - نوع رقم صحيح غير سالب uint32_t)
} TemperatureSample;


// ============================================================================
// --- الدوال الجاهزة للاستخدام البرمجي ---
// ============================================================================

/**
 * @brief  دالة تهيئة النظام وإقلاع الذاكرة.
 * @note   تقوم بقراءة الـ EEPROM ونقل جميع البيانات المخزنة سابقاً إلى الـ RAM فور تشغيل الجهاز.
 * @param  لا يوجد (Void).
 * @return لا يوجد (Void).
 */
void EEPROM_Manager_Init(void);

/**
 * @brief  دالة إضافة عينة جديدة وحفظها.
 * @note   تقوم بحفظ العينة في مصفوفة الـ RAM وفي خلايا الـ EEPROM فيزيائياً في نفس الوقت وتزيد العداد تلقائياً.
 * @param  temp: قيمة درجة الحرارة المراد حفظها. (نوع البيانات: float - كسر عشري).
 * @param  timestamp: الوقت الحالي بالثواني المأخوذ من الـ RTC. (نوع البيانات: uint32_t - رقم صحيح).
 * @return حالة العملية (نوع البيانات: HAL_StatusTypeDef). 
 *         ترجع HAL_OK في حال نجاح الكتابة والاتصال، أو HAL_ERROR/HAL_BUSY في حال حدوث مشكلة.
 */
HAL_StatusTypeDef EEPROM_Manager_AddSample(float temp, uint32_t timestamp);

/**
 * @brief  دالة مسح كافة البيانات وتصفير النظام.
 * @note   تقوم بتصفير عداد العينات في الـ EEPROM والـ RAM مما يجعل النظام يرى الذاكرة فارغة تماماً.
 * @param  لا يوجد (Void).
 * @return لا يوجد (Void).
 */
void EEPROM_Manager_ClearAll(void);

/**
 * @brief  دالة جلب عدد العينات المخزنة حالياً في النظام.
 * @param  لا يوجد (Void).
 * @return عدد العينات الحالي المعالج في المصفوفة. (نوع البيانات: uint16_t - رقم صحيح بين 0 و 65535).
 */
uint16_t EEPROM_Manager_GetCount(void);

/**
 * @brief  دالة طباعة كافة عناصر المصفوفة عبر السيريال بورت (UART).
 * @note   تستخدم لمراقبة البيانات المخزنة من الكمبيوتر عبر برامج التيرمينال.
 * @param  لا يوجد (Void).
 * @return لا يوجد (Void).
 */
void EEPROM_Manager_PrintAll(void);

/**
 * @brief  [الزر 1] دالة استخراج أعلى درجة حرارة على الإطلاق.
 * @note   تقوم بعمل لوح كامل على مصفوفة الـ RAM للبحث عن أكبر قيمة حرارة سُجلت.
 * @param  لا يوجد (Void).
 * @return أوبجكت كامل يحتوي على (الحرارة القصوى ووقتها). (نوع البيانات المرتجعة: TemperatureSample).
 */
TemperatureSample EEPROM_Manager_GetAbsoluteMax(void);

/**
 * @brief  [الزر 2] دالة استخراج أقل درجة حرارة على الإطلاق.
 * @note   تقوم بعمل لوح كامل على مصفوفة الـ RAM للبحث عن أصغر قيمة حرارة سُجلت.
 * @param  لا يوجد (Void).
 * @return أوبجكت كامل يحتوي على (الحرارة الدنيا ووقتها). (نوع البيانات المرتجعة: TemperatureSample).
 */
TemperatureSample EEPROM_Manager_GetAbsoluteMin(void);

/**
 * @brief  [الزر 3] دالة استخراج أعلى درجة حرارة حدثت في فتره "اليوم السابق" فقط.
 * @note   تحسب نطاق الـ 24 ساعة الخاصة بيوم أمس وتفلتر عناصر المصفوفة بناءً عليها.
 * @param  current_time: الوقت الحالي بالثواني (Unix Timestamp) ليتم الحساب بناءً عليه. (نوع البيانات: uint32_t).
 * @return أوبجكت العينة الأكبر ليوم أمس. (نوع البيانات المرتجعة: TemperatureSample).
 *         ملاحظة: إذا كانت قيمة الـ timestamp المرتجعة تساوي 0، فهذا يعني عدم وجود عينات في نطاق يوم أمس.
 */
TemperatureSample EEPROM_Manager_GetYesterdayMax(uint32_t current_time);

/**
 * @brief  [الزر 4] دالة استخراج أقل درجة حرارة حدثت في فتره "اليوم السابق" فقط.
 * @note   تحسب نطاق الـ 24 ساعة الخاصة بيوم أمس وتفلتر عناصر المصفوفة لتبحث عن الأقل.
 * @param  current_time: الوقت الحالي بالثواني (Unix Timestamp). (نوع البيانات: uint32_t).
 * @return أوبجكت العينة الأقل ليوم أمس. (نوع البيانات المرتجعة: TemperatureSample).
 */
TemperatureSample EEPROM_Manager_GetYesterdayMin(uint32_t current_time);

#endif /* EEPROM_MANAGER_H */