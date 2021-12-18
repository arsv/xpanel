#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <err.h>

#include "common.h"

uint color;
uint cx, cy;

void moveto(uint x, uint y)
{
	cx = x;
	cy = y;
}

void advance(uint width)
{
	pix_wused += width;
	cx = 0;
	cy = 0;
}

void setcolor(uint c)
{
	color = c;
}

void point(uint x, uint y)
{
	uint w = pix_width;

	x += pix_wused;

	if(x >= pix_width)
		return;
	if(y >= pix_height)
		return;

	image[y*w + x] = color;
}

void bitmap(byte* data, uint w, uint h)
{
	uint r, c, b = 0;

	for(r = 0; r < h; r++) {
		for(c = 0; c < w; c++) {
			byte d = data[b/8];
			uint s = b % 8;

			b++;

			if(!(d & (1 << s)))
				continue;

			point(cx + c, cy + r);
		}

		b = (b + 7) & ~7;
	}

	cx += w;
}

int load_file(char* name)
{
	int fd, ret;

	if((fd = open(name, O_RDONLY)) < 0)
		return fd;
	if((ret = read(fd, databuf, sizeof(databuf))) < 0)
		return ret;

	datalen = ret;

	if((ret = close(fd)) < 0)
		err(-1, "close");

	return ret;
}

char* skip_to_eol(char* p, char* e)
{
	while(p < e)
		if(!*p || *p == '\n')
			return p;
		else
			p++;

	return NULL;
}

char* parse_int(char* p, uint* v)
{
	uint r = 0;
	char c = *p;

	if(c < '0' || c > '9')
		return NULL;

	while((c = *p)) {
		if(c < '0' || c > '9')
			break;

		r = r*10 + (c - '0');

		p++;
	}

	*v = r;

	while(c == ' ')
		c = *(++p);

	return p;
}

char* parse_add(char* p, uint64_t* v)
{
	uint64_t r = 0;
	char c = *p;

	if(!p) return p;

	if(c < '0' || c > '9')
		return NULL;

	while((c = *p)) {
		if(c < '0' || c > '9')
			break;

		r = r*10 + (c - '0');

		p++;
	} while(c == ' ') {
		p++;
		c = *p;
	}

	*v += r;

	return p;
}

char* skip_space(char* p)
{
	char c;

	while((c = *p) && (c == ' '))
		p++;

	return p;
}

char* skip_word(char* p)
{
	char c;

	while((c = *p) && (c != ' '))
		p++;

	return p;
}

char* skip_field(char* p)
{
	p = skip_word(p);
	p = skip_space(p);

	return p;
}
