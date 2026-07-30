/*
	Port path API demo: Canvas-like beginPath/fill/stroke via PathPort.
*/
import {} from "piu/MC";
import { PathPort } from "piu/PortPath";

const backgroundSkin = new Skin({ fill: "#102027" });
const labelStyle = new Style({ font: "20px Open Sans", color: "#D7E2E8", horizontal: "center" });

class PathPortBehavior extends Behavior {
	onCreate(port) {
		port.duration = 4000;
		port.loop = true;
		port.start();
	}
	onDraw(port, x, y, w, h) {
		const cx = port.width >> 1;
		const cy = 110;
		const t = port.fraction * Math.PI * 2;

		port.fillColor("#102027", x, y, w, h);

		port.fillRoundRect({
			x0: 20, y0: 20, x1: 220, y1: 100,
			stops: [
				{ offset: 0, r: 45, g: 62, b: 72 },
				{ offset: 1, r: 28, g: 40, b: 48 },
			],
		}, 16, 16, port.width - 32, 188, 18);

		port.strokeRoundRect("#3C5360", 16, 16, port.width - 32, 188, 18, 2);

		port.beginPath();
		port.arc(cx, cy, 54, 0, Math.PI * 2);
		port.fillStyle = "#1A2A32";
		port.fill();

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

		port.fillRoundRect("#1A73E8", 28, 220, port.width - 56, 44, 14);
		port.beginPath();
		port.moveTo(48, 242);
		port.bezierCurveTo(80, 228, 120, 256, 160, 242);
		port.quadraticCurveTo(180, 234, 200, 246);
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
		Label($, {
			left: 0, right: 0, top: 8, height: 28,
			style: labelStyle,
			string: "Port Path API",
		}),
		PathPort($, {
			left: 0, right: 0, top: 36, bottom: 0,
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
