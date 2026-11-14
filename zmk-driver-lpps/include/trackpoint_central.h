/*
 * SPDX-License-Identifier: MIT
 *
 * Central-only TrackPoint control (toggle / runtime N). Invoked from
 * behavior_tp_left_ctl on the split central half.
 */

#pragma once

#include <zmk/behavior.h>

#if IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)

int trackpoint_ctl_on_pressed(struct zmk_behavior_binding *binding,
                              struct zmk_behavior_binding_event event);

#endif
