#include "commodettoPoco.h"

struct PocoOutlineRecord {
	uint16_t	n_points;
	uint16_t	n_contours;
	uint16_t	flags;
	uint8_t		cboxValid;
	uint8_t		reserved;
#if (90 == kPocoRotation) || (180 == kPocoRotation) || (270 == kPocoRotation)
	uint16_t	rw;
	uint16_t	rh;
#endif

	// CBox as integers (no fractional part)
	int16_t		xMin;
	int16_t		yMin;
	uint16_t	w;
	uint16_t	h;
	

	// points as FT_Vector_, contours as uint16_t, tags as uint8_t
};

typedef struct PocoOutlineRecord PocoOutlineRecord;
typedef struct PocoOutlineRecord *PocoOutline;

enum {
	kPocoPaintSolid = 0,
	kPocoPaintLinearGradient = 1
};

enum {
	kPocoGradientFlagVertical = 1 << 0,
	kPocoGradientFlagHorizontal = 1 << 1,
	kPocoGradientFlagAngular = 1 << 2
};

#define kPocoLinearGradientMaxStops (8)

typedef struct {
	int16_t x0;
	int16_t y0;
	int16_t x1;
	int16_t y1;
	uint8_t stopCount;
	uint8_t flags;
	struct {
		uint8_t offset;		/* 0..255 */
		uint8_t r;
		uint8_t g;
		uint8_t b;
	} stops[kPocoLinearGradientMaxStops];
	int32_t adx;
	int32_t ady;
	uint32_t len2;
	float startAngle;		/* angular: radians */
	float sweepAngle;		/* angular: radians, same direction as CanvasPath.arc */
} PocoLinearGradientRecord;
typedef PocoLinearGradientRecord *PocoLinearGradient;

extern void PocoLinearGradientPrepare(PocoLinearGradient gradient);
extern PocoPixel PocoLinearGradientSamplePixel(const PocoLinearGradientRecord *gradient, int x, int y);

extern void PocoOutlineFill(Poco poco, PocoColor color, uint8_t blend, PocoOutline pOutline, PocoCoordinate dx, PocoCoordinate dy);
extern void PocoOutlineFillGradient(Poco poco, const PocoLinearGradientRecord *gradient, uint8_t blend, PocoOutline pOutline, PocoCoordinate dx, PocoCoordinate dy);

extern void PocoOutlineCalculateCBox(PocoOutline pOutline);
#if (90 == kPocoRotation) || (180 == kPocoRotation) || (270 == kPocoRotation)
extern void PocoOutlineRotate(PocoOutline pOutline, int w, int h);
extern void PocoOutlineUnrotate(PocoOutline pOutline);
#endif
