/*
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <drivers/input_processor.h>
#include <zephyr/input/input.h>

#include <dt-bindings/zmk/trackpoint_control.h>
#include <string.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define TPC_POS_LEFT  44
#define TPC_POS_RIGHT 45

static uint8_t tpc_mode_left;
static uint8_t tpc_mode_right;

static int binding_is_tpc(const struct zmk_behavior_binding *binding) {
    if (binding == NULL || binding->behavior_dev == NULL) {
        return 0;
    }

#if DT_NODE_HAS_STATUS(DT_NODELABEL(tpc), okay)
    return strcmp(binding->behavior_dev, DEVICE_DT_NAME(DT_NODELABEL(tpc))) == 0;
#else
    return strcmp(binding->behavior_dev, "TRACKPOINT_CONTROL") == 0;
#endif
}

static int tpc_config_init(void) {
    tpc_mode_left = MOUSE_MOVE;
    tpc_mode_right = MOUSE_MOVE;

    const struct zmk_behavior_binding *left =
        zmk_keymap_get_layer_binding_at_idx(zmk_keymap_layer_default(), TPC_POS_LEFT);
    const struct zmk_behavior_binding *right =
        zmk_keymap_get_layer_binding_at_idx(zmk_keymap_layer_default(), TPC_POS_RIGHT);

    if (binding_is_tpc(left)) {
        tpc_mode_left = left->param1;
        LOG_INF("LTP mode: %s", left->param1 == MOUSE_SCRL ? "MOUSE_SCRL" : "MOUSE_MOVE");
    }

    if (binding_is_tpc(right)) {
        tpc_mode_right = right->param1;
        LOG_INF("RTP mode: %s", right->param1 == MOUSE_SCRL ? "MOUSE_SCRL" : "MOUSE_MOVE");
    }

    return 0;
}

SYS_INIT(tpc_config_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);

#define DT_DRV_COMPAT zmk_input_processor_tpc_mode

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int tpc_scroll_rem_y;
static int tpc_scroll_rem_x;
static int64_t tpc_last_scroll_ms;

static int tpc_handle_event(const struct device *dev, struct input_event *event, uint32_t side,
                            uint32_t unused, struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(unused);
    ARG_UNUSED(state);

    const uint8_t mode = (side == 0) ? tpc_mode_left : tpc_mode_right;

    if (mode != MOUSE_SCRL || event->type != INPUT_EV_REL) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();
    if ((now - tpc_last_scroll_ms) > 300) {
        tpc_scroll_rem_x = 0;
        tpc_scroll_rem_y = 0;
    }
    tpc_last_scroll_ms = now;

    switch (event->code) {
    case INPUT_REL_Y: {
        int val = event->value + tpc_scroll_rem_y;
        int scaled = val / 4;
        tpc_scroll_rem_y = val - (scaled * 4);
        event->code = INPUT_REL_WHEEL;
        event->value = -scaled;
        break;
    }
    case INPUT_REL_X: {
        int val = event->value + tpc_scroll_rem_x;
        int scaled = val / 4;
        tpc_scroll_rem_x = val - (scaled * 4);
        event->code = INPUT_REL_HWHEEL;
        event->value = scaled;
        break;
    }
    default:
        break;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static struct zmk_input_processor_driver_api tpc_proc_api = {.handle_event = tpc_handle_event};

#define TPC_INST(n)                                                                                \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                   \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tpc_proc_api);

DT_INST_FOREACH_STATUS_OKAY(TPC_INST)

#endif
