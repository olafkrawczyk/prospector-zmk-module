#define DT_DRV_COMPAT zmk_behavior_pomodoro

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>
#include <zmk/behavior.h>
#include <zmk/pomodoro.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct behavior_pomodoro_config {
    uint8_t action; /* 0 = start/stop toggle, 1 = cycle preset */
};

static int on_pomodoro_binding_pressed(struct zmk_behavior_binding *binding,
                                       struct zmk_behavior_binding_event event) {
    const struct device *dev = zmk_behavior_get_binding(binding->behavior_dev);
    const struct behavior_pomodoro_config *cfg = dev->config;

    if (cfg->action == 1) {
        zmk_pomodoro_cycle_preset();
    } else {
        zmk_pomodoro_start_stop();
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_pomodoro_binding_released(struct zmk_behavior_binding *binding,
                                        struct zmk_behavior_binding_event event) {
    return ZMK_BEHAVIOR_OPAQUE;
}

static const struct behavior_driver_api behavior_pomodoro_driver_api = {
    .binding_pressed = on_pomodoro_binding_pressed,
    .binding_released = on_pomodoro_binding_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .get_parameter_metadata = zmk_behavior_get_empty_param_metadata,
#endif
};

#define POMO_INST(n)                                                                               \
    static const struct behavior_pomodoro_config behavior_pomodoro_config_##n = {                  \
        .action = DT_INST_PROP_OR(n, action, 0),                                                   \
    };                                                                                             \
    BEHAVIOR_DT_INST_DEFINE(n, NULL, NULL, NULL, &behavior_pomodoro_config_##n, POST_KERNEL,       \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &behavior_pomodoro_driver_api);

DT_INST_FOREACH_STATUS_OKAY(POMO_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */
