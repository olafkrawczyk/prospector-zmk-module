# Prospector ZMK Module

This is a [ZMK module](https://zmk.dev/docs/features/modules) that provides custom status screen support for the [Prospector](https://github.com/carrefinho/prospector) display dongle.

![Four status screen layouts for Prospector](docs/images/status-screen-update-hero.png)

> [!IMPORTANT]
> This branch is a work-in-progress and is only compatible with the Zephyr 4.1 version of ZMK (current main).

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Status Screens](#status-screens)
- [Touchscreen](#touchscreen)
- [Usage](#usage)
- [Configuration](#configuration)
- [Troubleshooting](#troubleshooting)
- [Known Issues](#known-issues)
- [To-Do](#to-do)

## Features

- Five status screen layouts to choose from
- Active layer display
- Peripheral battery status
- BLE profile and output indicator
- Active modifier display
- Caps word indicator
- Optional touchscreen support with swipe gestures (screen switching + brightness control)

## Installation

Your ZMK keyboard should be set up with a dongle as central.

Add this module to your `config/west.yml` with these new entries under `remotes` and `projects`:

```yaml
manifest:
  remotes:
    - name: zmkfirmware
      url-base: https://github.com/zmkfirmware
    - name: carrefinho                            # <--- add this
      url-base: https://github.com/carrefinho     # <--- and this
  projects:
    - name: zmk
      remote: zmkfirmware
      revision: main
      import: app/west.yml
    - name: prospector-zmk-module                 # <--- and these
      remote: carrefinho                          # <---
      revision: feat/new-status-screens           # <---
  self:
    path: config
```

Then add the `prospector_adapter` shield to the dongle in your `build.yaml`:

```yaml
---
include:
  - board: xiao_ble//zmk
    shield: [YOUR KEYBOARD SHIELD]_dongle prospector_adapter
```

For more information on ZMK Modules and building locally, see [the ZMK docs page on modules.](https://zmk.dev/docs/features/modules)

## Status Screens

Classic is used by default. To choose a different boot screen, add one of the following to your `.conf` file:

```ini
CONFIG_PROSPECTOR_STATUS_SCREEN_RADII=y
CONFIG_PROSPECTOR_STATUS_SCREEN_FIELD=y
CONFIG_PROSPECTOR_STATUS_SCREEN_OPERATOR=y
```

## Touchscreen

The Prospector's display is the Waveshare 1.69" Touch LCD Module, which
includes a CST816S capacitive touch controller (I2C, address `0x15`).
However, the official assembly guide instructs builders to cut the four
touch wires — only the eight LCD wires are soldered. Touch support is
therefore **off by default** and requires a small hardware modification.

### Hardware wiring

Open the Prospector case and solder four jumpers from the display module's
12-pin header to the XIAO nRF52840:

| Display pin | XIAO pin | Notes |
| ----------- | -------- | ----- |
| `TP_SDA`    | D4       | Shares I2C bus with APDS9960 (if populated) |
| `TP_SCL`    | D5       | Shares I2C bus with APDS9960 (if populated) |
| `TP_RST`    | D0       | Active-low reset |
| `TP_IRQ`    | D1       | Active-low interrupt, pull-up |

If the pre-crimped wires were cut too short, solder directly to the
display's header pins.

### Enabling touch in firmware

1. Add the touch nodes to your dongle's `.overlay`:

```dts
&i2c1 {
    cst816s: cst816s@15 {
        compatible = "hynitron,cst816s";
        status = "okay";
        reg = <0x15>;
        irq-gpios = <&xiao_d 1 (GPIO_ACTIVE_LOW | GPIO_PULL_UP)>;
        rst-gpios = <&xiao_d 0 GPIO_ACTIVE_LOW>;
    };
};

/ {
    lvgl_pointer_input: lvgl_pointer_input {
        compatible = "zephyr,lvgl-pointer-input";
        input = <&cst816s>;
        /* The CST816S coordinate frame is rotated 180° relative to the
         * ST7789 display in the standard Prospector mounting. Uncomment
         * if your swipe directions are inverted:
        invert-x;
        invert-y;
        */
    };
};
```

> [!NOTE]
> These nodes must be defined in your keyboard's dongle overlay, not in a
> module overlay. Zephyr concatenates devicetree overlays in shield order
> (keyboard shield first, `prospector_adapter` second), and dtc resolves
> `&label` references only after the label's definition in the combined file.

2. Enable the touchscreen feature in your `.conf` file:

```ini
CONFIG_PROSPECTOR_TOUCHSCREEN=y
```

3. (Optional) Compile in additional status screens to swipe between:

```ini
CONFIG_PROSPECTOR_SCREEN_OPERATOR_ENABLED=y
```

The boot screen (chosen via the `PROSPECTOR_STATUS_SCREEN_*` choice above)
is always compiled in. Additional screens require `PROSPECTOR_TOUCHSCREEN=y`.

### Gestures

All widgets are treated as **inert** — touches never trigger clicks,
scrolls, or other widget interactions. Only swipe gestures are recognized:

| Swipe        | Action                                            |
| ------------ | ------------------------------------------------- |
| Left / Right | Cycle through compiled-in screens (slide animation) |
| Up / Down    | Raise / lower display brightness (10% steps)       |

Brightness gestures work even with a single screen compiled in. The
brightness range is clamped to 1–100%.

When `PROSPECTOR_TOUCHSCREEN` is enabled, the LVGL heap is raised to 32 KB
automatically to accommodate multiple screen trees. If you encounter a RAM
overflow, reduce the display buffer:

```ini
CONFIG_LV_Z_VDB_SIZE=25
```

## Usage

For split keyboards, the peripheral battery widget arranges sub-widgets in pairing order. After flashing the dongle, pair the left side first, then the right side. For more than two peripherals, pair them left to right.

The layer display shows the `display-name` property when available, falling back to the layer index otherwise. To add a `display-name` to a keymap layer:

```dts
keymap {
  compatible = "zmk,keymap";
  base {
    display-name = "Base";           # <--- add this
    bindings = <
      ...
    >;
  }
}
```

## Configuration

To customize, add config options to your `.conf` file:
```ini
CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR=n
CONFIG_PROSPECTOR_FIXED_BRIGHTNESS=80
```

### General
| Name | Description | Default |
| ---- | ----------- | ------- |
| `CONFIG_PROSPECTOR_ROTATE_DISPLAY_180` | Rotate the display 180 degrees | n |
| `CONFIG_PROSPECTOR_USE_AMBIENT_LIGHT_SENSOR` | Use ambient light sensor for auto brightness | y |
| `CONFIG_PROSPECTOR_FIXED_BRIGHTNESS` | Fixed display brightness when not using ambient light sensor | 50 (1-100) |
| `CONFIG_PROSPECTOR_LAYER_NAME_UPPERCASE` | Convert layer names to uppercase (Operator and Radii only) | y |

### Touchscreen
| Name | Description | Default |
| ---- | ----------- | ------- |
| `CONFIG_PROSPECTOR_TOUCHSCREEN` | Enable CST816S touch input and swipe gestures (screen switching + brightness). Requires soldering TP_* wires (see [Touchscreen](#touchscreen)). | n |
| `CONFIG_PROSPECTOR_SCREEN_CLASSIC_ENABLED` | Compile the Classic screen (always enabled when it is the boot screen) | y if default |
| `CONFIG_PROSPECTOR_SCREEN_RADII_ENABLED` | Compile the Radii screen for swipe switching | y if default, requires `PROSPECTOR_TOUCHSCREEN` |
| `CONFIG_PROSPECTOR_SCREEN_FIELD_ENABLED` | Compile the Field screen for swipe switching | y if default, requires `PROSPECTOR_TOUCHSCREEN` |
| `CONFIG_PROSPECTOR_SCREEN_OPERATOR_ENABLED` | Compile the Operator screen for swipe switching | y if default, requires `PROSPECTOR_TOUCHSCREEN` |
| `CONFIG_PROSPECTOR_SCREEN_BONGO_ENABLED` | Compile the Bongo Cat screen for swipe switching | y if default, requires `PROSPECTOR_TOUCHSCREEN` |

### Modifiers
| Name | Description | Default |
| ---- | ----------- | ------- |
| `CONFIG_PROSPECTOR_SHOW_MODIFIERS` | Display modifier key indicators | y |
| `CONFIG_PROSPECTOR_SHOW_INACTIVE_MODIFIERS` | Show inactive modifiers dimmed (Classic and Field only) | y |
| `CONFIG_PROSPECTOR_MODIFIER_ORDER` | Order of modifiers: G=GUI, A=Alt, C=Ctrl, S=Shift | "GACS" |

### Field-specific
| Name | Description | Default |
| ---- | ----------- | ------- |
| `CONFIG_PROSPECTOR_ANIMATION_WPM_REFERENCE` | WPM value at which animation reaches max speed | 70 |
| `CONFIG_PROSPECTOR_ANIMATION_INTENSITY_DECAY_SEC` | Seconds for lines to fade out after typing stops | 30 |
| `CONFIG_PROSPECTOR_ANIMATION_FLOW_DECAY_SEC` | Seconds for line directions and length to settle | 300 |

## Troubleshooting

### RAM overflow error

If you encounter a `region 'RAM' overflowed` error when building, add the following to your `.conf` file to reduce the display buffer size:

```ini
CONFIG_LV_Z_VDB_SIZE=25
```

## Known Issues

- One peripheral may fail to register key presses after connecting to the dongle; reset the affected peripheral to fix. https://github.com/zmkfirmware/zmk/issues/3156
- Operator, Radii: battery display only supports up to three peripherals

## To-Do

- Operator: per-profile BLE status
- Radii: document and improve color theme customization
- OS-specific modifier styles
- Caps lock indication
