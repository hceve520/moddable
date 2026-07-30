# Piu MCU Widgets
Copyright 2026 Moddable Tech, Inc.<BR>
Revised: July 30, 2026

## Overview

The MCU widget kit provides common interactive controls for Piu apps running on microcontrollers:

- `Button`
- `Switch`
- `Slider` (horizontal)
- `ProgressBar`

Widgets are built on [`RoundContent`](../commodetto/outline/Outlines.md#draw-using-piu-roundcontent-object) and color `Skin` `radius` / gradient fields, so controls get anti-aliased rounded corners without texture assets.

## Setup

Include the widgets manifest (it already pulls in Outline / RoundContent):

```json
"include": "$(MODDABLE)/modules/piu/MC/widgets/manifest.json"
```

Import:

```javascript
import { Button, Switch, Slider, ProgressBar } from "piu/widgets";
```

Default button style uses `20px Open Sans`. Add the font resource to your app manifest, or pass a custom `style`.

## Button

```javascript
new Button(null, {
	left: 20, right: 20, top: 24, height: 48,
	string: "OK",
});
```

Touch states: `0` disabled, `1` normal, `2` pressed.

On tap, the button bubbles `onButtonPressed` (or bubbles `container.name` when set).

Optional dictionary fields: `string`, `skin`, `style`.

## Switch

```javascript
const model = { value: true };
new Switch(model, { right: 20, top: 80 });
```

The behavior data object must provide a boolean `value`. Drag or tap toggles it and bubbles `onSwitchChanged` with the new value.

Optional dictionary fields: `barSkin`, `buttonSkin`.

## Slider

```javascript
const model = { min: 0, max: 100, value: 40, step: 1 };
new Slider(model, { left: 20, right: 20, top: 120 });
```

Bubbles:

- `onSliderChanging` while dragging
- `onSliderChanged` on touch end

`HorizontalSlider` is an alias of `Slider`.

Optional dictionary fields: `trackSkin`, `fillSkin`, `thumbSkin`.

## ProgressBar

```javascript
const model = { min: 0, max: 100, value: 25 };
const bar = new ProgressBar(model, { left: 20, right: 20, top: 170 });
// later:
model.value = 80;
bar.delegate("onDataChanged");
```

Optional dictionary fields: `trackSkin`, `fillSkin`.

## Theming

Default skins/styles are exported for reuse or replacement:

```javascript
import { widgetSkins, widgetStyles } from "piu/widgets";
```

## Example

See `examples/piu/widgets`.
