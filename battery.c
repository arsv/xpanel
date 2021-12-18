#include <stdlib.h>
#include <string.h>

#include "common.h"

#include "xbm/s0.xbm"
#include "xbm/s1.xbm"
#include "xbm/s2.xbm"
#include "xbm/s3.xbm"
#include "xbm/s4.xbm"
#include "xbm/s5.xbm"
#include "xbm/s6.xbm"
#include "xbm/s7.xbm"
#include "xbm/s8.xbm"
#include "xbm/s9.xbm"
#include "xbm/sc.xbm"

#define W 60
#define H 20
#define B 2

#define INACTIVE 0
#define CHARGING 1
#define DISCHARGING 2

static uint bat_disabled;
static uint bat_status;
static uint bat_charge_full;
static uint bat_charge_prev;
static uint bat_charge_now;
static uint bat_current_now;

#define XBM(name) { name##_bits, name##_width, name##_height }

static const struct bitmap {
	byte* data;
	int w;
	int h;
} bitmaps[] = {
	XBM(s0),
	XBM(s1),
	XBM(s2),
	XBM(s3),
	XBM(s4),
	XBM(s5),
	XBM(s6),
	XBM(s7),
	XBM(s8),
	XBM(s9),
	XBM(sc),
};

static void draw_xbm(uint idx)
{
	const struct bitmap* bm = &bitmaps[idx];

	bitmap(bm->data, bm->w, bm->h);
}

static char* prefix(char* p, char* pre)
{
	uint len = strlen(pre);

	if(strncmp(p, pre, len))
		return NULL;

	return p + len;
}

static void parse_bat_line(char* p)
{
	char* q;

	if((q = prefix(p, "POWER_SUPPLY_CHARGE_FULL="))) {
		bat_charge_full = atoi(q);
		return;
	}

	if((q = prefix(p, "POWER_SUPPLY_CHARGE_NOW="))) {
		bat_charge_now = atoi(q);
		return;
	}

	if((q = prefix(p, "POWER_SUPPLY_CURRENT_NOW="))) {
		bat_current_now = atoi(q);
		return;
	}

	if((q = prefix(p, "POWER_SUPPLY_STATUS="))) {
		if(!strcmp(q, "Full"))
			bat_status = INACTIVE;
		else if(!strcmp(q, "Charging"))
			bat_status = CHARGING;
		else if(!strcmp(q, "Discharging"))
			bat_status = DISCHARGING;
		else
			bat_status = INACTIVE;
	}
}

static void parse_bat_info(void)
{
	char* p = databuf;
	char* e = p + datalen;

	bat_charge_prev = bat_charge_now;
	bat_charge_now = 0;

	while(p < e) {
		char* q = skip_to_eol(p, e);

		if(!q) break;

		*q = '\0';

		parse_bat_line(p);

		p = q + 1;
	}
}

static void fillrec(uint x, uint y, uint w, uint h)
{
	uint i, j;

	for(j = 0; j < h; j++) {
		for(i = 0; i < w; i++) {
			point(x + i, y + j);
		}
	}
}

static void hline(uint x, uint y, uint dx)
{
	uint i;

	for(i = 0; i < dx; i++)
		point(x + i, y);
}

static void vline(uint x, uint y, uint dy)
{
	uint i;

	for(i = 0; i < dy; i++)
		point(x, y + i);
}

static void draw_bat_border(void)
{
	setcolor(0x888888);

	hline(2, 2, W - 4);
	hline(2, H - 3, W - 4);

	vline(2, 2, H - 4);
	vline(W - 3, 2, H - 4);
}

static void draw_bat_charge(void)
{
	if(bat_status == CHARGING)
		setcolor(0x4040A0);
	else
		setcolor(0x007000);

	uint full = bat_charge_full / 1000;
	uint now = bat_charge_now / 1000;

	uint ox = 3;
	uint oy = 3;
	uint bw = W - 2*ox;
	uint bh = H - 2*oy;

	uint charge = (bw*now) / full; /* [0..bw] */
	uint empty = bw - charge;

	fillrec(ox + empty, oy, bw - empty, bh);
}

static char digit(uint x)
{
	return '0' + (x % 10);
}

static char* format_time(uint blh, uint blm)
{
	static char buf[10];

	char* p = &buf[9];
	*p = '\0';

	*--p = digit(blm); blm /= 10;
	*--p = digit(blm);

	*--p = ':';

	while((p >= buf) && (blh > 0)) {
		*--p = digit(blh); blh /= 10;
	}

	return p;
}

static uint align_time(char* str)
{
	char* p = str;
	uint w = 0;
	char c;

	while((c = *p++)) {
		if(c >= '0' && c <= '9')
			w += bitmaps[0].w;
		else
			w += bitmaps[10].w;

	}

	if(w > W) return 0;

	return W/2 - w/2;
}

static void draw_string(char* str)
{
	char* p = str;
	char c;

	while((c = *p++)) {
		if(c >= '0' && c <= '9')
			draw_xbm(c - '0');
		else
			draw_xbm(10);
	}
}

static void draw_bat_estime(void)
{
	if(bat_status != DISCHARGING)
		return;
	if(bat_current_now < 10000) /* 10mA? */
		return;

	uint bt = 60*bat_charge_now / bat_current_now;
	uint blh = bt / 60;
	uint blm = bt % 60;

	char* str = format_time(blh, blm);

	uint x = align_time(str);
	uint y = 4;

	setcolor(0xFFFFFF);
	moveto(x, y);

	draw_string(str);
}

static void redraw_battery(void)
{
	if(bat_status == INACTIVE)
		return;
	if(!bat_charge_full)
		return;

	advance(5);

	draw_bat_border();

	draw_bat_charge();

	draw_bat_estime();

	advance(W + 5);
}

void put_battery(void)
{
	if(!bat_disabled)
		;
	else if(bat_disabled++ < 1000)
		return;

	if(load_file("/sys/class/power_supply/BAT0/uevent") < 0) {
		bat_disabled = 1;
		return;
	}

	parse_bat_info();

	redraw_battery();
}
