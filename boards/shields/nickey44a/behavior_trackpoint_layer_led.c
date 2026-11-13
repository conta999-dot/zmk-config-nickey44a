/*
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_trackpoint_layer_led

#include <zephyr/device.h>
#include <zephyr/drivers/led.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

#define LED_GPIO_NODE_ID DT_COMPAT_GET_ANY_STATUS_OKAY(gpio_leds)

#if DT_NODE_EXISTS(DT_ALIAS(led_red))
static const struct device *const led_dev = DEVICE_DT_GET(LED_GPIO_NODE_ID);
static const uint8_t rgb_idx[] = {DT_NODE_CHILD_IDX(DT_ALIAS(led_red)),
                                  DT_NODE_CHILD_IDX(DT_ALIAS(led_green)),
                                  DT_NODE_CHILD_IDX(DT_ALIAS(led_blue))};

/* param1: 0 = off, 1 = red, 2 = blue */
static void set_led_color(uint8_t color) {
    if (!device_is_ready(led_dev)) {
        return;
    }

    led_off(led_dev, rgb_idx[0]);
    led_off(led_dev, rgb_idx[1]);
    led_off(led_dev, rgb_idx[2]);

    switch (color) {
    case 1:
        led_on(led_dev, rgb_idx[0]);
        break;
    case 2:
        led_on(led_dev, rgb_idx[2]);
        break;
    default:
        break;
    }
}
#else
static void set_led_color(uint8_t color) {
    ARG_UNUSED(color);
}
#endif

static int tpl_led_binding_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    set_led_color((uint8_t)binding->param1);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int tpl_led_binding_released(struct zmk_behavior_binding *binding,
                                    struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api tpl_led_driver_api = {
    .binding_pressed = tpl_led_binding_pressed,
    .binding_released = tpl_led_binding_released,
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
};

#if DT_NODE_HAS_STATUS(DT_NODELABEL(tpl_led), okay)
BEHAVIOR_DT_DEFINE(DT_NODELABEL(tpl_led), NULL, NULL, NULL, NULL, POST_KERNEL,
                   CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tpl_led_driver_api);
#endif

#endif
