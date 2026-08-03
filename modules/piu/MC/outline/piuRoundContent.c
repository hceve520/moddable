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

typedef struct PiuRoundContentStruct PiuRoundContentRecord, *PiuRoundContent;
struct PiuRoundContentStruct {
	PiuHandlePart;
	PiuIdlePart;
	PiuBehaviorPart;
	PiuContentPart;
	PiuContainerPart;

	PiuDimension border;
	PiuDimension radius;
	void* fillGradient;
	void* strokeGradient;

	PiuDimension cacheWidth;
	PiuDimension cacheHeight;
	PiuDimension cacheRadius;
	PiuDimension cacheBorder;
	void* outerOutline;
	void* innerOutline;

	PocoColor fillColor;
	uint8_t fillBlend;
	PocoColor strokeColor;
	uint8_t strokeBlend;
};

extern void PocoLinearGradientFromSlot(xsMachine *the, xsSlot *slot, PocoLinearGradient gradient);

static void PiuRoundContentDictionary(xsMachine* the, void* it);
static void PiuRoundContentDraw(void* it, PiuView* view, PiuRectangle area);
static void PiuRoundContentDrawAux(void* it, PiuView* view, PiuCoordinate x, PiuCoordinate y, PiuDimension sw, PiuDimension sh);
static void PiuRoundContentEnsureOutlines(PiuRoundContent* self);
static void PiuRoundContentMark(xsMachine* the, void* it, xsMarkRoot markRoot);
static PiuDimension PiuRoundContentResolvedRadius(PiuRoundContent* self);
static xsSlot* PiuRoundContentResolvedFillGradient(PiuRoundContent* self);
static xsSlot* PiuRoundContentResolvedStrokeGradient(PiuRoundContent* self);

const PiuDispatchRecord ICACHE_FLASH_ATTR PiuRoundContentDispatchRecord = {
	"RoundContent",
	PiuContainerBind,
	PiuContainerCascade,
	PiuRoundContentDraw,
	PiuContainerFitHorizontally,
	PiuContainerFitVertically,
	PiuContainerHit,
	PiuContentIdle,
	PiuContainerInvalidate,
	PiuContainerMeasureHorizontally,
	PiuContainerMeasureVertically,
	PiuContainerPlace,
	PiuContainerPlaceContentHorizontally,
	PiuContainerPlaceContentVertically,
	PiuContainerReflow,
	PiuContainerShowing,
	PiuContainerShown,
	PiuContentSync,
	PiuContainerUnbind,
	PiuContainerUpdate
};

const xsHostHooks ICACHE_FLASH_ATTR PiuRoundContentHooks = {
	PiuContentDelete,
	PiuRoundContentMark,
	NULL
};

static PiuDimension PiuRoundContentResolvedRadius(PiuRoundContent* self)
{
	PiuDimension radius = (*self)->radius;
	if (!radius) {
		PiuSkin* skin = (*self)->skin;
		if (skin && !((*skin)->flags & piuSkinPattern))
			radius = (*skin)->radius;
	}
	return radius;
}

static xsSlot* PiuRoundContentResolvedFillGradient(PiuRoundContent* self)
{
	if ((*self)->fillGradient)
		return (*self)->fillGradient;
	if ((*self)->skin && !((*(*self)->skin)->flags & piuSkinPattern))
		return (*(*self)->skin)->fillGradient;
	return NULL;
}

static xsSlot* PiuRoundContentResolvedStrokeGradient(PiuRoundContent* self)
{
	if ((*self)->strokeGradient)
		return (*self)->strokeGradient;
	if ((*self)->skin && !((*(*self)->skin)->flags & piuSkinPattern))
		return (*(*self)->skin)->strokeGradient;
	return NULL;
}

static void PiuRoundContentEnsureOutlines(PiuRoundContent* self)
{
	PiuDimension width = (*self)->bounds.width;
	PiuDimension height = (*self)->bounds.height;
	PiuDimension radius = PiuRoundContentResolvedRadius(self);
	PiuDimension border = (*self)->border;
	xsMachine* the = (*self)->the;
	PiuDimension maxRadius;

	if (!width || !height)
		return;

	maxRadius = (width < height) ? (width >> 1) : (height >> 1);
	if (radius > maxRadius)
		radius = maxRadius;
	if (border) {
		PiuDimension maxBorder = maxRadius;
		if (border > maxBorder)
			border = maxBorder;
	}

	/* Reuse fill/stroke Outline geometry while size, radius, and border are unchanged. */
	if (((*self)->cacheWidth == width)
			&& ((*self)->cacheHeight == height)
			&& ((*self)->cacheRadius == radius)
			&& ((*self)->cacheBorder == border)
			&& (*self)->outerOutline)
		return;

	(*self)->outerOutline = NULL;
	(*self)->innerOutline = NULL;
	(*self)->cacheWidth = 0;
	(*self)->cacheHeight = 0;
	(*self)->cacheRadius = 0;
	(*self)->cacheBorder = 0;

	xsBeginHost(the);
	xsVars(5);
	xsVar(0) = xsGet(xsGlobal, xsID_Shape);
	xsVar(1) = xsGet(xsVar(0), xsID_Outline);

	/*
	 * With a border:
	 *   outerOutline = even-odd (outer − inner) annulus for the stroke color
	 *   innerOutline = filled inner round-rect for optional fill color
	 * Without a border:
	 *   outerOutline = filled round-rect (legacy)
	 *
	 * When skin has stroke but no fill (fillBlend == 0), only the annulus is
	 * drawn — a true hollow frame that reveals content underneath.
	 */
	if (border && (width > (PiuDimension)(border << 1)) && (height > (PiuDimension)(border << 1))) {
		PiuDimension innerRadius = (radius > border) ? (PiuDimension)(radius - border) : 0;

		xsVar(2) = xsCall5(xsVar(1), xsID_RoundRectPath,
				xsInteger(0), xsInteger(0), xsInteger(width), xsInteger(height), xsInteger(radius));
		xsVar(3) = xsCall5(xsVar(1), xsID_RoundRectPath,
				xsInteger(border), xsInteger(border),
				xsInteger(width - (border << 1)), xsInteger(height - (border << 1)),
				xsInteger(innerRadius));
		xsVar(4) = xsCall1(xsVar(2), xsID_concat, xsVar(3));
		/* Outline.EVEN_ODD_RULE == 2 */
		xsResult = xsCall2(xsVar(1), xsID_fill, xsVar(4), xsInteger(2));
		(*self)->outerOutline = xsToReference(xsResult);

		xsResult = xsCall1(xsVar(1), xsID_fill, xsVar(3));
		(*self)->innerOutline = xsToReference(xsResult);
	}
	else {
		xsResult = xsCall5(xsVar(1), xsID_RoundRectPath,
				xsInteger(0), xsInteger(0), xsInteger(width), xsInteger(height), xsInteger(radius));
		xsVar(2) = xsCall1(xsVar(1), xsID_fill, xsResult);
		(*self)->outerOutline = xsToReference(xsVar(2));
		(*self)->innerOutline = NULL;
	}

	xsEndHost(the);

	(*self)->cacheWidth = width;
	(*self)->cacheHeight = height;
	(*self)->cacheRadius = radius;
	(*self)->cacheBorder = border;
}

void PiuRoundContentDictionary(xsMachine* the, void* it)
{
	PiuRoundContent* self = it;
	xsIntegerValue integer;
	if (xsFindInteger(xsArg(1), xsID_border, &integer)) {
		if (integer < 0)
			integer = 0;
		(*self)->border = (PiuDimension)integer;
	}
	if (xsFindInteger(xsArg(1), xsID_radius, &integer)) {
		if (integer < 0)
			integer = 0;
		(*self)->radius = (PiuDimension)integer;
	}
	if (xsFindResult(xsArg(1), xsID_fillGradient)) {
		if (xsTest(xsResult))
			(*self)->fillGradient = xsToReference(xsResult);
	}
	if (xsFindResult(xsArg(1), xsID_strokeGradient)) {
		if (xsTest(xsResult))
			(*self)->strokeGradient = xsToReference(xsResult);
	}
}

void PiuRoundContentDraw(void* it, PiuView* view, PiuRectangle area)
{
	PiuRoundContent* self = it;
	PiuSkin* skin = (*self)->skin;
	xsSlot* fillGradient = PiuRoundContentResolvedFillGradient(self);
	xsSlot* strokeGradient = PiuRoundContentResolvedStrokeGradient(self);
	PiuDimension radius = PiuRoundContentResolvedRadius(self);
	PiuColorRecord color;
	PiuState state = (*self)->state;
	uint8_t canDraw;

	if (state < 0) state = 0;
	else if (3 < state) state = 3;

	canDraw = (skin || fillGradient || strokeGradient) && (radius || (*self)->border || fillGradient || strokeGradient);
	if (!canDraw) {
		/* No rounded/gradient styling: fall back to ordinary rectangular skin. */
		if (skin)
			PiuContentDraw(it, view, area);
		return;
	}

	if (!(*self)->bounds.width || !(*self)->bounds.height)
		return;

	PiuRoundContentEnsureOutlines(self);
	if (!(*self)->outerOutline)
		return;

	(*self)->fillBlend = 255;
	(*self)->strokeBlend = 255;
	(*self)->fillColor = 0;
	(*self)->strokeColor = 0;

	if (skin) {
		PiuColorsBlend((*skin)->data.color.fill, state, &color);
		(*self)->fillColor = PocoMakeColor((*view)->poco, color.r, color.g, color.b);
		(*self)->fillBlend = color.a;
		PiuColorsBlend((*skin)->data.color.stroke, state, &color);
		(*self)->strokeColor = PocoMakeColor((*view)->poco, color.r, color.g, color.b);
		(*self)->strokeBlend = color.a;
	}

	PiuViewDrawContent(view, PiuRoundContentDrawAux, it, 0, 0, (*self)->bounds.width, (*self)->bounds.height);
}

void PiuRoundContentDrawAux(void* it, PiuView* view, PiuCoordinate x, PiuCoordinate y, PiuDimension sw, PiuDimension sh)
{
	PiuRoundContent* self = it;
	PocoOutline outline;
	xsSlot* fillGradient = PiuRoundContentResolvedFillGradient(self);
	xsSlot* strokeGradient = PiuRoundContentResolvedStrokeGradient(self);
	PiuSkin* skin = (*self)->skin;

	xsBeginHost((*self)->the);
	if ((*self)->innerOutline && ((*self)->cacheBorder > 0) && (skin || strokeGradient || fillGradient)) {
		/* Annulus (even-odd) — hollow when fillBlend is 0 / no fillGradient */
		if (skin || strokeGradient) {
			xsResult = xsReference((*self)->outerOutline);
			outline = xsGetHostData(xsResult);
			if (strokeGradient) {
				PocoLinearGradientRecord gradient;
				xsResult = xsReference(strokeGradient);
				PocoLinearGradientFromSlot((*self)->the, &xsResult, &gradient);
				PocoOutlineFillGradient((*view)->poco, &gradient, (*self)->strokeBlend, outline, x, y);
			}
			else if ((*self)->strokeBlend)
				PocoOutlineFill((*view)->poco, (*self)->strokeColor, (*self)->strokeBlend, outline, x, y);
		}

		/* Optional opaque interior */
		if (fillGradient || (skin && (*self)->fillBlend)) {
			xsResult = xsReference((*self)->innerOutline);
			outline = xsGetHostData(xsResult);
			if (fillGradient) {
				PocoLinearGradientRecord gradient;
				xsResult = xsReference(fillGradient);
				PocoLinearGradientFromSlot((*self)->the, &xsResult, &gradient);
				PocoOutlineFillGradient((*view)->poco, &gradient, (*self)->fillBlend, outline, x, y);
			}
			else
				PocoOutlineFill((*view)->poco, (*self)->fillColor, (*self)->fillBlend, outline, x, y);
		}
	}
	else if ((*self)->outerOutline && (skin || fillGradient)) {
		xsResult = xsReference((*self)->outerOutline);
		outline = xsGetHostData(xsResult);
		if (fillGradient) {
			PocoLinearGradientRecord gradient;
			xsResult = xsReference(fillGradient);
			PocoLinearGradientFromSlot((*self)->the, &xsResult, &gradient);
			PocoOutlineFillGradient((*view)->poco, &gradient, (*self)->fillBlend, outline, x, y);
		}
		else if (skin && (*self)->fillBlend)
			PocoOutlineFill((*view)->poco, (*self)->fillColor, (*self)->fillBlend, outline, x, y);
	}
	xsEndHost((*self)->the);
}

void PiuRoundContentMark(xsMachine* the, void* it, xsMarkRoot markRoot)
{
	PiuRoundContent self = it;
	PiuContainerMark(the, it, markRoot);
	PiuMarkReference(the, self->fillGradient);
	PiuMarkReference(the, self->strokeGradient);
	PiuMarkReference(the, self->outerOutline);
	PiuMarkReference(the, self->innerOutline);
}

void PiuRoundContent_create(xsMachine* the)
{
	PiuRoundContent* self;
	xsVars(4);
	xsSetHostChunk(xsThis, NULL, sizeof(PiuRoundContentRecord));
	self = PIU(RoundContent, xsThis);
	(*self)->the = the;
	(*self)->reference = xsToReference(xsThis);
	xsSetHostHooks(xsThis, (xsHostHooks*)&PiuRoundContentHooks);
	(*self)->dispatch = (PiuDispatch)&PiuRoundContentDispatchRecord;
	(*self)->recordSize = PiuRecordSize(sizeof(PiuRoundContentRecord));
	(*self)->flags = piuVisible | piuContainer | piuClip;
	PiuContentDictionary(the, self);
	PiuContainerDictionary(the, self);
	PiuRoundContentDictionary(the, self);
	PiuBehaviorOnCreate(self);
}

void PiuRoundContent_get_border(xsMachine* the)
{
	PiuRoundContent* self = PIU(RoundContent, xsThis);
	xsResult = xsPiuDimension((*self)->border);
}

void PiuRoundContent_get_radius(xsMachine* the)
{
	PiuRoundContent* self = PIU(RoundContent, xsThis);
	xsResult = xsPiuDimension(PiuRoundContentResolvedRadius(self));
}

void PiuRoundContent_get_fillGradient(xsMachine* the)
{
	PiuRoundContent* self = PIU(RoundContent, xsThis);
	xsSlot* fillGradient = (*self)->fillGradient;
	if (fillGradient)
		xsResult = xsReference(fillGradient);
}

void PiuRoundContent_get_strokeGradient(xsMachine* the)
{
	PiuRoundContent* self = PIU(RoundContent, xsThis);
	xsSlot* strokeGradient = (*self)->strokeGradient;
	if (strokeGradient)
		xsResult = xsReference(strokeGradient);
}

void PiuRoundContent_set_border(xsMachine *the)
{
	PiuRoundContent* self = PIU(RoundContent, xsThis);
	xsIntegerValue integer = xsToInteger(xsArg(0));
	if (integer < 0)
		integer = 0;
	(*self)->border = (PiuDimension)integer;
	(*self)->cacheWidth = 0;
	PiuContentInvalidate(self, NULL);
}

void PiuRoundContent_set_radius(xsMachine *the)
{
	PiuRoundContent* self = PIU(RoundContent, xsThis);
	xsIntegerValue integer = xsToInteger(xsArg(0));
	if (integer < 0)
		integer = 0;
	(*self)->radius = (PiuDimension)integer;
	(*self)->cacheWidth = 0;
	PiuContentInvalidate(self, NULL);
}

void PiuRoundContent_set_fillGradient(xsMachine *the)
{
	PiuRoundContent* self = PIU(RoundContent, xsThis);
	if (xsTest(xsArg(0)))
		(*self)->fillGradient = xsToReference(xsArg(0));
	else
		(*self)->fillGradient = NULL;
	PiuContentInvalidate(self, NULL);
}

void PiuRoundContent_set_strokeGradient(xsMachine *the)
{
	PiuRoundContent* self = PIU(RoundContent, xsThis);
	if (xsTest(xsArg(0)))
		(*self)->strokeGradient = xsToReference(xsArg(0));
	else
		(*self)->strokeGradient = NULL;
	PiuContentInvalidate(self, NULL);
}
