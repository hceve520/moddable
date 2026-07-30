/*
 * Copyright (c) 2026 Moddable Tech, Inc
 *
 *   This file is part of the Moddable SDK Tools.
 *
 *   The Moddable SDK Tools is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The Moddable SDK Tools is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with the Moddable SDK Tools.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

declare module "piu/PortPath" {
  import { Port, PortConstructor, Color, LinearGradient } from "piu/MC";
  import { Outline } from "commodetto/outline";

  type Paint = Color | number | LinearGradient;

  interface PathPort extends Port {
    fillStyle: Paint;
    strokeStyle: Paint;
    lineWidth: number;
    lineCap: number;
    lineJoin: number;
    blend: number;

    drawOutline(paint: Paint, blend: number, outline: Outline, x?: number, y?: number): void;

    beginPath(): this;
    moveTo(x: number, y: number): this;
    lineTo(x: number, y: number): this;
    quadraticCurveTo(cpx: number, cpy: number, x: number, y: number): this;
    bezierCurveTo(cp1x: number, cp1y: number, cp2x: number, cp2y: number, x: number, y: number): this;
    arc(x: number, y: number, radius: number, startAngle: number, endAngle: number, counterclockwise?: boolean): this;
    arcTo(x1: number, y1: number, x2: number, y2: number, radius: number): this;
    ellipse(x: number, y: number, radiusX: number, radiusY: number, rotation: number, startAngle: number, endAngle: number, counterclockwise?: boolean): this;
    rect(x: number, y: number, w: number, h: number): this;
    closePath(): this;

    fill(rule?: number): this;
    stroke(weight?: number, cap?: number, join?: number): this;

    fillRoundRect(color: Paint, x: number, y: number, w: number, h: number, r: number, blend?: number): this;
    strokeRoundRect(color: Paint, x: number, y: number, w: number, h: number, r: number, weight?: number, blend?: number): this;
  }

  interface PathPortConstructor extends PortConstructor {
    new(behaviorData?: any, dictionary?: object): PathPort;
    (behaviorData?: any, dictionary?: object): PathPort;
    Outline: typeof Outline;
  }

  export const PathPort: PathPortConstructor;
  export default PathPort;

  global {
    const PathPort: PathPortConstructor;
  }
}
