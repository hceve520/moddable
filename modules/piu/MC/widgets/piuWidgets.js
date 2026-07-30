/*
 * Copyright (c) 2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Runtime.
 *
 *   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The Moddable SDK Runtime is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

import {} from "piu/RoundContent";

/*
	MCU widget kit built on RoundContent + color Skin radius/gradients.
	States (touch, no hover):
		0 = disabled / off track
		1 = normal / on
		2 = pressed / active fill
*/

const ButtonSkin = Skin.template(Object.freeze({
	fill: Object.freeze(["#5A6A72", "#1A73E8", "#1557B0"]),
	stroke: Object.freeze(["#3C4A50", "#0B57D0", "#0A4AAD"]),
	radius: 18,
}));
const ProgressTrackSkin = Skin.template(Object.freeze({
	fill: "#2A3840",
	stroke: "#3C5360",
	radius: 6,
}));
const ProgressFillSkin = Skin.template(Object.freeze({
	fill: "#1A73E8",
	radius: 6,
	fillGradient: Object.freeze({
		x0: 0, y0: 0, x1: 200, y1: 0,
		stops: Object.freeze([
			Object.freeze({ offset: 0, r: 66, g: 133, b: 244 }),
			Object.freeze({ offset: 1, r: 26, g: 115, b: 232 }),
		]),
	}),
}));
const SwitchBarSkin = Skin.template(Object.freeze({
	fill: Object.freeze(["#3C4A50", "#1B7A6E"]),
	stroke: Object.freeze(["#2A3840", "#0E4F47"]),
	radius: 12,
}));
const SwitchButtonSkin = Skin.template(Object.freeze({
	fill: Object.freeze(["#9AA5AB", "#F2F5F7"]),
	stroke: Object.freeze(["#6B787F", "#D7E2E8"]),
	radius: 11,
}));
const SliderTrackSkin = Skin.template(Object.freeze({
	fill: "#2A3840",
	stroke: "#3C5360",
	radius: 4,
}));
const SliderFillSkin = Skin.template(Object.freeze({
	fill: "#1A73E8",
	stroke: "#0B57D0",
	radius: 4,
	fillGradient: Object.freeze({
		x0: 0, y0: 0, x1: 200, y1: 0,
		stops: Object.freeze([
			Object.freeze({ offset: 0, r: 66, g: 133, b: 244 }),
			Object.freeze({ offset: 1, r: 26, g: 115, b: 232 }),
		]),
	}),
}));
const SliderThumbSkin = Skin.template(Object.freeze({
	fill: Object.freeze(["#9AA5AB", "#F2F5F7", "#FFFFFF"]),
	stroke: Object.freeze(["#6B787F", "#D7E2E8", "#FFFFFF"]),
	radius: 11,
}));
const ButtonStyle = Style.template(Object.freeze({
	font: "20px Open Sans",
	color: Object.freeze(["#B0B8BD", "white", "white"]),
	horizontal: "center",
	vertical: "middle",
}));

export const widgetSkins = Object.freeze({
	get button() { return ButtonSkin(); },
	get progressTrack() { return ProgressTrackSkin(); },
	get progressFill() { return ProgressFillSkin(); },
	get switchBar() { return SwitchBarSkin(); },
	get switchButton() { return SwitchButtonSkin(); },
	get sliderTrack() { return SliderTrackSkin(); },
	get sliderFill() { return SliderFillSkin(); },
	get sliderThumb() { return SliderThumbSkin(); },
});

export const widgetStyles = Object.freeze({
	get button() { return ButtonStyle(); },
});

// BUTTON

export class ButtonBehavior extends Behavior {
	changeState(container, state) {
		container.state = state;
		let content = container.first;
		while (content) {
			content.state = state;
			content = content.next;
		}
	}
	onCreate(container, data) {
		this.data = data;
	}
	onDisplaying(container) {
		this.changeState(container, container.active ? 1 : 0);
	}
	onTap(container) {
		const name = container.name;
		if (name)
			container.bubble(name, this.data);
		else
			container.bubble("onButtonPressed", this.data);
	}
	onTouchBegan(container, id, x, y, ticks) {
		if (!container.active)
			return;
		this.changeState(container, 2);
		container.captureTouch(id, x, y, ticks);
	}
	onTouchEnded(container, id, x, y, ticks) {
		if (!container.active)
			return;
		if (container.hit(x, y)) {
			this.changeState(container, 1);
			this.onTap(container);
		}
		else
			this.changeState(container, 1);
	}
	onTouchMoved(container, id, x, y, ticks) {
		if (!container.active)
			return;
		this.changeState(container, container.hit(x, y) ? 2 : 1);
	}
}

export const Button = Container.template(($, it = {}) => ({
	height: 48,
	active: true,
	Behavior: ButtonBehavior,
	contents: [
		RoundContent($, {
			left: 0, right: 0, top: 0, bottom: 0,
			border: 1,
			radius: 18,
			skin: (it && it.skin) || ButtonSkin(),
		}),
		Label($, {
			left: 0, right: 0, top: 0, bottom: 0,
			style: (it && it.style) || ButtonStyle(),
			string: (it && it.string) || "",
		}),
	],
}));

// PROGRESS

export class ProgressBarBehavior extends Behavior {
	getMax() {
		return this.data.max;
	}
	getMin() {
		return this.data.min;
	}
	getValue() {
		return this.data.value;
	}
	onCreate(container, data) {
		this.data = data || { min: 0, max: 100, value: 0 };
		if (this.data.min === undefined) this.data.min = 0;
		if (this.data.max === undefined) this.data.max = 100;
		if (this.data.value === undefined) this.data.value = 0;
	}
	onDataChanged(container) {
		this.onLayoutChanged(container);
	}
	onDisplaying(container) {
		this.onLayoutChanged(container);
	}
	onLayoutChanged(container) {
		const track = container.first;
		const fill = track.first;
		const min = this.getMin();
		const max = this.getMax();
		const value = this.getValue();
		const range = max - min;
		fill.width = range ? Math.round(((value - min) * track.width) / range) : 0;
	}
	setValue(container, value) {
		const min = this.getMin();
		const max = this.getMax();
		if (value < min) value = min;
		else if (value > max) value = max;
		if (this.data.value !== value) {
			this.data.value = value;
			this.onLayoutChanged(container);
			container.bubble("onProgressChanged", value);
		}
	}
}

export const ProgressBar = Container.template(($, it = {}) => ({
	height: 24,
	Behavior: ProgressBarBehavior,
	contents: [
		RoundContent($, {
			left: 0, right: 0, height: 12, top: 6,
			radius: 6,
			skin: (it && it.trackSkin) || ProgressTrackSkin(),
			contents: [
				RoundContent($, {
					left: 0, width: 0, top: 0, bottom: 0,
					radius: 6,
					skin: (it && it.fillSkin) || ProgressFillSkin(),
				}),
			],
		}),
	],
}));

// SWITCH

export class SwitchBehavior extends Behavior {
	changeOffset(container, offset) {
		const bar = container.first;
		const button = bar.next;
		if (offset < 0)
			offset = 0;
		else if (offset > this.size)
			offset = this.size;
		else
			offset = Math.round(offset);
		this.offset = offset;
		bar.state = (container.active && offset) ? 1 : 0;
		button.state = container.active ? 1 : 0;
		button.x = bar.x + offset + 1;
	}
	onCreate(container, data) {
		this.data = data || { value: false };
		if (this.data.value === undefined)
			this.data.value = false;
	}
	onDataChanged(container) {
		this.changeOffset(container, this.data.value ? this.size : 0);
	}
	onDisplaying(container) {
		const bar = container.first;
		const button = bar.next;
		this.size = bar.width - button.width - 2;
		this.onDataChanged(container);
	}
	onTimeChanged(container) {
		this.changeOffset(container, this.anchor + Math.round(this.delta * container.fraction));
	}
	onTouchBegan(container, id, x, y, ticks) {
		if (!container.active)
			return;
		if (container.running) {
			container.stop();
			container.time = container.duration;
		}
		this.anchor = x;
		this.moved = false;
		this.delta = this.offset;
		container.captureTouch(id, x, y, ticks);
	}
	onTouchEnded(container, id, x, y, ticks) {
		if (!container.active)
			return;
		let offset = this.offset;
		const size = this.size;
		let delta = size >> 1;
		if (this.moved) {
			if (offset < delta)
				delta = 0 - offset;
			else
				delta = size - offset;
		}
		else {
			if (offset === 0)
				delta = size;
			else if (offset === size)
				delta = 0 - size;
			else if (x > (container.x + (container.width >> 1)))
				delta = size - offset;
			else
				delta = 0 - offset;
		}
		if (delta) {
			this.anchor = offset;
			this.delta = delta;
			container.duration = 125 * Math.abs(delta) / size;
			container.time = 0;
			container.start();
		}
		const value = ((this.offset + delta) !== 0);
		if (this.data.value !== value) {
			this.data.value = value;
			this.onValueChanged(container);
		}
	}
	onTouchMoved(container, id, x, y, ticks) {
		this.moved = Math.abs(x - this.anchor) >= 8;
		this.changeOffset(container, this.delta + x - this.anchor);
	}
	onValueChanged(container) {
		container.bubble("onSwitchChanged", this.data.value);
	}
}

export const Switch = Container.template(($, it = {}) => ({
	width: 52,
	height: 32,
	active: true,
	Behavior: SwitchBehavior,
	contents: [
		RoundContent($, {
			left: 4, width: 44, top: 6, height: 20,
			border: 1,
			radius: 10,
			skin: (it && it.barSkin) || SwitchBarSkin(),
		}),
		RoundContent($, {
			left: 5, width: 20, top: 6, height: 20,
			border: 1,
			radius: 10,
			skin: (it && it.buttonSkin) || SwitchButtonSkin(),
		}),
	],
}));

// SLIDER

export class SliderBehavior extends Behavior {
	getMax() {
		return this.data.max;
	}
	getMin() {
		return this.data.min;
	}
	getOffset(size) {
		const min = this.getMin();
		const max = this.getMax();
		const value = this.getValue();
		const range = max - min;
		return range ? Math.round(((value - min) * size) / range) : 0;
	}
	getValue() {
		return this.data.value;
	}
	onCreate(container, data) {
		this.data = data || { min: 0, max: 100, value: 0 };
		if (this.data.min === undefined) this.data.min = 0;
		if (this.data.max === undefined) this.data.max = 100;
		if (this.data.value === undefined) this.data.value = this.data.min;
	}
	onDataChanged(container) {
		const active = container.active;
		const thumb = container.last;
		thumb.state = active ? 1 : 0;
		this.onLayoutChanged(container);
	}
	onDisplaying(container) {
		const thumb = container.last;
		this.dx = thumb.x - container.x;
		this.onDataChanged(container);
	}
	onLayoutChanged(container) {
		const thumb = container.last;
		const fill = thumb.previous;
		const track = fill.previous;
		const size = container.width - thumb.width - (this.dx << 1);
		const offset = this.getOffset(size);
		const x = container.x + this.dx + offset;
		thumb.x = x;
		fill.width = Math.max(thumb.width, (x - track.x) + (thumb.width >> 1));
	}
	onTouchBegan(container, id, x, y, ticks) {
		if (!container.active)
			return;
		container.captureTouch(id, x, y, ticks);
		this.onTouchMoved(container, id, x, y, ticks);
	}
	onTouchEnded(container) {
		if (!container.active)
			return;
		container.bubble("onSliderChanged", this.data.value);
	}
	onTouchMoved(container, id, x, y, ticks) {
		if (!container.active)
			return;
		const thumb = container.last;
		const size = container.width - thumb.width - (this.dx << 1);
		const offset = (x - (thumb.width >> 1) - container.x - this.dx);
		this.setOffset(container, size, offset);
		this.onLayoutChanged(container);
		container.bubble("onSliderChanging", this.data.value);
	}
	setOffset(container, size, offset) {
		const min = this.getMin();
		const max = this.getMax();
		let value = min + ((offset * (max - min)) / size);
		if (value < min) value = min;
		else if (value > max) value = max;
		this.setValue(container, value);
	}
	setValue(container, value) {
		const data = this.data;
		if ("step" in data && data.step)
			value = data.step * Math.round(value / data.step);
		data.value = value;
	}
}

export const Slider = Container.template(($, it = {}) => ({
	height: 36,
	active: true,
	Behavior: SliderBehavior,
	contents: [
		RoundContent($, {
			left: 12, right: 12, top: 14, height: 8,
			border: 1,
			radius: 4,
			skin: (it && it.trackSkin) || SliderTrackSkin(),
		}),
		RoundContent($, {
			left: 12, width: 18, top: 14, height: 8,
			border: 1,
			radius: 4,
			skin: (it && it.fillSkin) || SliderFillSkin(),
		}),
		RoundContent($, {
			left: 6, width: 22, top: 7, height: 22,
			border: 1,
			radius: 11,
			skin: (it && it.thumbSkin) || SliderThumbSkin(),
		}),
	],
}));

export const HorizontalSlider = Slider;

Object.freeze(ButtonBehavior.prototype);
Object.freeze(ProgressBarBehavior.prototype);
Object.freeze(SwitchBehavior.prototype);
Object.freeze(SliderBehavior.prototype);

globalThis.Button = Button;
globalThis.ProgressBar = ProgressBar;
globalThis.Switch = Switch;
globalThis.Slider = Slider;
globalThis.HorizontalSlider = HorizontalSlider;
