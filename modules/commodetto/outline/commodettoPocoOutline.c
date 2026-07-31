#include "xsmc.h"
#include "mc.xs.h"
#include "xsHost.h"

#include "mc.defines.h"

#include <math.h>

#include "commodettoPoco.h"
#include "commodettoPocoOutline.h"

#define STANDALONE_ 1
#define FT_BEGIN_HEADER
#define FT_END_HEADER
#include "ftraster.h"
#include "ftgrays.h"

extern void FT_Outline_Get_CBox( const FT_Outline*  outline, FT_BBox *acbox);

typedef struct {
	uint8_t		*bits;
	uint32_t	pitch;
} xsSpanInfoRecord, *xsSpanInfo;

#define kPocoOutlineGradientSlotInitial (16)
#define kPocoOutlineGradientSlotMax (256)
#define kPocoOutlineTurnFull (65536u)	/* one turn ≡ 2π */

typedef struct {
	FT_Raster raster;
	FT_Raster_Params params;
	int pitch;

	/* Gradient records live here for the current display-list frame; commands store slot indices only. */
	PocoLinearGradientRecord *gradientSlots;
	uint16_t gradientSlotCount;
	uint16_t gradientSlotCapacity;
	Poco gradPoco;
	char *gradDisplayList;
	char *gradHighWater;

	uint8_t renderPool[1];
} xsOutlineRendererRecord, *xsOutlineRenderer;

static void doOutline(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h, uint8_t xphase);
static void doPolygon(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h, uint8_t xphase);
static void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user);
static void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user);
static void outlineFillCommon(Poco poco, uint8_t paintKind, PocoColor color, const PocoLinearGradientRecord *gradient, uint8_t blend, PocoOutline pOutline, PocoCoordinate dx, PocoCoordinate dy);
static uint16_t outlineAllocGradientSlot(Poco poco, xsOutlineRenderer or, const PocoLinearGradientRecord *gradient);

void PocoLinearGradientFromSlot(xsMachine *the, xsSlot *slot, PocoLinearGradient gradient);

static xsOutlineRenderer gxOutlineRenderer = NULL;

static uint32_t gOutlineFillCount = 0;
static uint32_t gOutlineGradientCount = 0;
static uint32_t gOutlineGradientSlotsPeak = 0;

xsOutlineRenderer PocoOutlineRenderer()
{
	if (gxOutlineRenderer == NULL) {
		xsOutlineRenderer or = c_calloc(1, sizeof(xsOutlineRendererRecord));
		or->params.flags = FT_RASTER_FLAG_AA | FT_RASTER_FLAG_DIRECT | FT_RASTER_FLAG_CLIP;
		ft_grays_raster.raster_new(NULL, &or->raster);
		ft_grays_raster.raster_reset(or->raster, or->renderPool, sizeof(or->renderPool));
		gxOutlineRenderer = or;
	}
	return gxOutlineRenderer;
}

typedef struct {
	xsOutlineRenderer or;
	struct FT_Outline_ outline;
	PocoCoordinate x;
	PocoCoordinate y;
	PocoCoordinate dx;
	PocoPixel color;
	uint8_t blend;
	uint8_t paintKind;
	uint16_t gradientSlot;	/* index into or->gradientSlots when paintKind is gradient */
} xsOutlineRenderRecord, *xsOutlineRender;

#if defined(__GNUC__) || defined(__clang__)
_Static_assert(sizeof(xsOutlineRenderRecord) <= 255, "outline render record exceeds PocoDrawExternal limit");
#endif

void PocoOutlineStatsGet(uint32_t *fills, uint32_t *gradients, uint32_t *slotsUsed, uint32_t *slotsPeak)
{
	xsOutlineRenderer or = gxOutlineRenderer;
	if (fills)
		*fills = gOutlineFillCount;
	if (gradients)
		*gradients = gOutlineGradientCount;
	if (slotsUsed)
		*slotsUsed = or ? or->gradientSlotCount : 0;
	if (slotsPeak)
		*slotsPeak = gOutlineGradientSlotsPeak;
}

void PocoOutlineStatsReset(void)
{
	gOutlineFillCount = 0;
	gOutlineGradientCount = 0;
	gOutlineGradientSlotsPeak = 0;
}

/*
	Reset gradient slots when the display list is rewound (new PocoDrawingBegin)
	or when the Poco instance / list buffer identity changes.
*/
static void outlineSyncGradientFrame(Poco poco, xsOutlineRenderer or)
{
	char *next = (char *)poco->next;

	if ((or->gradPoco != poco) || (or->gradDisplayList != poco->displayList)) {
		or->gradPoco = poco;
		or->gradDisplayList = poco->displayList;
		or->gradientSlotCount = 0;
		or->gradHighWater = next;
		return;
	}

	if ((NULL != or->gradHighWater) && ((next < or->gradHighWater) || (next == poco->displayList)))
		or->gradientSlotCount = 0;

	if (next > or->gradHighWater)
		or->gradHighWater = next;
}

static uint16_t outlineAllocGradientSlot(Poco poco, xsOutlineRenderer or, const PocoLinearGradientRecord *gradient)
{
	uint16_t index;

	outlineSyncGradientFrame(poco, or);

	if (or->gradientSlotCount >= or->gradientSlotCapacity) {
		uint16_t capacity = or->gradientSlotCapacity ? (uint16_t)(or->gradientSlotCapacity << 1) : kPocoOutlineGradientSlotInitial;
		PocoLinearGradientRecord *slots;

		if (capacity > kPocoOutlineGradientSlotMax)
			capacity = kPocoOutlineGradientSlotMax;
		if (or->gradientSlotCount >= capacity) {
			/* Exhausted — reuse last slot (last writer wins). Prefer larger displayListLength over silent corruption. */
			index = (uint16_t)(capacity - 1);
			or->gradientSlots[index] = *gradient;
			return index;
		}
		slots = c_realloc(or->gradientSlots, capacity * sizeof(PocoLinearGradientRecord));
		if (!slots) {
			static PocoLinearGradientRecord emergency;
			emergency = *gradient;
			if (!or->gradientSlots) {
				or->gradientSlots = &emergency;
				or->gradientSlotCapacity = kPocoOutlineGradientSlotMax; /* avoid realloc of static */
				or->gradientSlotCount = 1;
				return 0;
			}
			or->gradientSlots[0] = *gradient;
			return 0;
		}
		or->gradientSlots = slots;
		or->gradientSlotCapacity = capacity;
	}

	index = or->gradientSlotCount++;
	or->gradientSlots[index] = *gradient;
	if (or->gradientSlotCount > gOutlineGradientSlotsPeak)
		gOutlineGradientSlotsPeak = or->gradientSlotCount;
	return index;
}
void bufferToFTOutline(void *buffer, struct FT_Outline_ *outline)
{
	PocoOutline header = (PocoOutline)buffer;
	outline->n_points = header->n_points;
	outline->n_contours = header->n_contours;
	outline->points = (struct FT_Vector_ *)(((unsigned char *)buffer) + sizeof(PocoOutlineRecord));
	outline->contours = (short *)(((unsigned char *)outline->points) + ((outline->n_points << 1) * 4));
	outline->tags = ((char *)outline->contours) + (outline->n_contours * 2);
	outline->flags = header->flags;
}

#ifndef M_PI_F
	#define M_PI_F 3.14159265f
#endif

void PocoLinearGradientPrepare(PocoLinearGradient gradient)
{
	gradient->len2Scale = 0;
	gradient->invSweepScale = 0;

	if (gradient->flags & kPocoGradientFlagAngular) {
		gradient->adx = 0;
		gradient->ady = 0;
		gradient->len2 = 1;	/* non-zero so single-stop short-circuit still works via stopCount */
		if (gradient->sweepTurn)
			gradient->invSweepScale = (255u << 16) / gradient->sweepTurn;
		return;
	}

	gradient->adx = (int32_t)gradient->x1 - (int32_t)gradient->x0;
	gradient->ady = (int32_t)gradient->y1 - (int32_t)gradient->y0;
	gradient->len2 = (uint32_t)(gradient->adx * gradient->adx + gradient->ady * gradient->ady);
	gradient->flags &= (uint8_t)~(kPocoGradientFlagVertical | kPocoGradientFlagHorizontal);
	if (0 == gradient->adx)
		gradient->flags |= kPocoGradientFlagVertical;
	if (0 == gradient->ady)
		gradient->flags |= kPocoGradientFlagHorizontal;
	if (gradient->len2)
		gradient->len2Scale = (uint32_t)(((uint64_t)255 << 24) / gradient->len2);
}

static void PocoAngularSetAngles(PocoLinearGradient gradient, float start, float sweep)
{
	float twoPi = 2.0f * M_PI_F;
	uint32_t startTurn, sweepTurn;

	if (sweep < 0)
		sweep = -sweep;
	while (start < 0)
		start += twoPi;
	while (start >= twoPi)
		start -= twoPi;

	startTurn = (uint32_t)(start * ((float)kPocoOutlineTurnFull / twoPi) + 0.5f);
	if (startTurn >= kPocoOutlineTurnFull)
		startTurn = 0;
	sweepTurn = (uint32_t)(sweep * ((float)kPocoOutlineTurnFull / twoPi) + 0.5f);
	if (sweepTurn > kPocoOutlineTurnFull)
		sweepTurn = kPocoOutlineTurnFull;

	gradient->startTurn = startTurn;
	gradient->sweepTurn = sweepTurn;
}

/*
	Integer atan2 → turn units [0, 65536). First-octant uses a linear fraction
	(adequate for ring-gauge shading; avoids soft-float atan2 per pixel).
*/
static uint32_t pocoAtan2Turn(int32_t y, int32_t x)
{
	uint32_t ax, ay, angle;

	if ((0 == x) && (0 == y))
		return 0;

	ax = (uint32_t)((x < 0) ? -x : x);
	ay = (uint32_t)((y < 0) ? -y : y);

	if (ax >= ay) {
		/* 0..45° → 0..8192 */
		angle = ax ? ((ay << 13) / ax) : 0;
	}
	else {
		/* 45..90° → 8192..16384 */
		angle = 16384u - (ay ? ((ax << 13) / ay) : 0);
	}

	if (x < 0)
		angle = 32768u - angle;
	if (y < 0)
		angle = kPocoOutlineTurnFull - angle;
	if (angle >= kPocoOutlineTurnFull)
		angle -= kPocoOutlineTurnFull;
	return angle;
}

/* Coarser 8-way angular sample for secondary gauges (flag AngularFast). */
static uint32_t pocoAtan2TurnFast(int32_t y, int32_t x)
{
	uint32_t ax, ay;

	if ((0 == x) && (0 == y))
		return 0;
	ax = (uint32_t)((x < 0) ? -x : x);
	ay = (uint32_t)((y < 0) ? -y : y);

	/* Quantize to octant centers: 0,45,90,... */
	if (ay * 2 < ax) {
		/* near 0° or 180° */
		return (x >= 0) ? 0 : 32768u;
	}
	if (ax * 2 < ay) {
		/* near 90° or 270° */
		return (y >= 0) ? 16384u : 49152u;
	}
	if ((x >= 0) && (y >= 0))
		return 8192u;
	if ((x < 0) && (y >= 0))
		return 24576u;
	if ((x < 0) && (y < 0))
		return 40960u;
	return 57344u;
}

static uint32_t PocoLinearGradientTFromNum(const PocoLinearGradientRecord *gradient, int64_t num);

uint8_t PocoLinearGradientSampleT(const PocoLinearGradientRecord *gradient, int x, int y)
{
	if (0 == gradient->stopCount)
		return 0;
	if (1 == gradient->stopCount)
		return 0;

	if (gradient->flags & kPocoGradientFlagAngular) {
		int32_t dx = (int32_t)x - (int32_t)gradient->x0;
		int32_t dy = (int32_t)y - (int32_t)gradient->y0;
		uint32_t ang = (gradient->flags & kPocoGradientFlagAngularFast)
				? pocoAtan2TurnFast(dy, dx)
				: pocoAtan2Turn(dy, dx);
		uint32_t rel = (ang - gradient->startTurn) & (kPocoOutlineTurnFull - 1);
		uint32_t sweep = gradient->sweepTurn;
		uint32_t t;

		if (0 == sweep)
			t = 0;
		else if (rel > sweep) {
			uint32_t after = rel - sweep;
			uint32_t before = kPocoOutlineTurnFull - rel;
			t = (after < before) ? 255 : 0;
		}
		else {
			t = (rel * gradient->invSweepScale) >> 16;
			if (t > 255)
				t = 255;
		}
		return (uint8_t)t;
	}

	if (0 == gradient->len2)
		return 0;

	return (uint8_t)PocoLinearGradientTFromNum(
		gradient,
		(int64_t)(x - gradient->x0) * gradient->adx + (int64_t)(y - gradient->y0) * gradient->ady
	);
}

static void PocoGradientShadeFromT(const PocoLinearGradientRecord *gradient, uint32_t t, uint8_t *r, uint8_t *g, uint8_t *b)
{
	int i;

	if (t <= gradient->stops[0].offset) {
		*r = gradient->stops[0].r;
		*g = gradient->stops[0].g;
		*b = gradient->stops[0].b;
		return;
	}

	for (i = 1; i < gradient->stopCount; i++) {
		if (t <= gradient->stops[i].offset) {
			uint8_t o0 = gradient->stops[i - 1].offset;
			uint8_t o1 = gradient->stops[i].offset;
			uint32_t span = (uint32_t)(o1 - o0);
			uint32_t f = span ? (((t - o0) << 8) / span) : 0;
			int32_t r0 = gradient->stops[i - 1].r;
			int32_t g0 = gradient->stops[i - 1].g;
			int32_t b0 = gradient->stops[i - 1].b;
			*r = (uint8_t)(r0 + ((((int32_t)gradient->stops[i].r - r0) * (int32_t)f) >> 8));
			*g = (uint8_t)(g0 + ((((int32_t)gradient->stops[i].g - g0) * (int32_t)f) >> 8));
			*b = (uint8_t)(b0 + ((((int32_t)gradient->stops[i].b - b0) * (int32_t)f) >> 8));
			return;
		}
	}

	i = gradient->stopCount - 1;
	*r = gradient->stops[i].r;
	*g = gradient->stops[i].g;
	*b = gradient->stops[i].b;
}

void PocoLinearGradientBuildLUT(const PocoLinearGradientRecord *gradient, PocoPixel *lut)
{
	uint32_t t;

	if (0 == gradient->stopCount) {
		for (t = 0; t < 256; t++)
			lut[t] = 0;
		return;
	}

	if (1 == gradient->stopCount) {
		PocoPixel color = PocoMakeColor((Poco)NULL, gradient->stops[0].r, gradient->stops[0].g, gradient->stops[0].b);
		for (t = 0; t < 256; t++)
			lut[t] = color;
		return;
	}

	for (t = 0; t < 256; t++) {
		uint8_t r, g, b;
		PocoGradientShadeFromT(gradient, t, &r, &g, &b);
		lut[t] = PocoMakeColor((Poco)NULL, r, g, b);
	}
}

static uint32_t PocoLinearGradientTFromNum(const PocoLinearGradientRecord *gradient, int64_t num)
{
	uint32_t t;

	if (num <= 0)
		return 0;
	if (num >= (int64_t)gradient->len2)
		return 255;
	if (!gradient->len2Scale)
		return 0;
	t = (uint32_t)(((uint64_t)num * gradient->len2Scale) >> 24);
	return (t > 255) ? 255 : t;
}

static void PocoLinearGradientSampleRGB(const PocoLinearGradientRecord *gradient, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b)
{
	if (0 == gradient->stopCount) {
		*r = *g = *b = 0;
		return;
	}
	if (1 == gradient->stopCount) {
		*r = gradient->stops[0].r;
		*g = gradient->stops[0].g;
		*b = gradient->stops[0].b;
		return;
	}
	PocoGradientShadeFromT(gradient, PocoLinearGradientSampleT(gradient, x, y), r, g, b);
}

PocoPixel PocoLinearGradientSamplePixel(const PocoLinearGradientRecord *gradient, int x, int y)
{
	uint8_t r, g, b;
	PocoLinearGradientSampleRGB(gradient, x, y, &r, &g, &b);
	return PocoMakeColor((Poco)NULL, r, g, b);
}

static void unpackPocoPixelRGB(PocoPixel color, uint8_t *r, uint8_t *g, uint8_t *b)
{
#if kCommodettoBitmapRGB565LE == kPocoPixelFormat
	*r = (uint8_t)(((color >> 11) & 0x1F) * 255 / 31);
	*g = (uint8_t)(((color >> 5) & 0x3F) * 255 / 63);
	*b = (uint8_t)((color & 0x1F) * 255 / 31);
#elif kCommodettoBitmapRGB565BE == kPocoPixelFormat
	{
		uint16_t c = commodetto_bswap16(color);
		*r = (uint8_t)(((c >> 11) & 0x1F) * 255 / 31);
		*g = (uint8_t)(((c >> 5) & 0x3F) * 255 / 63);
		*b = (uint8_t)((c & 0x1F) * 255 / 31);
	}
#elif (kCommodettoBitmapGray16 == kPocoPixelFormat) || (kCommodettoBitmapGray256 == kPocoPixelFormat)
	*r = *g = *b = (uint8_t)color;
#else
	*r = *g = *b = 0;
#endif
}

void PocoLinearGradientFromSlot(xsMachine *the, xsSlot *slot, PocoLinearGradient gradient)
{
	int i, j, count;
	uint8_t angular = 0;

	c_memset(gradient, 0, sizeof(PocoLinearGradientRecord));

	xsmcVars(3);

	if (xsmcHas(*slot, xsID_type)) {
		char *type;
		xsmcGet(xsVar(0), *slot, xsID_type);
		type = xsmcToString(xsVar(0));
		if ((0 == c_strcmp(type, "angular")) || (0 == c_strcmp(type, "conic")))
			angular = 1;
	}
	else if (xsmcHas(*slot, xsID_cx) && xsmcHas(*slot, xsID_startAngle))
		angular = 1;

	if (angular) {
		float startAngle, sweepAngle;

		xsmcGet(xsVar(0), *slot, xsID_cx);
		gradient->x0 = (int16_t)xsmcToInteger(xsVar(0));
		xsmcGet(xsVar(0), *slot, xsID_cy);
		gradient->y0 = (int16_t)xsmcToInteger(xsVar(0));
		xsmcGet(xsVar(0), *slot, xsID_startAngle);
		startAngle = (float)xsmcToNumber(xsVar(0));
		if (xsmcHas(*slot, xsID_sweepAngle)) {
			xsmcGet(xsVar(0), *slot, xsID_sweepAngle);
			sweepAngle = (float)xsmcToNumber(xsVar(0));
		}
		else {
			xsmcGet(xsVar(0), *slot, xsID_endAngle);
			sweepAngle = (float)xsmcToNumber(xsVar(0)) - startAngle;
		}
		PocoAngularSetAngles(gradient, startAngle, sweepAngle);
		gradient->flags = kPocoGradientFlagAngular;
		if (xsmcHas(*slot, xsID_fast)) {
			xsmcGet(xsVar(0), *slot, xsID_fast);
			if (xsmcToBoolean(xsVar(0)))
				gradient->flags |= kPocoGradientFlagAngularFast;
		}
	}
	else {
		xsmcGet(xsVar(0), *slot, xsID_x0);
		gradient->x0 = (int16_t)xsmcToInteger(xsVar(0));
		xsmcGet(xsVar(0), *slot, xsID_y0);
		gradient->y0 = (int16_t)xsmcToInteger(xsVar(0));
		xsmcGet(xsVar(0), *slot, xsID_x1);
		gradient->x1 = (int16_t)xsmcToInteger(xsVar(0));
		xsmcGet(xsVar(0), *slot, xsID_y1);
		gradient->y1 = (int16_t)xsmcToInteger(xsVar(0));
	}

	xsmcGet(xsVar(0), *slot, xsID_stops);
	xsmcGet(xsVar(1), xsVar(0), xsID_length);
	count = xsmcToInteger(xsVar(1));
	if (count < 1)
		xsUnknownError("gradient needs stops");
	if (count > kPocoLinearGradientMaxStops)
		count = kPocoLinearGradientMaxStops;

	for (i = 0; i < count; i++) {
		double offset;
		uint8_t r = 0, g = 0, b = 0;

		xsmcGetIndex(xsVar(1), xsVar(0), i);

		xsmcGet(xsVar(2), xsVar(1), xsID_offset);
		offset = xsmcToNumber(xsVar(2));
		if (offset < 0)
			offset = 0;
		else if (offset > 1)
			offset = 1;
		gradient->stops[i].offset = (uint8_t)(offset * 255 + 0.5);

		if (xsmcHas(xsVar(1), xsID_r)) {
			xsmcGet(xsVar(2), xsVar(1), xsID_r);
			r = (uint8_t)xsmcToInteger(xsVar(2));
			xsmcGet(xsVar(2), xsVar(1), xsID_g);
			g = (uint8_t)xsmcToInteger(xsVar(2));
			xsmcGet(xsVar(2), xsVar(1), xsID_b);
			b = (uint8_t)xsmcToInteger(xsVar(2));
		}
		else {
			PocoPixel color;
			xsmcGet(xsVar(2), xsVar(1), xsID_color);
			color = (PocoPixel)xsmcToInteger(xsVar(2));
			unpackPocoPixelRGB(color, &r, &g, &b);
		}
		gradient->stops[i].r = r;
		gradient->stops[i].g = g;
		gradient->stops[i].b = b;
	}

	/* insertion-sort stops by offset */
	for (i = 1; i < count; i++) {
		uint8_t o = gradient->stops[i].offset;
		uint8_t rr = gradient->stops[i].r;
		uint8_t gg = gradient->stops[i].g;
		uint8_t bb = gradient->stops[i].b;
		j = i - 1;
		while ((j >= 0) && (gradient->stops[j].offset > o)) {
			gradient->stops[j + 1] = gradient->stops[j];
			j -= 1;
		}
		gradient->stops[j + 1].offset = o;
		gradient->stops[j + 1].r = rr;
		gradient->stops[j + 1].g = gg;
		gradient->stops[j + 1].b = bb;
	}

	gradient->stopCount = (uint8_t)count;
	PocoLinearGradientPrepare(gradient);
}

void xs_outlinerenderer_blendOutline(xsMachine *the)
{
	Poco poco = xsmcGetHostDataPoco(xsThis);
	xsOutlineRenderer or = PocoOutlineRenderer();
	PocoOutline buffer = (PocoOutline)xsmcGetHostData(xsArg(2));
	xsOutlineRenderRecord orr;
	PocoCoordinate xMax, yMax, dx, dy;
	xsType paintType;

	orr.or = or;
	orr.blend = (uint8_t)xsmcToInteger(xsArg(1));
	orr.gradientSlot = 0;
	paintType = xsmcTypeOf(xsArg(0));
	if ((xsIntegerType == paintType) || (xsNumberType == paintType)) {
		orr.paintKind = kPocoPaintSolid;
		orr.color = (PocoPixel)xsmcToInteger(xsArg(0));
	}
	else {
		PocoLinearGradientRecord gradient;
		orr.paintKind = kPocoPaintLinearGradient;
		orr.color = 0;
		PocoLinearGradientFromSlot(the, &xsArg(0), &gradient);
		orr.gradientSlot = outlineAllocGradientSlot(poco, or, &gradient);
		gOutlineGradientCount += 1;
	}
	gOutlineFillCount += 1;

	if (xsmcArgc >= 4) {
		dx = xsmcToInteger(xsArg(3));
		dy = xsmcToInteger(xsArg(4));
	}
	else {
		dx = 0;
		dy = 0;
	}
	bufferToFTOutline(buffer, &orr.outline);
#if (90 == kPocoRotation) || (180 == kPocoRotation) || (270 == kPocoRotation)
	PocoOutlineRotate(buffer, poco->width, poco->height);
	#if (90 == kPocoRotation)
		int t = dx;
	 	dx = -dy;
	 	dy = t;
	#elif (180 == kPocoRotation)
	 	dx = -dx;
	 	dy = -dy;
	 #else
		int t = dx;
	 	dx = dy;
	 	dy = -t;
	#endif 	
#endif
	PocoOutlineCalculateCBox(buffer);
	orr.dx = dx;

	int x = buffer->xMin + orr.dx;
	int y = buffer->yMin + dy;
	int w = buffer->w;
	int h = buffer->h;

	xMax = x + w;
	yMax = y + h;

	if (x < poco->x)
		x = poco->x;

	if (xMax > poco->xMax)
		xMax = poco->xMax;

	if (x >= xMax)
		return;

	w = xMax - x;

	if (y < poco->y)
		y = poco->y;

	if (yMax > poco->yMax)
		yMax = poco->yMax;

	if (y >= yMax)
		return;

	h = yMax - y;

	orr.x = x - orr.dx;
	orr.y = y - dy;
	PocoDrawExternal(poco, doOutline, (void *)&orr, sizeof(orr), x, y, w, h);
	
	PocoHold(the, poco, xsmcToReference(xsArg(2)));
}

void xs_outlinerenderer_getOutlineStats(xsMachine *the)
{
	uint32_t fills, gradients, slotsUsed, slotsPeak;

	xsmcVars(1);
	PocoOutlineStatsGet(&fills, &gradients, &slotsUsed, &slotsPeak);
	xsResult = xsNewObject();
	xsmcSetInteger(xsVar(0), fills);
	xsmcSet(xsResult, xsID_fills, xsVar(0));
	xsmcSetInteger(xsVar(0), gradients);
	xsmcSet(xsResult, xsID_gradients, xsVar(0));
	xsmcSetInteger(xsVar(0), slotsUsed);
	xsmcSet(xsResult, xsID_slotsUsed, xsVar(0));
	xsmcSetInteger(xsVar(0), slotsPeak);
	xsmcSet(xsResult, xsID_slotsPeak, xsVar(0));
}

void xs_outlinerenderer_resetOutlineStats(xsMachine *the)
{
	PocoOutlineStatsReset();
}


typedef struct {
	PocoPixel		*dst;
	int16_t			rowBytes;
	PocoCoordinate	x;
	PocoCoordinate	y;
	PocoPixel 		color;
	uint8_t 		blend;
	uint8_t			paintKind;
	uint8_t			haveLut;
	PocoLinearGradientRecord gradient;
	PocoPixel		lut[256];		/* built once per band; not stored in display list */
#if 4 == kPocoPixelSize
	uint8_t 		xphase;
#endif
} xsOutlineSpanRecord, *xsOutlineSpan;

#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#endif
static inline PocoPixel pocoGradColor(xsOutlineSpan os, int x, int y)
{
	return os->lut[PocoLinearGradientSampleT(&os->gradient, x, y)];
}

void doOutline(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h, uint8_t xphase)
{
	xsOutlineRender orr = (xsOutlineRender)refcon;
	xsOutlineRenderer or = orr->or;
	xsOutlineSpanRecord os;

	os.dst = dst;
	os.rowBytes = poco->rowBytes;
	os.x = orr->x;
	os.y = orr->y;
	os.color = orr->color;
	os.blend = orr->blend;
	os.paintKind = orr->paintKind;
	os.haveLut = 0;
	if (kPocoPaintLinearGradient == orr->paintKind) {
		if (or->gradientSlots && (orr->gradientSlot < or->gradientSlotCount))
			os.gradient = or->gradientSlots[orr->gradientSlot];
		else
			c_memset(&os.gradient, 0, sizeof(os.gradient));
		PocoLinearGradientBuildLUT(&os.gradient, os.lut);
		os.haveLut = 1;
	}
#if 4 == kPocoPixelSize
	os.xphase = xphase;
#endif

	or->params.user = &os;
	or->params.clip_box.xMin = orr->x;
	or->params.clip_box.yMin = orr->y;
	or->params.clip_box.xMax = or->params.clip_box.xMin + w;
	or->params.clip_box.yMax = or->params.clip_box.yMin + h;

	or->params.source = &orr->outline;
	or->params.gray_spans = (255 == os.blend) ? doOutlineOpaqueSpan : doOutlineBlendSpan;

	ft_grays_raster.raster_render(or->raster, &or->params);

	orr->y += h;
}

typedef struct {
	xsOutlineRenderer or;
	PocoCoordinate x;
	PocoCoordinate y;
	short n_points;
	PocoPixel color;
	uint8_t blend;
	FT_Pos points[16 * 2];
} xsPolygonRenderRecord, *xsPolygonRender;

const char gPolygonFlags[16] = {FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON,
						FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON, FT_CURVE_TAG_ON};

void xs_outlinerenderer_blendPolygon(xsMachine *the)
{
	Poco poco = xsmcGetHostDataPoco(xsThis);
	xsOutlineRenderer or = PocoOutlineRenderer();
	xsPolygonRenderRecord prr;
	PocoCoordinate xMax, yMax;
	int i;
	int argc = xsmcArgc;

	prr.or = or;
	prr.color = (PocoPixel)xsmcToInteger(xsArg(0));
	prr.blend = (uint8_t)xsmcToInteger(xsArg(1));
	prr.n_points = (argc - 2);
	if ((prr.n_points >= 32) || (prr.n_points <= 0))
		xsUnknownError("too many points");
	if (1 == prr.n_points) {
		xsmcGet(xsResult, xsArg(3), xsID_length);
		prr.n_points = xsmcToInteger(xsResult);
		if ((prr.n_points >= 32) || (prr.n_points <= 0))
			xsUnknownError("too many points");
		for (i = 0; i < prr.n_points; i++) {
			xsmcGetIndex(xsResult, xsArg(3), i);
			prr.points[i] = xsmcToInteger(xsResult) << 6;
		}
		prr.n_points >>= 1;
	}
	else {
		for (i = 0; i < prr.n_points; i++)
			prr.points[i] = xsmcToInteger(xsArg(2 + i)) << 6;
		prr.n_points >>= 1;
	}
	prr.n_points -= 1;

	FT_BBox box;
	FT_Outline outline = {
    	.n_contours = 1,
    	.n_points = prr.n_points + 1,
    	.points = (FT_Vector *)prr.points,
    	.tags = (char *)gPolygonFlags,
    	.contours = &prr.n_points,
    	.flags = FT_OUTLINE_NONE
	};
	FT_Outline_Get_CBox(&outline, &box);

	int x = box.xMin >> 6;
	int y = box.yMin >> 6;
	int w = ((box.xMax + 63) >> 6) - x;
	int h = ((box.yMax + 63) >> 6) - y;

	xMax = x + w;
	yMax = y + h;

	if (x < poco->x)
		x = poco->x;

	if (xMax > poco->xMax)
		xMax = poco->xMax;

	if (x >= xMax)
		return;

	w = xMax - x;

	if (y < poco->y)
		y = poco->y;

	if (yMax > poco->yMax)
		yMax = poco->yMax;

	if (y >= yMax)
		return;

	h = yMax - y;

	prr.x = x;
	prr.y = y;
	PocoDrawExternal(poco, doPolygon, (void *)&prr, offsetof(xsPolygonRenderRecord, points) + ((1 + prr.n_points) * 2 * sizeof(FT_Pos)), x, y, w, h);
}

void doPolygon(Poco poco, uint8_t *refcon, PocoPixel *dst, PocoDimension w, PocoDimension h, uint8_t xphase)
{
	xsPolygonRender prr = (xsPolygonRender)refcon;
	xsOutlineRenderer or = prr->or;
	xsOutlineSpanRecord os;
	FT_Outline outline = {
    	.n_contours = 1,
    	.n_points = prr->n_points + 1,
    	.points = (FT_Vector *)prr->points,
    	.tags = (char *)gPolygonFlags,
    	.contours = &prr->n_points,
    	.flags = FT_OUTLINE_NONE,
	};

	os.dst = dst;
	os.rowBytes = poco->rowBytes;
	os.x = prr->x;
	os.y = prr->y;
	os.color = prr->color;
	os.blend = prr->blend;
	os.paintKind = kPocoPaintSolid;
	os.haveLut = 0;
#if 4 == kPocoPixelSize
	os.xphase = xphase;
#endif

	or->params.user = &os;
	or->params.clip_box.xMin = poco->x;
	or->params.clip_box.yMin = prr->y;
	or->params.clip_box.xMax = poco->x + poco->w;
	or->params.clip_box.yMax = prr->y + h;

	or->params.source = &outline;
	or->params.gray_spans = (255 == os.blend) ? doOutlineOpaqueSpan : doOutlineBlendSpan;

	ft_grays_raster.raster_render(or->raster, &or->params);

	prr->y += h;
}

#if kCommodettoBitmapRGB565LE == kPocoPixelFormat

static void blendPixelRGB565LE(PocoPixel *p, PocoPixel color, uint8_t blend5)
{
	int src32, dst;

	if (31 == blend5) {
		*p = color;
		return;
	}

	src32 = color;
	src32 |= src32 << 16;
	src32 &= 0x07E0F81F;
	dst = *p;
	dst |= dst << 16;
	dst &= 0x07E0F81F;
	src32 = src32 - dst;
	dst = blend5 * src32 + (dst << 5) - dst;
	dst += 0x02008010;
	dst += (dst >> 5) & 0x07E0F81F;
	dst >>= 5;
	dst &= 0x07E0F81F;
	dst |= dst >> 16;
	*p = (PocoPixel)dst;
}

void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	if (kPocoPaintLinearGradient == os->paintKind) {
		const PocoLinearGradientRecord *g = &os->gradient;
		uint8_t flags = g->flags;
		PocoPixel rowColor = 0;
		uint8_t haveRowColor = (flags & kPocoGradientFlagVertical) ? 1 : 0;
		if (haveRowColor)
			rowColor = os->lut[PocoLinearGradientSampleT(g, 0, y)];
		pixels -= os->x;
		do {
			uint16_t len = spans->len;
			int x = spans->x;
			PocoPixel *p = pixels + x;
			uint8_t coverage = spans->coverage;
			if (255 == coverage) {
				if (haveRowColor) {
					while (len--)
						*p++ = rowColor;
				}
				else if (flags & kPocoGradientFlagAngular) {
					while (len--) {
						*p++ = os->lut[PocoLinearGradientSampleT(g, x, y)];
						x += 1;
					}
				}
				else {
					int64_t num = (int64_t)(x - g->x0) * g->adx + (int64_t)(y - g->y0) * g->ady;
					while (len--) {
						*p++ = os->lut[PocoLinearGradientTFromNum(g, num)];
						num += g->adx;
						x += 1;
					}
				}
			}
			else {
				uint8_t blend = coverage >> 3;
				if (haveRowColor) {
					while (len--)
						blendPixelRGB565LE(p++, rowColor, blend);
				}
				else if (flags & kPocoGradientFlagAngular) {
					while (len--) {
						blendPixelRGB565LE(p++, os->lut[PocoLinearGradientSampleT(g, x, y)], blend);
						x += 1;
					}
				}
				else {
					int64_t num = (int64_t)(x - g->x0) * g->adx + (int64_t)(y - g->y0) * g->ady;
					while (len--) {
						blendPixelRGB565LE(p++, os->lut[PocoLinearGradientTFromNum(g, num)], blend);
						num += g->adx;
						x += 1;
					}
				}
			}
			spans++;
		} while (--count);
		return;
	}

	{
		int src32 = os->color;
		src32 |= src32 << 16;
		src32 &= 0x07E0F81F;

		pixels -= os->x;
		do {
			uint16_t len = spans->len;
			PocoPixel *p = pixels + spans->x;
			uint8_t blend = spans->coverage;

			if (255 == blend) {
				uint16_t pixel = os->color;
				while (len--)
					*p++ = pixel;
			}
			else {
				blend >>= 3;

				while (len--) {
					int	dst, src;

					dst = *p;
					dst |= dst << 16;
					dst &= 0x07E0F81F;
					src = src32 - dst;
					dst = blend * src + (dst << 5) - dst;
					dst += 0x02008010;
					dst += (dst >> 5) & 0x07E0F81F;
					dst >>= 5;
					dst &= 0x07E0F81F;
					dst |= dst >> 16;
					*p++ = (PocoPixel)dst;
				}
			}

			spans++;
		} while (--count);
	}
}

#elif kCommodettoBitmapRGB565BE == kPocoPixelFormat

static void blendPixelRGB565BE(PocoPixel *p, PocoPixel color, uint8_t blend5)
{
	int src32, dst;

	if (31 == blend5) {
		*p = color;
		return;
	}

	src32 = commodetto_bswap16(color);
	src32 |= src32 << 16;
	src32 &= 0x07E0F81F;
	dst = commodetto_bswap16(*p);
	dst |= dst << 16;
	dst &= 0x07E0F81F;
	src32 = src32 - dst;
	dst = blend5 * src32 + (dst << 5) - dst;
	dst += 0x02008010;
	dst += (dst >> 5) & 0x07E0F81F;
	dst >>= 5;
	dst &= 0x07E0F81F;
	dst |= dst >> 16;
	*p = (PocoPixel)commodetto_bswap16((uint16_t)dst);
}

void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	if (kPocoPaintLinearGradient == os->paintKind) {
		const PocoLinearGradientRecord *g = &os->gradient;
		uint8_t flags = g->flags;
		PocoPixel rowColor = 0;
		uint8_t haveRowColor = (flags & kPocoGradientFlagVertical) ? 1 : 0;
		if (haveRowColor)
			rowColor = os->lut[PocoLinearGradientSampleT(g, 0, y)];
		pixels -= os->x;
		do {
			uint16_t len = spans->len;
			int x = spans->x;
			PocoPixel *p = pixels + x;
			uint8_t coverage = spans->coverage;
			if (255 == coverage) {
				if (haveRowColor) {
					while (len--)
						*p++ = rowColor;
				}
				else if (flags & kPocoGradientFlagAngular) {
					while (len--) {
						*p++ = os->lut[PocoLinearGradientSampleT(g, x, y)];
						x += 1;
					}
				}
				else {
					int64_t num = (int64_t)(x - g->x0) * g->adx + (int64_t)(y - g->y0) * g->ady;
					while (len--) {
						*p++ = os->lut[PocoLinearGradientTFromNum(g, num)];
						num += g->adx;
						x += 1;
					}
				}
			}
			else {
				uint8_t blend = coverage >> 3;
				if (haveRowColor) {
					while (len--)
						blendPixelRGB565BE(p++, rowColor, blend);
				}
				else if (flags & kPocoGradientFlagAngular) {
					while (len--) {
						blendPixelRGB565BE(p++, os->lut[PocoLinearGradientSampleT(g, x, y)], blend);
						x += 1;
					}
				}
				else {
					int64_t num = (int64_t)(x - g->x0) * g->adx + (int64_t)(y - g->y0) * g->ady;
					while (len--) {
						blendPixelRGB565BE(p++, os->lut[PocoLinearGradientTFromNum(g, num)], blend);
						num += g->adx;
						x += 1;
					}
				}
			}
			spans++;
		} while (--count);
		return;
	}

	{
		int src32 = commodetto_bswap16(os->color);
		src32 |= src32 << 16;
		src32 &= 0x07E0F81F;

		pixels -= os->x;
		do {
			uint16_t len = spans->len;
			PocoPixel *p = pixels + spans->x;
			uint8_t blend = spans->coverage;

			if (255 == blend) {
				uint16_t pixel = os->color;
				while (len--)
					*p++ = pixel;
			}
			else {
				blend >>= 3;

				while (len--) {
					int	dst, src;

					dst = commodetto_bswap16(*p);
					dst |= dst << 16;
					dst &= 0x07E0F81F;
					src = src32 - dst;
					dst = blend * src + (dst << 5) - dst;
					dst += 0x02008010;
					dst += (dst >> 5) & 0x07E0F81F;
					dst >>= 5;
					dst &= 0x07E0F81F;
					dst |= dst >> 16;
					*p++ = (PocoPixel)commodetto_bswap16((uint16_t)dst);
				}
			}

			spans++;
		} while (--count);
	}
}

#elif kCommodettoBitmapGray16 == kPocoPixelFormat

void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	do {
		uint16_t len = spans->len;
		int dx = spans->x - os->x;
		int x = spans->x;
		PocoPixel *p = pixels + (dx >> 1);
		uint8_t xphase = os->xphase + (dx & 1);
		if (2 == xphase) {
			xphase = 0;
			p++;
		}
		uint16_t c = spans->coverage;
		while (len--) {
			uint8_t color = os->color;
			if (kPocoPaintLinearGradient == os->paintKind)
				color = (uint8_t)pocoGradColor(os, x, y);
			if (255 == c) {
				uint8_t pixel = *p;
				if (xphase) {
					*p++ = (pixel & 0xF0) | color;
					xphase = 0;
				}
				else {
					*p = (pixel & 0x0F) | (color << 4);
					xphase = 1;
				}
			}
			else {
				uint8_t pixel = *p;
				uint16_t accum;
				if (xphase)
					accum = pixel & 0x0F;
				else
					accum = (pixel & 0xF0) >> 4;
				accum *= (255 - c);
				accum += color * c;
				accum >>= 8;
				if (xphase) {
					*p++ = (pixel & 0xF0) | accum;
					xphase = 0;
				}
				else {
					*p = (pixel & 0x0F) | (accum << 4);
					xphase = 1;
				}
			}
			x += 1;
		}

		spans++;
	} while (--count);
}
#elif kCommodettoBitmapGray256 == kPocoPixelFormat

void doOutlineOpaqueSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	pixels -= os->x;
	do {
		uint16_t len = spans->len;
		int x = spans->x;
		PocoPixel *p = pixels + x;
		uint8_t blend = spans->coverage >> 3;

		while (len--) {
			PocoPixel color = os->color;
			if (kPocoPaintLinearGradient == os->paintKind)
				color = pocoGradColor(os, x, y);
			if (31 == blend)
				*p++ = color;
			else {
				uint16_t pixel = color * blend;
				uint16_t t = (*p * (31 - blend)) + pixel;
				*p++ = t >> 5;
			}
			x += 1;
		}

		spans++;
	} while (--count);
}

#else
	#error unsupported outline pixel format
#endif

#if kCommodettoBitmapRGB565LE == kPocoPixelFormat

void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	if (kPocoPaintLinearGradient == os->paintKind) {
		const PocoLinearGradientRecord *g = &os->gradient;
		uint8_t flags = g->flags;
		PocoPixel rowColor = 0;
		uint8_t haveRowColor = (flags & kPocoGradientFlagVertical) ? 1 : 0;
		if (haveRowColor)
			rowColor = os->lut[PocoLinearGradientSampleT(g, 0, y)];
		pixels -= os->x;
		do {
			uint16_t len = spans->len;
			int x = spans->x;
			PocoPixel *p = pixels + x;
			uint8_t blend = (spans->coverage * os->blend) >> 11;
			if (haveRowColor) {
				while (len--)
					blendPixelRGB565LE(p++, rowColor, blend);
			}
			else if (flags & kPocoGradientFlagAngular) {
				while (len--) {
					blendPixelRGB565LE(p++, os->lut[PocoLinearGradientSampleT(g, x, y)], blend);
					x += 1;
				}
			}
			else {
				int64_t num = (int64_t)(x - g->x0) * g->adx + (int64_t)(y - g->y0) * g->ady;
				while (len--) {
					blendPixelRGB565LE(p++, os->lut[PocoLinearGradientTFromNum(g, num)], blend);
					num += g->adx;
					x += 1;
				}
			}
			spans++;
		} while (--count);
		return;
	}

	{
		int src32 = os->color;
		src32 |= src32 << 16;
		src32 &= 0x07E0F81F;

		pixels -= os->x;
		do {
			uint16_t len = spans->len;
			PocoPixel *p = pixels + spans->x;
			uint8_t blend = (spans->coverage * os->blend) >> 11;

			while (len--) {
				int	dst, src;

				dst = *p;
				dst |= dst << 16;
				dst &= 0x07E0F81F;
				src = src32 - dst;
				dst = blend * src + (dst << 5) - dst;
				dst += 0x02008010;
				dst += (dst >> 5) & 0x07E0F81F;
				dst >>= 5;
				dst &= 0x07E0F81F;
				dst |= dst >> 16;
				*p++ = (PocoPixel)dst;
			}

			spans++;
		} while (--count);
	}
}

#elif kCommodettoBitmapRGB565BE == kPocoPixelFormat

void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	if (kPocoPaintLinearGradient == os->paintKind) {
		const PocoLinearGradientRecord *g = &os->gradient;
		uint8_t flags = g->flags;
		PocoPixel rowColor = 0;
		uint8_t haveRowColor = (flags & kPocoGradientFlagVertical) ? 1 : 0;
		if (haveRowColor)
			rowColor = os->lut[PocoLinearGradientSampleT(g, 0, y)];
		pixels -= os->x;
		do {
			uint16_t len = spans->len;
			int x = spans->x;
			PocoPixel *p = pixels + x;
			uint8_t blend = (spans->coverage * os->blend) >> 11;
			if (haveRowColor) {
				while (len--)
					blendPixelRGB565BE(p++, rowColor, blend);
			}
			else if (flags & kPocoGradientFlagAngular) {
				while (len--) {
					blendPixelRGB565BE(p++, os->lut[PocoLinearGradientSampleT(g, x, y)], blend);
					x += 1;
				}
			}
			else {
				int64_t num = (int64_t)(x - g->x0) * g->adx + (int64_t)(y - g->y0) * g->ady;
				while (len--) {
					blendPixelRGB565BE(p++, os->lut[PocoLinearGradientTFromNum(g, num)], blend);
					num += g->adx;
					x += 1;
				}
			}
			spans++;
		} while (--count);
		return;
	}

	{
		int src32 = commodetto_bswap16(os->color);
		src32 |= src32 << 16;
		src32 &= 0x07E0F81F;

		pixels -= os->x;
		do {
			uint16_t len = spans->len;
			PocoPixel *p = pixels + spans->x;
			uint8_t blend = (spans->coverage * os->blend) >> 11;

			while (len--) {
				int	dst, src;

				dst = commodetto_bswap16(*p);
				dst |= dst << 16;
				dst &= 0x07E0F81F;
				src = src32 - dst;
				dst = blend * src + (dst << 5) - dst;
				dst += 0x02008010;
				dst += (dst >> 5) & 0x07E0F81F;
				dst >>= 5;
				dst &= 0x07E0F81F;
				dst |= dst >> 16;
				*p++ = (PocoPixel)commodetto_bswap16((uint16_t)dst);
			}

			spans++;
		} while (--count);
	}
}

#elif kCommodettoBitmapGray16 == kPocoPixelFormat

void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	do {
		uint16_t len = spans->len;
		int dx = spans->x - os->x;
		int x = spans->x;
		PocoPixel *p = pixels + (dx >> 1);
		uint8_t xphase = os->xphase + (dx & 1);
		if (2 == xphase) {
			xphase = 0;
			p++;
		}
		uint16_t c = (spans->coverage * os->blend) >> 8;
		while (len--) {
			uint8_t color = os->color;
			if (kPocoPaintLinearGradient == os->paintKind)
				color = (uint8_t)pocoGradColor(os, x, y);
			{
				uint8_t pixel = *p;
				uint16_t accum;
				uint16_t cc = color * c;
				if (xphase)
					accum = pixel & 0x0F;
				else
					accum = (pixel & 0xF0) >> 4;
				accum *= (255 - c);
				accum += cc;
				accum >>= 8;
				if (xphase) {
					*p++ = (pixel & 0xF0) | accum;
					xphase = 0;
				}
				else {
					*p = (pixel & 0x0F) | (accum << 4);
					xphase = 1;
				}
			}
			x += 1;
		}

		spans++;
	} while (--count);
}

#elif kCommodettoBitmapGray256 == kPocoPixelFormat

void doOutlineBlendSpan(int y, int count, const FT_Span *spans, void *user)
{
	xsOutlineSpan os = user;
	PocoPixel *pixels = (PocoPixel *)(((uint8_t *)os->dst) + ((y - os->y) * os->rowBytes));

	pixels -= os->x;
	do {
		uint16_t len = spans->len;
		int x = spans->x;
		PocoPixel *p = pixels + x;
		uint8_t blend = spans->coverage >> 3;

		while (len--) {
			PocoPixel color = os->color;
			if (kPocoPaintLinearGradient == os->paintKind)
				color = pocoGradColor(os, x, y);
			{
				uint16_t pixel = color * blend;
				uint16_t t = (*p * (31 - blend)) + pixel;
				*p++ = t >> 5;
			}
			x += 1;
		}

		spans++;
	} while (--count);
}

#else
	#error unsupported outline pixel format
#endif


static void outlineFillCommon(Poco poco, uint8_t paintKind, PocoColor color, const PocoLinearGradientRecord *gradient, uint8_t blend, PocoOutline pOutline, PocoCoordinate dx, PocoCoordinate dy)
{
	xsOutlineRenderRecord orr;
	PocoCoordinate xMax, yMax;
	orr.or = PocoOutlineRenderer();
	orr.paintKind = paintKind;
	orr.color = color;
	orr.blend = blend;
	orr.gradientSlot = 0;
	if (kPocoPaintLinearGradient == paintKind) {
		orr.gradientSlot = outlineAllocGradientSlot(poco, orr.or, gradient);
		gOutlineGradientCount += 1;
	}
	gOutlineFillCount += 1;
	bufferToFTOutline(pOutline, &orr.outline);
	
#if (90 == kPocoRotation) || (180 == kPocoRotation) || (270 == kPocoRotation)
	PocoOutlineRotate(pOutline, poco->width, poco->height);
	#if (90 == kPocoRotation)
		int t = dx;
	 	dx = -dy;
	 	dy = t;
	#elif (180 == kPocoRotation)
	 	dx = -dx;
	 	dy = -dy;
	 #else
		int t = dx;
	 	dx = dy;
	 	dy = -t;
	#endif 	
#endif
	PocoOutlineCalculateCBox(pOutline);
	orr.dx = dx;

	int x = pOutline->xMin + dx;
	int y = pOutline->yMin + dy;
	int w = pOutline->w;
	int h = pOutline->h;

	xMax = x + w;
	yMax = y + h;

	if (x < poco->x)
		x = poco->x;

	if (xMax > poco->xMax)
		xMax = poco->xMax;

	if (x >= xMax)
		return;

	w = xMax - x;

	if (y < poco->y)
		y = poco->y;

	if (yMax > poco->yMax)
		yMax = poco->yMax;

	if (y >= yMax)
		return;

	h = yMax - y;

	orr.x = x - orr.dx;
	orr.y = y - dy;
	PocoDrawExternal(poco, doOutline, (void *)&orr, sizeof(orr), x, y, w, h);
}

void PocoOutlineFill(Poco poco, PocoColor color, uint8_t blend, PocoOutline pOutline, PocoCoordinate dx, PocoCoordinate dy)
{
	outlineFillCommon(poco, kPocoPaintSolid, color, NULL, blend, pOutline, dx, dy);
}

void PocoOutlineFillGradient(Poco poco, const PocoLinearGradientRecord *gradient, uint8_t blend, PocoOutline pOutline, PocoCoordinate dx, PocoCoordinate dy)
{
	outlineFillCommon(poco, kPocoPaintLinearGradient, 0, gradient, blend, pOutline, dx, dy);
}
