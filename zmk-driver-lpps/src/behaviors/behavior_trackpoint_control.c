#define DT_DRV_COMPAT zmk_behavior_trackpoint_control

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_NODELABEL(tpc))

static int on_tpc_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_tpc_released(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api tpc_driver_api = {
    .binding_pressed = on_tpc_pressed,
    .binding_released = on_tpc_released,
};

BEHAVIOR_DT_DEFINE(DT_NODELABEL(tpc), NULL, NULL, NULL, NULL, POST_KERNEL,
                   CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tpc_driver_api);

#endif
