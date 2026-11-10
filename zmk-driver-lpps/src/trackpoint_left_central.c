/*
 * SPDX-License-Identifier: MIT
 *
 * Central: drop left split TrackPoint input when disabled; push runtime N to left;
 * Layer-5 shortcuts via &tp_left_ctl (toggle / N+ / N-).
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/led.h>
#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>

#include <drivers/behavior.h>
#include <drivers/input_processor.h>

#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/input.h>
#include <zmk/split/central.h>
#include <zmk/split/bluetooth/peripheral.h>

#if IS_ENABLED(CONFIG_RGBLED_WIDGET)
#include <zmk_rgbled_widget/widget.h>
#endif

#define TP_LEFT_CTL_TOGGLE 0
#define TP_LEFT_CTL_N_UP   1
#define TP_LEFT_CTL_N_DOWN 2

#define TP_LEFT_N_MIN 1
#define TP_LEFT_N_MAX 127
#define TP_LEFT_N_DEFAULT 20

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

static bool left_tp_enabled = true;
static int left_tp_accum_n = TP_LEFT_N_DEFAULT;

static struct k_work_delayable led_restore_work;

#if DT_HAS_COMPAT_STATUS_OKAY(DT_NODELABEL(tpl_led))
static const char *const tpl_led_name = DEVICE_DT_NAME(DT_NODELABEL(tpl_led));
#endif

#if DT_HAS_COMPAT_STATUS_OKAY(DT_NODELABEL(lpps_set_accum))
static const char *const lpps_set_accum_name = DEVICE_DT_NAME(DT_NODELABEL(lpps_set_accum));
#endif

#if DT_NODE_EXISTS(DT_ALIAS(led_red))
static const struct device *const central_led_dev = DEVICE_DT_GET(DT_COMPAT_GET_ANY_STATUS_OKAY(gpio_leds));
static const uint8_t central_rgb_idx[] = {DT_NODE_CHILD_IDX(DT_ALIAS(led_red)),
                                          DT_NODE_CHILD_IDX(DT_ALIAS(led_green)),
                                          DT_NODE_CHILD_IDX(DT_ALIAS(led_blue))};

static void central_set_led_color(uint8_t color) {
    if (!device_is_ready(central_led_dev)) {
        return;
    }
    led_off(central_led_dev, central_rgb_idx[0]);
    led_off(central_led_dev, central_rgb_idx[1]);
    led_off(central_led_dev, central_rgb_idx[2]);
    if (color == 1) {
        led_on(central_led_dev, central_rgb_idx[0]);
    } else if (color == 2) {
        led_on(central_led_dev, central_rgb_idx[2]);
    }
}
#else
static void central_set_led_color(uint8_t color) { ARG_UNUSED(color); }
#endif

static void led_restore_work_handler(struct k_work *work) {
    ARG_UNUSED(work);
    central_set_led_color(0);
#if DT_HAS_COMPAT_STATUS_OKAY(DT_NODELABEL(tpl_led))
    struct zmk_behavior_binding binding = {
        .behavior_dev = tpl_led_name,
        .param1 = 0,
    };
    struct zmk_behavior_binding_event event = {
        .position = 0,
        .timestamp = k_uptime_get(),
    };
    (void)zmk_split_central_invoke_behavior(0, &binding, event, false);
#endif
#if IS_ENABLED(CONFIG_RGBLED_WIDGET)
    indicate_layer();
#endif
}

static void flash_led_brief(uint8_t color) {
    central_set_led_color(color);
#if DT_HAS_COMPAT_STATUS_OKAY(DT_NODELABEL(tpl_led))
    struct zmk_behavior_binding binding = {
        .behavior_dev = tpl_led_name,
        .param1 = color,
    };
    struct zmk_behavior_binding_event event = {
        .position = 0,
        .timestamp = k_uptime_get(),
    };
    (void)zmk_split_central_invoke_behavior(0, &binding, event, false);
#endif
    k_work_reschedule(&led_restore_work, K_MSEC(450));
}

static int push_n_to_left_peripheral(void) {
#if DT_HAS_COMPAT_STATUS_OKAY(DT_NODELABEL(lpps_set_accum))
    if (!zmk_split_bt_peripheral_is_connected()) {
        return 0;
    }
    struct zmk_behavior_binding binding = {
        .behavior_dev = lpps_set_accum_name,
        .param1 = left_tp_accum_n,
    };
    struct zmk_behavior_binding_event event = {
        .position = 0,
        .timestamp = k_uptime_get(),
    };
    return zmk_split_central_invoke_behavior(0, &binding, event, false);
#else
    return 0;
#endif
}

static int save_left_tp_settings(void) {
#if IS_ENABLED(CONFIG_SETTINGS)
    settings_save_one("trackpoint/left_en", &left_tp_enabled, sizeof(left_tp_enabled));
    settings_save_one("trackpoint/left_n", &left_tp_accum_n, sizeof(left_tp_accum_n));
#endif
    return 0;
}

bool zmk_left_trackpoint_is_enabled(void) { return left_tp_enabled; }

#define DT_DRV_COMPAT zmk_input_processor_left_tp_gate

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int left_tp_gate_handle_event(const struct device *dev, struct input_event *event,
                                     uint32_t unused, uint32_t unused2,
                                     struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(unused);
    ARG_UNUSED(unused2);
    ARG_UNUSED(state);

    if (!left_tp_enabled && event->type == INPUT_EV_REL) {
        return ZMK_INPUT_PROC_STOP;
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api left_tp_gate_api = {
    .handle_event = left_tp_gate_handle_event,
};

#define LEFT_TP_GATE_INST(n)                                                                       \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                   \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &left_tp_gate_api);

DT_INST_FOREACH_STATUS_OKAY(LEFT_TP_GATE_INST)

#endif

#define DT_DRV_COMPAT zmk_behavior_tp_left_ctl

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_tp_left_ctl_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);

    switch (binding->param1) {
    case TP_LEFT_CTL_TOGGLE:
        left_tp_enabled = !left_tp_enabled;
        save_left_tp_settings();
        return ZMK_BEHAVIOR_OPAQUE;
    case TP_LEFT_CTL_N_UP:
        if (left_tp_accum_n < TP_LEFT_N_MAX) {
            left_tp_accum_n++;
        }
        save_left_tp_settings();
        push_n_to_left_peripheral();
        flash_led_brief(1);
        return ZMK_BEHAVIOR_OPAQUE;
    case TP_LEFT_CTL_N_DOWN:
        if (left_tp_accum_n > TP_LEFT_N_MIN) {
            left_tp_accum_n--;
        }
        save_left_tp_settings();
        push_n_to_left_peripheral();
        flash_led_brief(2);
        return ZMK_BEHAVIOR_OPAQUE;
    default:
        return ZMK_BEHAVIOR_OPAQUE;
    }
}

static int on_tp_left_ctl_released(struct zmk_behavior_binding *binding,
                                   struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api tp_left_ctl_api = {
    .binding_pressed = on_tp_left_ctl_pressed,
    .binding_released = on_tp_left_ctl_released,
    .locality = BEHAVIOR_LOCALITY_GLOBAL,
};

#define TP_LEFT_CTL_INST(n)                                                                        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                 \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tp_left_ctl_api);

DT_INST_FOREACH_STATUS_OKAY(TP_LEFT_CTL_INST)

#endif

#if IS_ENABLED(CONFIG_SETTINGS)

static int trackpoint_left_settings_set(const char *name, size_t len, settings_read_cb read_cb,
                                        void *cb_arg) {
    if (settings_name_steq(name, "left_en", NULL)) {
        if (len == sizeof(left_tp_enabled)) {
            read_cb(cb_arg, &left_tp_enabled, len);
        }
    } else if (settings_name_steq(name, "left_n", NULL)) {
        if (len == sizeof(left_tp_accum_n)) {
            read_cb(cb_arg, &left_tp_accum_n, len);
            if (left_tp_accum_n < TP_LEFT_N_MIN) {
                left_tp_accum_n = TP_LEFT_N_MIN;
            }
            if (left_tp_accum_n > TP_LEFT_N_MAX) {
                left_tp_accum_n = TP_LEFT_N_MAX;
            }
        }
    }
    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(trackpoint_left, "trackpoint", NULL, trackpoint_left_settings_set,
                               NULL, NULL);

#endif

static int trackpoint_left_central_init(void) {
    k_work_init_delayable(&led_restore_work, led_restore_work_handler);
    push_n_to_left_peripheral();
    return 0;
}

SYS_INIT(trackpoint_left_central_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

static int on_peripheral_status(const zmk_event_t *eh) {
    const struct zmk_split_peripheral_status_changed *ev =
        as_zmk_split_peripheral_status_changed(eh);

    if (ev == NULL || !ev->connected) {
        return ZMK_EV_EVENT_BUBBLE;
    }
    push_n_to_left_peripheral();
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(trackpoint_left_periph, on_peripheral_status);
ZMK_SUBSCRIPTION(trackpoint_left_periph, zmk_split_peripheral_status_changed);

#endif
