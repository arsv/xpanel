#include <time.h>

#include "common.h"

#include "xbm/n0.xbm"
#include "xbm/n1.xbm"
#include "xbm/n2.xbm"
#include "xbm/n3.xbm"
#include "xbm/n4.xbm"
#include "xbm/n5.xbm"
#include "xbm/n6.xbm"
#include "xbm/n7.xbm"
#include "xbm/n8.xbm"
#include "xbm/n9.xbm"
#include "xbm/nc.xbm"

#define XBM(name) { name##_bits, name##_width, name##_height }

static const struct bitmap {
	byte* data;
	int w;
	int h;
} bitmaps[] = {
	XBM(n0),
	XBM(n1),
	XBM(n2),
	XBM(n3),
	XBM(n4),
	XBM(n5),
	XBM(n6),
	XBM(n7),
	XBM(n8),
	XBM(n9),
	XBM(nc),
};

static void draw_xbm(uint idx)
{
	const struct bitmap* bm = &bitmaps[idx];

	bitmap(bm->data, bm->w, bm->h);
}

static void draw_00(uint v)
{
	draw_xbm((v / 10) % 10);
	draw_xbm(v % 10);
}

void put_clock(void)
{
	struct timespec ts;
	int ret;

	if((ret = clock_gettime(CLOCK_REALTIME, &ts)) < 0)
		return;

	struct tm* tm = localtime(&ts.tv_sec);

	if(!dtms)
		setcolor(0xFFFFFF);
	else
		setcolor(0x00A800);

	moveto(0, 0);

	draw_00(tm->tm_hour);
	draw_xbm(10);
	draw_00(tm->tm_min);
	draw_xbm(10);
	draw_00(tm->tm_sec);

	uint width = 6*bitmaps[0].w + 2*bitmaps[10].w;

	advance(width);
}
