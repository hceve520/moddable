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

#include "piuMC.h"
#include "commodettoPocoOutline.h"

typedef struct PiuPortOutlineOpStruct PiuPortOutlineOpRecord, *PiuPortOutlineOp;
struct PiuPortOutlineOpStruct {
	xsSlot* reference;
	xsSlot* outline;
	xsSlot* gradient;
	PocoColor color;
	uint8_t blend;
	uint8_t kind;
};

enum {
	kPiuPortOutlineSolid = 0,
	kPiuPortOutlineGradient = 1
};

extern void PocoLinearGradientFromSlot(xsMachine *the, xsSlot *slot, PocoLinearGradient gradient);

static void PiuPortOutlineOpDrawAux(void* it, PiuView* view, PiuCoordinate x, PiuCoordinate y, PiuDimension sw, PiuDimension sh);
static void PiuPortOutlineOpMark(xsMachine* the, void* it, xsMarkRoot markRoot);

static const xsHostHooks ICACHE_FLASH_ATTR PiuPortOutlineOpHooks = {
	NULL,
	PiuPortOutlineOpMark,
	NULL
};

static void PiuPortOutlineOpMark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	PiuPortOutlineOp self = it;
	PiuMarkReference(the, self->outline);
	PiuMarkReference(the, self->gradient);
}

static void PiuPortOutlineOpDrawAux(void* it, PiuView* view, PiuCoordinate x, PiuCoordinate y, PiuDimension sw, PiuDimension sh)
{
	PiuPortOutlineOp* self = it;
	PocoOutline outline;

	xsBeginHost((*view)->the);
	xsResult = xsReference((*self)->outline);
	outline = xsGetHostData(xsResult);
	if ((*self)->kind == kPiuPortOutlineGradient) {
		PocoLinearGradientRecord gradient;
		xsResult = xsReference((*self)->gradient);
		PocoLinearGradientFromSlot((*view)->the, &xsResult, &gradient);
		PocoOutlineFillGradient((*view)->poco, &gradient, (*self)->blend, outline, x, y);
	}
	else
		PocoOutlineFill((*view)->poco, (*self)->color, (*self)->blend, outline, x, y);
	xsEndHost((*view)->the);
}

void PiuPort_drawOutline(xsMachine* the)
{
	xsIntegerValue c = xsToInteger(xsArgc);
	PiuPort* port = PIU(Port, xsThis);
	PiuView* view = (*port)->view;
	PiuPortOutlineOp* op;
	PiuCoordinate x = 0, y = 0;
	uint8_t blend = 255;
	xsType paintType;

	if (!view)
		xsUnknownError("out of sequence");
	if (c < 3)
		xsUnknownError("drawOutline(paint, blend, outline)");

	xsVars(1);
	xsVar(0) = xsNewHostObject(NULL);
	xsSetHostChunk(xsVar(0), NULL, sizeof(PiuPortOutlineOpRecord));
	op = PIU(PortOutlineOp, xsVar(0));
	(*op)->reference = xsToReference(xsVar(0));
	xsSetHostHooks(xsVar(0), (xsHostHooks*)&PiuPortOutlineOpHooks);
	(*op)->outline = xsToReference(xsArg(2));
	(*op)->gradient = NULL;
	(*op)->color = 0;
	(*op)->blend = 255;
	(*op)->kind = kPiuPortOutlineSolid;

	if (c > 1)
		blend = (uint8_t)xsToInteger(xsArg(1));

	paintType = xsTypeOf(xsArg(0));
	if ((xsIntegerType == paintType) || (xsNumberType == paintType)) {
		(*op)->kind = kPiuPortOutlineSolid;
		(*op)->color = (PocoColor)xsToInteger(xsArg(0));
		(*op)->blend = blend;
	}
	else if (xsStringType == paintType) {
		PiuColorRecord color;
		PiuColorDictionary(the, &xsArg(0), &color);
		(*op)->kind = kPiuPortOutlineSolid;
		(*op)->color = PocoMakeColor((*view)->poco, color.r, color.g, color.b);
		(*op)->blend = (uint8_t)((blend * color.a) / 255);
	}
	else {
		(*op)->kind = kPiuPortOutlineGradient;
		(*op)->gradient = xsToReference(xsArg(0));
		(*op)->blend = blend;
	}

	if (c > 3)
		x = xsToPiuCoordinate(xsArg(3));
	if (c > 4)
		y = xsToPiuCoordinate(xsArg(4));

	PiuViewDrawContent(view, PiuPortOutlineOpDrawAux, op, x, y, 0, 0);
}
