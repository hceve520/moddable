/*
	MCU widget kit demo: Button, Switch, Slider, ProgressBar.
*/
import {} from "piu/MC";
import { Button, Switch, Slider, ProgressBar } from "piu/widgets";

const backgroundSkin = new Skin({ fill: "#102027" });
const titleStyle = new Style({ font: "20px Open Sans", color: "#D7E2E8", horizontal: "left" });
const valueStyle = new Style({ font: "20px Open Sans", color: "white", horizontal: "right" });

class DemoBehavior extends Behavior {
	onCreate(application, $) {
		this.$ = $;
	}
	onButtonPressed(application) {
		const $ = this.$;
		$.progress.value = Math.min($.progress.max, $.progress.value + 10);
		$.PROGRESS.delegate("onDataChanged");
		$.STATUS.string = "Button pressed";
	}
	onSwitchChanged(application, value) {
		const $ = this.$;
		$.switch.value = value;
		$.SLIDER.active = value;
		$.SLIDER.delegate("onDataChanged");
		$.STATUS.string = value ? "Controls enabled" : "Slider disabled";
	}
	onSliderChanging(application, value) {
		const $ = this.$;
		$.VALUE.string = String(Math.round(value));
		$.progress.value = value;
		$.PROGRESS.delegate("onDataChanged");
	}
	onSliderChanged(application, value) {
		this.$.STATUS.string = `Slider ${Math.round(value)}`;
	}
}

const WidgetsApplication = Application.template($ => ({
	skin: backgroundSkin,
	Behavior: DemoBehavior,
	contents: [
		Label($, {
			left: 20, right: 20, top: 16, height: 28,
			style: titleStyle,
			string: "MCU Widgets",
		}),
		Button($, {
			left: 20, right: 20, top: 52, height: 48,
			string: "Add 10%",
		}),
		Label($, {
			left: 20, top: 116, height: 28,
			style: titleStyle,
			string: "Switch",
		}),
		Switch($.switch, {
			right: 20, top: 112,
		}),
		Label($, {
			left: 20, top: 156, height: 28,
			style: titleStyle,
			string: "Slider",
		}),
		Label($, {
			anchor: "VALUE",
			right: 20, top: 156, height: 28, width: 48,
			style: valueStyle,
			string: String(Math.round($.slider.value)),
		}),
		Slider($.slider, {
			anchor: "SLIDER",
			left: 20, right: 20, top: 188,
		}),
		Label($, {
			left: 20, top: 232, height: 28,
			style: titleStyle,
			string: "Progress",
		}),
		ProgressBar($.progress, {
			anchor: "PROGRESS",
			left: 20, right: 20, top: 264,
		}),
		Label($, {
			anchor: "STATUS",
			left: 20, right: 20, bottom: 16, height: 28,
			style: titleStyle,
			string: "Ready",
		}),
	],
}));

const model = {
	switch: { value: true },
	slider: { min: 0, max: 100, value: 35, step: 1 },
	progress: { min: 0, max: 100, value: 35 },
};

export default new WidgetsApplication(model, {
	displayListLength: 4096,
	touchCount: 1,
});
