/*
 * SPDX-License-Identifier: MIT
 *
 * Central input processor: suppress relative TrackPoint motion for `param1` ms
 * after a non-modifier key is pressed, preventing typing vibration / PCB flex
 * jitter and accidental auto-mouse layer activation.
 *
 * Non-conflicting design:
 * - Modifier keys (Shift/Ctrl/Alt/GUI) do NOT trigger lockout so Ctrl+drag works.
 * - Mouse clicks (&mkp) emit mouse events, not zmk_keycode_state_changed, so
 *   clicking while moving the pointer never locks out the cursor.
 */

#define DT_DRV_COMPAT zmk_input_processor_typing_lockout

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>

#include <drivers/input_processor.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/keys.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int64_t last_non_mod_key_ms = -100000;
static atomic_t held_non_mod_keys = ATOMIC_INIT(0);

static int typing_lockout_keycode_listener(const zmk_event_t *eh) {
    const struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);
    if (ev == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (is_mod(ev->usage_page, ev->keycode)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (ev->state) {
        atomic_inc(&held_non_mod_keys);
        last_non_mod_key_ms = ev->timestamp;
    } else {
        if (atomic_get(&held_non_mod_keys) > 0) {
            atomic_dec(&held_non_mod_keys);
        }
        /* Refresh timestamp on release so key-up bounce is also covered. */
        last_non_mod_key_ms = ev->timestamp;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(typing_lockout_key_listener, typing_lockout_keycode_listener);
ZMK_SUBSCRIPTION(typing_lockout_key_listener, zmk_keycode_state_changed);

static int typing_lockout_handle_event(const struct device *dev, struct input_event *event,
                                       uint32_t lockout_ms, uint32_t unused,
                                       struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(unused);
    ARG_UNUSED(state);

    if (event->type != INPUT_EV_REL || lockout_ms == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (atomic_get(&held_non_mod_keys) > 0) {
        return ZMK_INPUT_PROC_STOP;
    }

    int64_t now = k_uptime_get();
    if ((now - last_non_mod_key_ms) < (int64_t)lockout_ms) {
        return ZMK_INPUT_PROC_STOP;
    }

    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api typing_lockout_api = {
    .handle_event = typing_lockout_handle_event,
};

#define TYPING_LOCKOUT_INST(n)                                                                     \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                   \
                          CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &typing_lockout_api);

DT_INST_FOREACH_STATUS_OKAY(TYPING_LOCKOUT_INST)

#endif
