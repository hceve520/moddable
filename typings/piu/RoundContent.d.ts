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

declare module "piu/RoundContent" {
  import { Container, ContainerDictionary, LinearGradient } from "piu/MC";

  interface RoundContent extends Container {
    border: number;
    radius: number;
    fillGradient: LinearGradient | null;
    strokeGradient: LinearGradient | null;
  }
  interface RoundContentDictionary extends ContainerDictionary {
    border?: number;
    radius?: number;
    fillGradient?: LinearGradient;
    strokeGradient?: LinearGradient;
  }
  interface RoundContentConstructor {
    new(behaviorData?: any, dictionary?: RoundContentDictionary): RoundContent;
    (behaviorData?: any, dictionary?: RoundContentDictionary): RoundContent;

    template<T>(this: T, fn: (arg: object) => RoundContentDictionary): T;
  }

  global {
    const RoundContent: RoundContentConstructor
  }
}
