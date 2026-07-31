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

declare module "piu/widgets" {
  import {
    Behavior,
    Container,
    ContainerDictionary,
    Skin,
    Style,
  } from "piu/MC";

  export const widgetSkins: {
    button: Skin;
    progressTrack: Skin;
    progressFill: Skin;
    switchBar: Skin;
    switchButton: Skin;
    sliderTrack: Skin;
    sliderFill: Skin;
    sliderThumb: Skin;
  };

  export const widgetStyles: {
    button: Style;
  };

  export class ButtonBehavior extends Behavior {}
  export class ProgressBarBehavior extends Behavior {
    setValue(container: Container, value: number): void;
  }
  export class SwitchBehavior extends Behavior {}
  export class SliderBehavior extends Behavior {}

  interface ButtonDictionary extends ContainerDictionary {
    data?: any;
    string?: string;
    skin?: Skin;
    style?: Style;
  }
  interface RangeData {
    min?: number;
    max?: number;
    value?: number;
    step?: number;
  }
  interface SwitchData {
    value?: boolean;
  }
  interface ProgressDictionary extends ContainerDictionary {
    data?: RangeData;
    trackSkin?: Skin;
    fillSkin?: Skin;
  }
  interface SwitchDictionary extends ContainerDictionary {
    data?: SwitchData;
    barSkin?: Skin;
    buttonSkin?: Skin;
  }
  interface SliderDictionary extends ContainerDictionary {
    data?: RangeData;
    trackSkin?: Skin;
    fillSkin?: Skin;
    thumbSkin?: Skin;
  }

  interface ButtonConstructor {
    new(behaviorData?: any, dictionary?: ButtonDictionary): Container;
    (behaviorData?: any, dictionary?: ButtonDictionary): Container;
    template<T>(this: T, fn: (arg: object) => ButtonDictionary): T;
  }
  interface ProgressBarConstructor {
    new(behaviorData?: any, dictionary?: ProgressDictionary): Container;
    (behaviorData?: any, dictionary?: ProgressDictionary): Container;
    template<T>(this: T, fn: (arg: object) => ProgressDictionary): T;
  }
  interface SwitchConstructor {
    new(behaviorData?: any, dictionary?: SwitchDictionary): Container;
    (behaviorData?: any, dictionary?: SwitchDictionary): Container;
    template<T>(this: T, fn: (arg: object) => SwitchDictionary): T;
  }
  interface SliderConstructor {
    new(behaviorData?: any, dictionary?: SliderDictionary): Container;
    (behaviorData?: any, dictionary?: SliderDictionary): Container;
    template<T>(this: T, fn: (arg: object) => SliderDictionary): T;
  }

  export const Button: ButtonConstructor;
  export const ProgressBar: ProgressBarConstructor;
  export const Switch: SwitchConstructor;
  export const Slider: SliderConstructor;
  export const HorizontalSlider: SliderConstructor;

  global {
    const Button: ButtonConstructor;
    const ProgressBar: ProgressBarConstructor;
    const Switch: SwitchConstructor;
    const Slider: SliderConstructor;
    const HorizontalSlider: SliderConstructor;
  }
}
