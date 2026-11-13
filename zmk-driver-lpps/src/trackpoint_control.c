/*
 * SPDX-License-Identifier: MIT
 */

#include <stdlib.h>
#include <string.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/init.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>
#include <dt-bindings/zmk/trackpoint_control.h>

#include <zmk/behavior.h>
#include <zmk/keymap.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define TPC_POS_LEFT  44
#define TPC_POS_RIGHT 45

#define TPC_SCROLL_DIV     32
#define TPC_SCROLL_IDLE_MS 500

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

static int tpc_frame_dx;
static int tpc_frame_dy;
static int tpc_scroll_rem_x;
static int tpc_scroll_rem_y;
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

    if (event->code != INPUT_REL_X && event->code != INPUT_REL_Y) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    const int64_t now = k_uptime_get();
    if ((now - tpc_last_scroll_ms) > TPC_SCROLL_IDLE_MS) {
        tpc_frame_dx = 0;
        tpc_frame_dy = 0;
        tpc_scroll_rem_x = 0;
        tpc_scroll_rem_y = 0;
    }
    tpc_last_scroll_ms = now;

    if (event->code == INPUT_REL_X) {
        tpc_frame_dx += event->value;
    } else {
        tpc_frame_dy += event->value;
    }

    /* Wait until the sync frame boundary so X and Y can be compared together. */
    if (!event->sync) {
        return ZMK_INPUT_PROC_STOP;
    }

    const int dx = tpc_frame_dx;
    const int dy = tpc_frame_dy;
    tpc_frame_dx = 0;
    tpc_frame_dy = 0;

    if (dx == 0 && dy == 0) {
        return ZMK_INPUT_PROC_STOP;
    }

    /*
     * Dominant-axis lock (bias slightly toward vertical scroll unless horizontal
     * push is clearly stronger: |dx| > |dy| * 1.5) + instant remainder reset on
     * direction reversal.
     */
    if (abs(dx) * 2 > abs(dy) * 3) {
        tpc_scroll_rem_y = 0;
        if ((dx > 0 && tpc_scroll_rem_x < 0) || (dx < 0 && tpc_scroll_rem_x > 0)) {
            tpc_scroll_rem_x = 0;
        }
        const int val = dx + tpc_scroll_rem_x;
        const int scaled = val / TPC_SCROLL_DIV;
        tpc_scroll_rem_x = val - (scaled * TPC_SCROLL_DIV);
        if (scaled == 0) {
            return ZMK_INPUT_PROC_STOP;
        }
        event->code = INPUT_REL_HWHEEL;
        event->value = -scaled;
        return ZMK_INPUT_PROC_CONTINUE;
    } else {
        tpc_scroll_rem_x = 0;
        if ((dy > 0 && tpc_scroll_rem_y < 0) || (dy < 0 && tpc_scroll_rem_y > 0)) {
            tpc_scroll_rem_y = 0;
        }
        const int val = dy + tpc_scroll_rem_y;
        const int scaled = val / TPC_SCROLL_DIV;
        tpc_scroll_rem_y = val - (scaled * TPC_SCROLL_DIV);
        if (scaled == 0) {
            return ZMK_INPUT_PROC_STOP;
        }
        event->code = INPUT_REL_WHEEL;
        event->value = scaled;
        return ZMK_INPUT_PROC_CONTINUE;
    }
}

static struct zmk_input_processor_driver_api tpc_proc_api = {.handle_event = tpc_handle_event};

#define TPC_INST(n)                                                                                \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                   \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tpc_proc_api);

DT_INST_FOREACH_STATUS_OKAY(TPC_INST)

#endif
