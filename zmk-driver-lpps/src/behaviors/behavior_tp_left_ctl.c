/*
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_tp_left_ctl

#include <zephyr/device.h>
#include <drivers/behavior.h>

#include <trackpoint_central.h>

#include <zmk/behavior.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_tp_left_ctl_pressed(struct zmk_behavior_binding *binding,
                                  struct zmk_behavior_binding_event event) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    return trackpoint_ctl_on_pressed(binding, event);
#else
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
#endif
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
#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    /* Keymap is evaluated on the central; avoid split-behavior invokes to the left half. */
    .locality = BEHAVIOR_LOCALITY_CENTRAL,
#else
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
#endif
};

#define TP_LEFT_CTL_INST(n)                                                                        \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                 \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tp_left_ctl_api);

DT_INST_FOREACH_STATUS_OKAY(TP_LEFT_CTL_INST)

#endif
