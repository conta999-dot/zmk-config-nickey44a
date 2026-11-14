#define DT_DRV_COMPAT zmk_behavior_lpps_set_accum

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zmk/behavior.h>

#include <lpps.h>

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

static int on_lpps_set_accum_pressed(struct zmk_behavior_binding *binding,
                                     struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    lpps_set_accum_report(binding->param1);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_lpps_set_accum_released(struct zmk_behavior_binding *binding,
                                      struct zmk_behavior_binding_event event) {
    ARG_UNUSED(binding);
    ARG_UNUSED(event);
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api lpps_set_accum_api = {
    .binding_pressed = on_lpps_set_accum_pressed,
    .binding_released = on_lpps_set_accum_released,
    .locality = BEHAVIOR_LOCALITY_EVENT_SOURCE,
};

#define LPPS_SET_ACCUM_INST(n)                                                                     \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, NULL, POST_KERNEL,                                 \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &lpps_set_accum_api);

DT_INST_FOREACH_STATUS_OKAY(LPPS_SET_ACCUM_INST)

#endif
