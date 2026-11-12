/*
 * SPDX-License-Identifier: MIT
 *
 * Central input processor: temporary mouse/TrackPoint layer with net-vector
 * displacement and multi-frame continuity guard.
 *
 * - Pointer movement itself passes through untouched with zero added latency.
 * - Pre-activation tracking uses signed net displacement (|sum(dx)| + |sum(dy)|)
 *   so back-and-forth jitter cancels out instead of accumulating, and requires
 *   at least `min-activation-frames` consecutive sync frames so isolated 1-2
 *   packet leaks never trigger Layer 4.
 * - Once active, any X/Y movement extends `param2` ms timeout for smooth aiming.
 */

#define DT_DRV_COMPAT zmk_input_processor_tp_layer

#include <stdlib.h>

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <drivers/input_processor.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct tp_layer_config {
    int32_t activation_threshold;
    int32_t min_activation_frames;
    int32_t activation_window_ms;
    int32_t require_prior_idle_ms;
    const uint16_t *excluded_positions;
    size_t num_positions;
};

struct tp_layer_data {
    struct k_mutex lock;
    struct k_work activate_work;
    struct k_work_delayable disable_work;
    uint8_t target_layer;
    bool is_active;
    bool frame_has_xy;
    int32_t net_dx;
    int32_t net_dy;
    int32_t frame_count;
    int64_t last_motion_ms;
    int64_t last_non_mod_key_ms;
};

static inline void reset_pre_activation(struct tp_layer_data *data) {
    data->net_dx = 0;
    data->net_dy = 0;
    data->frame_count = 0;
    data->frame_has_xy = false;
}

static bool pos_is_excluded(const struct tp_layer_config *cfg, uint32_t pos) {
    for (size_t i = 0; i < cfg->num_positions; i++) {
        if (cfg->excluded_positions[i] == pos) {
            return true;
        }
    }
    return false;
}

static void activate_work_cb(struct k_work *work) {
    struct tp_layer_data *data = CONTAINER_OF(work, struct tp_layer_data, activate_work);

    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return;
    }

    if (!data->is_active) {
        data->is_active = true;
        zmk_keymap_layer_activate(data->target_layer);
    }

    k_mutex_unlock(&data->lock);
}

static void disable_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct tp_layer_data *data = CONTAINER_OF(dwork, struct tp_layer_data, disable_work);

    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return;
    }

    if (data->is_active) {
        data->is_active = false;
        reset_pre_activation(data);
        if (zmk_keymap_layer_active(data->target_layer)) {
            zmk_keymap_layer_deactivate(data->target_layer);
        }
    }

    k_mutex_unlock(&data->lock);
}

static int tp_layer_handle_event(const struct device *dev, struct input_event *event,
                                 uint32_t layer_idx, uint32_t timeout_ms,
                                 struct zmk_input_processor_state *state) {
    ARG_UNUSED(state);

    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_X && event->code != INPUT_REL_Y)) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (layer_idx >= ZMK_KEYMAP_LAYERS_LEN) {
        return -EINVAL;
    }

    const struct tp_layer_config *cfg = dev->config;
    struct tp_layer_data *data = dev->data;
    const int64_t now = k_uptime_get();

    if (k_mutex_lock(&data->lock, K_FOREVER) < 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    data->target_layer = (uint8_t)layer_idx;

    if (data->is_active) {
        if (timeout_ms > 0 && event->value != 0) {
            k_work_reschedule(&data->disable_work, K_MSEC(timeout_ms));
        }
        k_mutex_unlock(&data->lock);
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if (cfg->require_prior_idle_ms > 0 &&
        (now - data->last_non_mod_key_ms) < cfg->require_prior_idle_ms) {
        reset_pre_activation(data);
        k_mutex_unlock(&data->lock);
        return ZMK_INPUT_PROC_CONTINUE;
    }

    if ((now - data->last_motion_ms) > cfg->activation_window_ms) {
        reset_pre_activation(data);
    }

    if (event->value != 0) {
        data->last_motion_ms = now;
        data->frame_has_xy = true;
        if (event->code == INPUT_REL_X) {
            data->net_dx += event->value;
        } else {
            data->net_dy += event->value;
        }
    }

    if (event->sync && data->frame_has_xy) {
        data->frame_count++;
        data->frame_has_xy = false;
    }

    const int32_t net_disp = abs(data->net_dx) + abs(data->net_dy);
    if (data->frame_count >= cfg->min_activation_frames &&
        net_disp >= cfg->activation_threshold) {
        reset_pre_activation(data);
        k_work_submit(&data->activate_work);
        if (timeout_ms > 0) {
            k_work_reschedule(&data->disable_work, K_MSEC(timeout_ms));
        }
    }

    k_mutex_unlock(&data->lock);
    return ZMK_INPUT_PROC_CONTINUE;
}

static void dispatch_to_instance(const struct device *dev, const zmk_event_t *eh) {
    const struct tp_layer_config *cfg = dev->config;
    struct tp_layer_data *data = dev->data;

    const struct zmk_layer_state_changed *layer_ev = as_zmk_layer_state_changed(eh);
    if (layer_ev != NULL) {
        if (k_mutex_lock(&data->lock, K_FOREVER) == 0) {
            if (data->is_active &&
                !zmk_keymap_layer_active(zmk_keymap_layer_index_to_id(data->target_layer))) {
                data->is_active = false;
                reset_pre_activation(data);
                k_work_cancel_delayable(&data->disable_work);
            }
            k_mutex_unlock(&data->lock);
        }
        return;
    }

    const struct zmk_position_state_changed *pos_ev = as_zmk_position_state_changed(eh);
    if (pos_ev != NULL) {
        if (!pos_ev->state) {
            return;
        }
        if (k_mutex_lock(&data->lock, K_FOREVER) == 0) {
            if (data->is_active && cfg->num_positions > 0 &&
                !pos_is_excluded(cfg, pos_ev->position)) {
                data->is_active = false;
                reset_pre_activation(data);
                k_work_cancel_delayable(&data->disable_work);
                zmk_keymap_layer_deactivate(data->target_layer);
            }
            k_mutex_unlock(&data->lock);
        }
        return;
    }

    const struct zmk_keycode_state_changed *key_ev = as_zmk_keycode_state_changed(eh);
    if (key_ev != NULL) {
        if (is_mod(key_ev->usage_page, key_ev->keycode)) {
            return;
        }
        if (k_mutex_lock(&data->lock, K_FOREVER) == 0) {
            data->last_non_mod_key_ms = key_ev->timestamp;
            reset_pre_activation(data);
            k_mutex_unlock(&data->lock);
        }
    }
}

#define TP_LAYER_DISPATCH(inst) dispatch_to_instance(DEVICE_DT_INST_GET(inst), eh);

static int tp_layer_event_listener(const zmk_event_t *eh) {
    DT_INST_FOREACH_STATUS_OKAY(TP_LAYER_DISPATCH)
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(processor_tp_layer, tp_layer_event_listener);
ZMK_SUBSCRIPTION(processor_tp_layer, zmk_layer_state_changed);
ZMK_SUBSCRIPTION(processor_tp_layer, zmk_position_state_changed);
ZMK_SUBSCRIPTION(processor_tp_layer, zmk_keycode_state_changed);

static int tp_layer_init(const struct device *dev) {
    struct tp_layer_data *data = dev->data;
    k_mutex_init(&data->lock);
    k_work_init(&data->activate_work, activate_work_cb);
    k_work_init_delayable(&data->disable_work, disable_work_cb);
    data->last_non_mod_key_ms = -100000;
    data->last_motion_ms = -100000;
    return 0;
}

static const struct zmk_input_processor_driver_api tp_layer_driver_api = {
    .handle_event = tp_layer_handle_event,
};

#define TP_LAYER_INST(n)                                                                           \
    static struct tp_layer_data tp_layer_data_##n = {};                                            \
    static const uint16_t tp_layer_excluded_##n[] = DT_INST_PROP_OR(n, excluded_positions, {});    \
    static const struct tp_layer_config tp_layer_config_##n = {                                    \
        .activation_threshold = DT_INST_PROP_OR(n, activation_threshold, 24),                      \
        .min_activation_frames = DT_INST_PROP_OR(n, min_activation_frames, 2),                     \
        .activation_window_ms = DT_INST_PROP_OR(n, activation_window_ms, 250),                     \
        .require_prior_idle_ms = DT_INST_PROP_OR(n, require_prior_idle_ms, 250),                   \
        .excluded_positions = tp_layer_excluded_##n,                                               \
        .num_positions = DT_INST_PROP_LEN_OR(n, excluded_positions, 0),                            \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(n, tp_layer_init, NULL, &tp_layer_data_##n, &tp_layer_config_##n,        \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &tp_layer_driver_api);

DT_INST_FOREACH_STATUS_OKAY(TP_LAYER_INST)

#endif
