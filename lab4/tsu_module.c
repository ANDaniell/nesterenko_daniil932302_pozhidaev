#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/timekeeping.h>
#include <linux/ktime.h>
#include <linux/types.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nesterenko Daniil");
MODULE_DESCRIPTION("Venus Tracker in Kernel Space (Fixed Point)");
MODULE_VERSION("2.0");

#define SCALE 100000
#define PI_SCALED 314159
#define TWO_PI_SCALED 628318
#define DEG_TO_RAD(d) ((d * 314159) / 18000000)

static s64 normalize_angle(s64 angle) {
    while (angle < 0) angle += 360 * SCALE;
    while (angle >= 360 * SCALE) angle -= 360 * SCALE;
    return angle;
}

static s64 fp_sin(s64 angle_scaled) {
    s64 x = normalize_angle(angle_scaled) / SCALE;
    s64 sign = 1;
    
    if (x >= 180) {
        x -= 180;
        sign = -1;
    }
    
    s64 num = 4 * x * (180 - x);
    s64 den = 40500 - x * (180 - x);
    
    if (den == 0) return 0;
    return sign * (num * SCALE) / den;
}

static s64 fp_cos(s64 angle_scaled) {
    return fp_sin(angle_scaled + 90 * SCALE);
}

static s64 fp_atan2(s64 y, s64 x) {
    s64 angle;
    s64 abs_y = (y >= 0) ? y : -y;
    s64 abs_x = (x >= 0) ? x : -x;
    
    if (x == 0 && y == 0) return 0;
    
    if (abs_x > abs_y) {
        angle = (abs_y * 45 * SCALE) / abs_x; 
    } else {
        angle = 90 * SCALE - (abs_x * 45 * SCALE) / abs_y;
    }

    if (x < 0) angle = 180 * SCALE - angle;
    if (y < 0) angle = 360 * SCALE - angle;
    
    return normalize_angle(angle);
}

#define E_N (0)
#define E_I (0)
#define E_W (1029373)
#define E_A (100000)
#define E_E (1670)
#define E_M (3575291)

#define V_N (766800)
#define V_I (33900)
#define V_W (1315300)
#define V_A (72333)
#define V_E (680)
#define V_M (502500)

static s64 get_venus_longitude(s64 days_since_j2000) {
    s64 M_earth = normalize_angle(E_M + (98560 * days_since_j2000) / 100000);
    s64 M_venus = normalize_angle(V_M + (160210 * days_since_j2000) / 100000);

    s64 C_earth = (191 * fp_sin(M_earth)) / 100;
    s64 C_venus = (78 * fp_sin(M_venus)) / 100;

    s64 L_earth_true = normalize_angle(M_earth + E_W + C_earth);
    s64 L_venus_true = normalize_angle(M_venus + V_W + C_venus);

    s64 X_earth = (E_A * fp_cos(L_earth_true)) / SCALE;
    s64 Y_earth = (E_A * fp_sin(L_earth_true)) / SCALE;

    s64 X_venus = (V_A * fp_cos(L_venus_true)) / SCALE;
    s64 Y_venus = (V_A * fp_sin(L_venus_true)) / SCALE;

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
    int state = 0;
    
    s64 AQUA_START = 301 * SCALE;
    s64 AQUA_END = 328 * SCALE; 

    printk(KERN_INFO "TSU Astro Module: Loaded.\n");

    ktime_get_real_ts64(&now);
    unix_secs = now.tv_sec;
    days_j2000 = (unix_secs - 946728000) / 86400;

    current_long = get_venus_longitude(days_j2000);
    printk(KERN_INFO "TSU: Current Venus Longitude: %lld.%05lld deg\n", 
           current_long / SCALE, current_long % SCALE);

    if (current_long >= AQUA_START && current_long <= AQUA_END) {
        state = 0;
    } else {
        state = 1;
    }

    for (day_offset = 1; day_offset < 1000; day_offset++) {
        future_long = get_venus_longitude(days_j2000 + day_offset);

        bool in_constellation = (future_long >= AQUA_START && future_long <= AQUA_END);

        if (state == 0) {
            if (!in_constellation) {
                state = 1;
            }
        } else if (state == 1) {
            if (in_constellation) {
                 printk(KERN_ALERT "TSU: FOUND! Next entry in approx %d days.\n", day_offset);
                 printk(KERN_ALERT "TSU: Check calendar for date: Now + %d days\n", day_offset);
                 break;
            }
        }
    }
    
    if (day_offset >= 1000) {
        printk(KERN_ERR "TSU: Calculation limit reached.\n");
    }

    return 0;
}

static void __exit tsu_module_exit(void) {
    printk(KERN_INFO "TSU module unloaded.\n");
}

module_init(tsu_module_init);
module_exit(tsu_module_exit);
