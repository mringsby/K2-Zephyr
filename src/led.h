#pragma once

#include <zephyr/drivers/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const struct gpio_dt_spec led;
void led_init(void);

#ifdef __cplusplus
}
#endif
