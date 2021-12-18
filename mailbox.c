#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>

#include "common.h"

#include "xbm/mo.xbm"
#include "xbm/mn.xbm"

static char* MAIL;

void init_mailbox(void)
{
	MAIL = getenv("MAIL");
}

static void draw_box(byte* data, uint w, uint h)
{
	advance(2);
	bitmap(data, w, h);
	advance(2 + w);
}

static void draw_new_mailbox(void)
{
	setcolor(0x00A800);
	draw_box(mn_bits, mn_width, mn_height);
}

static void draw_old_mailbox(void)
{
	setcolor(0x666666);
	draw_box(mo_bits, mo_width, mo_height);
}

void put_mailbox(void)
{
	char* name = MAIL;
	struct stat st;

	if(!name)
		return;

	if(stat(name, &st) < 0)
		return;

	if(!st.st_size)
		return;
	if(st.st_atime < st.st_mtime)
		draw_new_mailbox();
	else
		draw_old_mailbox();
}
