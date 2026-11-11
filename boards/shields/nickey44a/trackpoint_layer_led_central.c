/*
 * SPDX-License-Identifier: MIT
 *
 * Sync solid red on the left peripheral when auto TrackPoint layer (4) is active.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/split/central.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL) && IS_ENABLED(CONFIG_RGBLED_WIDGET)

#define TRACKPOINT_LAYER_IDX 4

#if DT_NODE_HAS_STATUS(DT_NODELABEL(tpl_led), okay)

static bool peripheral_led_on;

static void sync_peripheral_trackpoint_led(bool on) {
    if (peripheral_led_on == on) {
        return;
    }
    peripheral_led_on = on;

    struct zmk_behavior_binding binding = {
        .behavior_dev = DEVICE_DT_NAME(DT_NODELABEL(tpl_led)),
        .param1 = on ? 1 : 0,
    };

    struct zmk_behavior_binding_event event = {
        .position = 0,
        .timestamp = k_uptime_get(),
    };

    (void)zmk_split_central_invoke_behavior(0, &binding, event, true);
}

static int on_trackpoint_layer_state(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(eh);

    if (ev == NULL || ev->layer != TRACKPOINT_LAYER_IDX) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    sync_peripheral_trackpoint_led(ev->state);
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(trackpoint_layer_led_central, on_trackpoint_layer_state);
ZMK_SUBSCRIPTION(trackpoint_layer_led_central, zmk_layer_state_changed);

#endif
#endif
