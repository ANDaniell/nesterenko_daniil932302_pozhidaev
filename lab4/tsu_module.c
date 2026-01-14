#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>
#include <linux/types.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("TSU student");
MODULE_DESCRIPTION("Venus Tracker in Kernel Space (Fixed Point)");
MODULE_VERSION("2.0");

/* --- МАТЕМАТИКА С ФИКСИРОВАННОЙ ТОЧКОЙ --- */
/* Масштабный коэффициент: 1.00000 = 100000 */
#define SCALE 100000
#define PI_SCALED 314159
#define TWO_PI_SCALED 628318

/* Макрос для перевода градусов в радианы (масштабированные) */
/* rad = deg * PI / 180 */
#define DEG_TO_RAD(d) ((d * 314159) / 18000000) // Временное упрощение для аргументов

/* Функция взятия остатка (аналог fmod) для углов 0-360 */
static s64 normalize_angle(s64 angle) {
    while (angle < 0) angle += 360 * SCALE;
    while (angle >= 360 * SCALE) angle -= 360 * SCALE;
    return angle;
}

/* * Аппроксимация синуса Бхаскары I (для целых чисел).
 * Вход: угол в градусах * SCALE. 
 * Точность достаточна для поиска созвездия.
 */
static s64 fp_sin(s64 angle_scaled) {
    s64 x = normalize_angle(angle_scaled) / SCALE; // Получаем градусы (0-360)
    s64 sign = 1;
    
    if (x >= 180) {
        x -= 180;
        sign = -1;
    }
    
    /* Формула: 4x(180-x) / (40500 - x(180-x)) */
    /* Результат умножаем на SCALE обратно */
    s64 num = 4 * x * (180 - x);
    s64 den = 40500 - x * (180 - x);
    
    if (den == 0) return 0; // Защита от деления на 0
    return sign * (num * SCALE) / den;
}

static s64 fp_cos(s64 angle_scaled) {
    /* cos(x) = sin(x + 90) */
    return fp_sin(angle_scaled + 90 * SCALE);
}

/* Функция арктангенса (упрощенная для квадрантов) - нужна для геоцентрического перехода */
/* Возвращает угол в градусах * SCALE */
static s64 fp_atan2(s64 y, s64 x) {
    s64 angle;
    s64 abs_y = (y >= 0) ? y : -y;
    s64 abs_x = (x >= 0) ? x : -x;
    
    if (x == 0 && y == 0) return 0;
    
    /* Простейшая линейная аппроксимация для оценки квадранта */
    /* Для точной астрономии нужен ряд Тейлора, но для созвездий хватит оценки */
    if (abs_x > abs_y) {
        // Угол мал, ~ y/x
        angle = (abs_y * 45 * SCALE) / abs_x; 
    } else {
        // Угол велик, ~ 90 - x/y
        angle = 90 * SCALE - (abs_x * 45 * SCALE) / abs_y;
    }

    if (x < 0) angle = 180 * SCALE - angle;
    if (y < 0) angle = 360 * SCALE - angle;
    
    return normalize_angle(angle);
}

/* --- АСТРОНОМИЯ --- */

/* Данные орбит (J2000) масштабированные */
/* Земля */
#define E_N (0)               /* Долгота восходящего узла */
#define E_I (0)               /* Наклонение */
#define E_W (1029373)         /* Перигелий (102.9373) */
#define E_A (100000)          /* Большая полуось (1.00000 AU) */
#define E_E (1670)            /* Эксцентриситет (0.01670) */
#define E_M (3575291)         /* Средняя аномалия на эпоху (357.5291) */

/* Венера */
#define V_N (766800)          /* (76.6800) */
#define V_I (33900)           /* (3.3900) - игнорируем для 2D проекции (упрощение) */
#define V_W (1315300)         /* (131.5300) */
#define V_A (72333)           /* (0.72333 AU) */
#define V_E (680)             /* (0.00680) */
#define V_M (502500)          /* (50.2500) */

/* Вычисление положения (возвращает эклиптическую долготу 0-360 * SCALE) */
static s64 get_venus_longitude(s64 days_since_j2000) {
    // 1. Средняя аномалия (M = M0 + n*d)
    // Движение Земли: 0.9856 град/день
    // Движение Венеры: 1.6021 град/день
    
    s64 M_earth = normalize_angle(E_M + (98560 * days_since_j2000) / 100000);
    s64 M_venus = normalize_angle(V_M + (160210 * days_since_j2000) / 100000);

    // 2. Уравнение центра (учет эллипса): v = M + 2e*sin(M)
    // E_E уже 1670 (0.0167 * SCALE), но нам нужно E_E как множитель
    // 2 * 0.0167 * sin(M) * (180/PI) для градусов ~ 1.91 * sin(M)
    
    s64 C_earth = (191 * fp_sin(M_earth)) / 100; // ~1.91 * sin
    s64 C_venus = (78 * fp_sin(M_venus)) / 100;  // ~0.78 * sin

    s64 L_earth_true = normalize_angle(M_earth + E_W + C_earth);
    s64 L_venus_true = normalize_angle(M_venus + V_W + C_venus);

    // 3. Геоцентрические координаты
    // X = r * cos(L), Y = r * sin(L)
    // r считаем равным большой полуоси (A) для простоты в этом шаге, т.к. эксцентриситет мал
    
    s64 X_earth = (E_A * fp_cos(L_earth_true)) / SCALE;
    s64 Y_earth = (E_A * fp_sin(L_earth_true)) / SCALE;

    s64 X_venus = (V_A * fp_cos(L_venus_true)) / SCALE;
    s64 Y_venus = (V_A * fp_sin(L_venus_true)) / SCALE;

    // Вектор от Земли к Венере
    s64 dx = X_venus - X_earth;
    s64 dy = Y_venus - Y_earth;

    return fp_atan2(dy, dx);
}


static int __init tsu_module_init(void)
{
    struct timespec64 now;
    s64 unix_secs;
    s64 days_j2000;
    s64 current_long, future_long;
    int day_offset = 0;
    int state = 0; // 0: ждем выхода, 1: ищем вход
    
    // Границы Водолея (Ecliptic Longitude approx): 301° - 326°
    // Это астрологические/астрономические границы с учетом прецессии на J2000
    s64 AQUA_START = 301 * SCALE;
    s64 AQUA_END = 328 * SCALE; 

    printk(KERN_INFO "TSU Astro Module: Loaded.\n");

    // Получаем время
    ktime_get_real_ts64(&now);
    unix_secs = now.tv_sec;
    // J2000 epoch is Jan 1, 2000 12:00 TT approx 946728000 unix
    days_j2000 = (unix_secs - 946728000) / 86400;

    // Проверка текущей позиции
    current_long = get_venus_longitude(days_j2000);
    printk(KERN_INFO "TSU: Current Venus Longitude: %lld.%05lld deg\n", 
           current_long / SCALE, current_long % SCALE);

    if (current_long >= AQUA_START && current_long <= AQUA_END) {
        printk(KERN_INFO "TSU: Venus is currently in the target constellation (Aquarius).\n");
        state = 0; // Сначала нужно выйти из него
    } else {
        printk(KERN_INFO "TSU: Venus is NOT in target. Searching for entry...\n");
        state = 1; // Сразу ищем вход
    }

    // Симуляция будущего (ограничим 1000 дней во избежание зависания ядра)
    for (day_offset = 1; day_offset < 1000; day_offset++) {
        future_long = get_venus_longitude(days_j2000 + day_offset);

        bool in_constellation = (future_long >= AQUA_START && future_long <= AQUA_END);

        if (state == 0) {
            // Ждем пока Венера ПОКИНЕТ созвездие
            if (!in_constellation) {
                state = 1; // Теперь ищем когда вернется
            }
        } else if (state == 1) {
            // Ищем когда Венера ВОЙДЕТ снова
            if (in_constellation) {
                 printk(KERN_ALERT "TSU: FOUND! Next entry in approx %d days.\n", day_offset);
                 // Грубая оценка даты (просто прибавляем секунды)
                 // В реальном коде нужен конвертер time_t -> Date
                 printk(KERN_ALERT "TSU: Check calendar for date: Now + %d days\n", day_offset);
                 break;
            }
        }
    }
    
    if (day_offset >= 1000) {
        printk(KERN_ERR "TSU: Calculation limit reached. Orbit too complex!\n");
    }

    return 0;
}

static void __exit tsu_module_exit(void) {
    printk(KERN_INFO "TSU module unloaded.\n");
}

module_init(tsu_module_init);
module_exit(tsu_module_exit);
