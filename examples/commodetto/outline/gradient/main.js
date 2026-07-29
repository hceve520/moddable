/*
	Linear gradient fill for Outline shapes (Poco).
*/
import Poco from "commodetto/Poco";
import {Outline} from "commodetto/outline";

const poco = new Poco(screen);
const black = poco.makeColor(0, 0, 0);

poco.begin();
	poco.fillRectangle(black, 0, 0, poco.width, poco.height);

	const buttonPath = Outline.RoundRectPath(0, 0, 200, 60, 15);
	const buttonOutline = Outline.fill(buttonPath);
	const buttonGradient = poco.makeLinearGradient(0, 0, 0, 60, [
		{ offset: 0, r: 0, g: 123, b: 255 },
		{ offset: 1, r: 0, g: 105, b: 217 },
	]);
	poco.blendOutline(buttonGradient, 255, buttonOutline, 20, 20);

	const ovalPath = new Outline.CanvasPath();
	ovalPath.ellipse(60, 60, 60, 40, 0, 0, 2 * Math.PI);
	ovalPath.closePath();
	const ovalOutline = Outline.fill(ovalPath);
	const ovalGradient = poco.makeLinearGradient(0, 20, 120, 100, [
		{ offset: 0, r: 255, g: 80, b: 80 },
		{ offset: 0.5, r: 255, g: 200, b: 60 },
		{ offset: 1, r: 80, g: 200, b: 120 },
	]);
	poco.blendOutline(ovalGradient, 255, ovalOutline, 20, 100);

	const dialPath = new Outline.CanvasPath();
	dialPath.arc(50, 50, 48, 0, 2 * Math.PI);
	dialPath.closePath();
	const dialOutline = Outline.fill(dialPath);
	const dialGradient = {
		x0: 10, y0: 10, x1: 90, y1: 90,
		stops: [
			{ offset: 0, color: poco.makeColor(40, 40, 120) },
			{ offset: 1, color: poco.makeColor(180, 220, 255) },
		],
	};
	poco.blendOutline(dialGradient, 255, dialOutline, 160, 100);
poco.end();
