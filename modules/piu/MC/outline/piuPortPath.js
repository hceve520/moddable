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

import { Outline } from "commodetto/outline";

/*
	PathPort — Port with a lightweight Canvas-like path API.
	Use only inside onDraw(). Not a full CanvasRenderingContext2D.

	Performance notes for MCU / dense dashboards:
	- Prefer one long-lived PathPort; do not create a new PathPort every frame.
	- beginPath() allocates a fresh CanvasPath. For static geometry, build an
	  Outline once (Outline.fill / Outline.stroke) and call drawOutline() each
	  frame instead of fill()/stroke().
	- fill() and stroke() allocate a new Outline every call — fine for a few
	  dynamic paths, expensive if repeated for many gauges per frame.
	- Gradients passed as fillStyle/strokeStyle are converted each draw; reuse
	  the same descriptor object across frames.
*/

function state(port) {
	let canvas = port._portPath;
	if (!canvas) {
		canvas = port._portPath = {
			path: null,
			fillStyle: "white",
			strokeStyle: "white",
			lineWidth: 1,
			lineCap: Outline.LINECAP_ROUND,
			lineJoin: Outline.LINEJOIN_ROUND,
			blend: 255,
		};
	}
	return canvas;
}

function requirePath(port) {
	const canvas = state(port);
	if (!canvas.path)
		canvas.path = new Outline.CanvasPath();
	return canvas.path;
}

const pathPort = {
	__proto__: Port.prototype,

	drawOutline(paint, blend, outline, x, y) {
		return native("PiuPort_drawOutline").call(this, paint, blend, outline, x, y);
	},

	get fillStyle() { return state(this).fillStyle; },
	set fillStyle(value) { state(this).fillStyle = value; },
	get strokeStyle() { return state(this).strokeStyle; },
	set strokeStyle(value) { state(this).strokeStyle = value; },
	get lineWidth() { return state(this).lineWidth; },
	set lineWidth(value) { state(this).lineWidth = value; },
	get lineCap() { return state(this).lineCap; },
	set lineCap(value) { state(this).lineCap = value; },
	get lineJoin() { return state(this).lineJoin; },
	set lineJoin(value) { state(this).lineJoin = value; },
	get blend() { return state(this).blend; },
	set blend(value) { state(this).blend = value; },

	beginPath() {
		state(this).path = new Outline.CanvasPath();
		return this;
	},
	moveTo(x, y) {
		requirePath(this).moveTo(x, y);
		return this;
	},
	lineTo(x, y) {
		requirePath(this).lineTo(x, y);
		return this;
	},
	quadraticCurveTo(cpx, cpy, x, y) {
		requirePath(this).quadraticCurveTo(cpx, cpy, x, y);
		return this;
	},
	bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y) {
		requirePath(this).bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
		return this;
	},
	arc(x, y, radius, startAngle, endAngle, counterclockwise) {
		requirePath(this).arc(x, y, radius, startAngle, endAngle, counterclockwise);
		return this;
	},
	arcTo(x1, y1, x2, y2, radius) {
		requirePath(this).arcTo(x1, y1, x2, y2, radius);
		return this;
	},
	ellipse(x, y, radiusX, radiusY, rotation, startAngle, endAngle, counterclockwise) {
		requirePath(this).ellipse(x, y, radiusX, radiusY, rotation, startAngle, endAngle, counterclockwise);
		return this;
	},
	rect(x, y, w, h) {
		requirePath(this).rect(x, y, w, h);
		return this;
	},
	closePath() {
		requirePath(this).closePath();
		return this;
	},

	fill(rule) {
		const canvas = state(this);
		if (!canvas.path)
			return this;
		const outline = Outline.fill(canvas.path, rule);
		if (outline)
			this.drawOutline(canvas.fillStyle, canvas.blend, outline);
		return this;
	},
	stroke(weight, cap, join) {
		const canvas = state(this);
		if (!canvas.path)
			return this;
		const outline = Outline.stroke(
			canvas.path,
			(weight !== undefined) ? weight : canvas.lineWidth,
			(cap !== undefined) ? cap : canvas.lineCap,
			(join !== undefined) ? join : canvas.lineJoin
		);
		if (outline)
			this.drawOutline(canvas.strokeStyle, canvas.blend, outline);
		return this;
	},

	fillRoundRect(color, x, y, w, h, r, blend) {
		const outline = Outline.fill(Outline.RoundRectPath(x, y, w, h, r));
		if (outline)
			this.drawOutline(color, (blend !== undefined) ? blend : state(this).blend, outline);
		return this;
	},
	strokeRoundRect(color, x, y, w, h, r, weight, blend) {
		const canvas = state(this);
		const outline = Outline.stroke(
			Outline.RoundRectPath(x, y, w, h, r),
			(weight !== undefined) ? weight : canvas.lineWidth,
			canvas.lineCap,
			canvas.lineJoin
		);
		if (outline)
			this.drawOutline(color, (blend !== undefined) ? blend : canvas.blend, outline);
		return this;
	},
};

export const PathPort = Template(pathPort);
PathPort.Outline = Outline;
Object.freeze(pathPort);
globalThis.PathPort = PathPort;

export default PathPort;
