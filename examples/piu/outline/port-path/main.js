/*
	Port path API demo: Canvas-like beginPath/fill/stroke via PathPort.
*/
import {} from "piu/MC";
import { PathPort } from "piu/PortPath";

const backgroundSkin = new Skin({ fill: "#102027" });

class PathPortBehavior extends Behavior {
	onDisplaying(port) {
		port.duration = 4000;
		port.loop = true;
		port.time = 0;
		port.start();
	}
	onDraw(port, x, y, w, h) {
		const cx = port.width >> 1;
		const cy = 120;
		const t = Math.max(0.02, port.fraction * Math.PI * 2);

		port.fillColor("#102027", x, y, w, h);

		port.fillRoundRect({
			x0: 20, y0: 36, x1: 220, y1: 220,
			stops: [
				{ offset: 0, r: 45, g: 62, b: 72 },
				{ offset: 1, r: 28, g: 40, b: 48 },
			],
		}, 16, 36, port.width - 32, 188, 18);
		port.strokeRoundRect("#3C5360", 16, 36, port.width - 32, 188, 18, 2);

		// track
		port.beginPath();
		port.arc(cx, cy, 48, 0, Math.PI * 2);
		port.strokeStyle = "#152028";
		port.lineWidth = 10;
		port.stroke();

		// progress arc
		port.beginPath();
		port.arc(cx, cy, 48, -Math.PI * 0.5, -Math.PI * 0.5 + t);
		port.strokeStyle = {
			x0: cx - 48, y0: cy, x1: cx + 48, y1: cy,
			stops: [
				{ offset: 0, r: 66, g: 133, b: 244 },
				{ offset: 1, r: 26, g: 115, b: 232 },
			],
		};
		port.lineWidth = 10;
		port.lineCap = PathPort.Outline.LINECAP_ROUND;
		port.stroke();

		// needle
		port.beginPath();
		port.moveTo(cx, cy);
		port.lineTo(
			cx + Math.cos(t - Math.PI * 0.5) * 36,
			cy + Math.sin(t - Math.PI * 0.5) * 36
		);
		port.strokeStyle = "white";
		port.lineWidth = 3;
		port.stroke();

		port.beginPath();
		port.arc(cx, cy, 5, 0, Math.PI * 2);
		port.fillStyle = "white";
		port.fill();

		port.fillRoundRect("#1A73E8", 28, 244, port.width - 56, 44, 14);
		port.beginPath();
		port.moveTo(48, 266);
		port.bezierCurveTo(80, 252, 120, 280, 160, 266);
		port.quadraticCurveTo(180, 258, 200, 270);
		port.strokeStyle = "white";
		port.lineWidth = 3;
		port.stroke();
	}
	onTimeChanged(port) {
		port.invalidate();
	}
}

const PortPathApplication = Application.template($ => ({
	skin: backgroundSkin,
	contents: [
		PathPort($, {
			left: 0, right: 0, top: 0, bottom: 0,
			Behavior: PathPortBehavior,
		}),
	],
}));

export default new PortPathApplication(null, {
	commandListLength: 4096,
	displayListLength: 16384,
	touchCount: 0,
	pixels: 240 * 64,
});
