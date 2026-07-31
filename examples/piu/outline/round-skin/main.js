/*
	Rounded + gradient Skin panels via RoundContent.
*/
import {} from "piu/MC";
import {} from "piu/RoundContent";

const backgroundSkin = new Skin({ fill: "#102027" });
const labelStyle = new Style({ font: "24px Open Sans", color: "white", horizontal: "center", vertical: "middle" });
const bodyStyle = new Style({ font: "20px Open Sans", color: "#D7E2E8", horizontal: "left", vertical: "middle" });

const solidSkin = new Skin({
	fill: ["#1B7A6E", "#249F90"],
	stroke: "#0E4F47",
	radius: 18,
});

const gradientSkin = new Skin({
	fill: "#1A73E8",
	stroke: "#0B57D0",
	radius: 20,
	fillGradient: {
		x0: 0, y0: 0, x1: 0, y1: 64,
		stops: [
			{ offset: 0, r: 66, g: 133, b: 244 },
			{ offset: 1, r: 26, g: 115, b: 232 },
		],
	},
});

const cardSkin = new Skin({
	fill: "#24343C",
	stroke: "#3C5360",
	radius: 16,
	fillGradient: {
		x0: 0, y0: 0, x1: 280, y1: 120,
		stops: [
			{ offset: 0, r: 45, g: 62, b: 72 },
			{ offset: 1, r: 28, g: 40, b: 48 },
		],
	},
});

class ButtonBehavior extends Behavior {
	onCreate(content) {
		content.state = 0;
	}
	onTouchBegan(content) {
		content.state = 1;
	}
	onTouchEnded(content) {
		content.state = 0;
	}
	onTouchCancelled(content) {
		content.state = 0;
	}
}

const RoundSkinApplication = Application.template($ => ({
	skin: backgroundSkin,
	contents: [
		RoundContent($, {
			left: 20, right: 20, top: 24, height: 64,
			border: 2,
			skin: solidSkin,
			active: true,
			Behavior: ButtonBehavior,
			contents: [
				Label($, {
					left: 0, right: 0, top: 0, bottom: 0,
					style: labelStyle,
					string: "Rounded Skin",
				}),
			],
		}),
		RoundContent($, {
			left: 20, right: 20, top: 104, height: 64,
			border: 2,
			skin: gradientSkin,
			active: true,
			Behavior: ButtonBehavior,
			contents: [
				Label($, {
					left: 0, right: 0, top: 0, bottom: 0,
					style: labelStyle,
					string: "Gradient Skin",
				}),
			],
		}),
		RoundContent($, {
			left: 20, right: 20, top: 188, height: 120,
			border: 1,
			skin: cardSkin,
			contents: [
				Label($, {
					left: 16, right: 16, top: 0, bottom: 0,
					style: bodyStyle,
					string: "RoundContent reads radius and fillGradient from Skin, and can hold children.",
				}),
			],
		}),
	],
}));

export default new RoundSkinApplication(null, { displayListLength: 2048, touchCount: 1 });
