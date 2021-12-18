#include <string.h>

#include "common.h"

#define MAXCPU 16
#define GRAPHW 60

static struct cpustat {
	uint64_t idle;
	uint64_t busy;
} cpustat;

static struct cpudata {
	uint64_t idle;
	uint64_t busy;
	uint load;
} cpustats[MAXCPU];

static struct cpupt {
	byte max;
	byte avg;
} graph[GRAPHW];

static uint ncpus;
static uint graphptr;

static void update_cpu_deltas(uint idx)
{
	struct cpudata* cd = &cpustats[idx];

	uint64_t idle = cpustat.idle - cd->idle;
	uint64_t busy = cpustat.busy - cd->busy;

	cd->idle = cpustat.idle;
	cd->busy = cpustat.busy;

	uint64_t total = idle + busy;

	if(!total || !dtms)
		cd->load = 0;
	else
		cd->load = 1000*busy/total;
}

static void parse_stat_line(char* p)
{
	uint cpuidx;

	if(!(p = parse_int(p, &cpuidx)))
		return;
	if(cpuidx >= MAXCPU)
		return;
	if(cpuidx >= ncpus)
		ncpus = cpuidx;

	cpustat.busy = 0;
	cpustat.idle = 0;

	if(!(p = parse_add(p, &cpustat.busy)))
		return;
	if(!(p = parse_add(p, &cpustat.busy)))
		return;
	if(!(p = parse_add(p, &cpustat.busy)))
		return;
	if(!(p = parse_add(p, &cpustat.idle)))
		return;

	while((p = parse_add(p, &cpustat.busy)))
		;

	update_cpu_deltas(cpuidx);
}

static uint graph_scale(uint v)
{
	return v * (pix_height + 1) / 1000;
}

static void add_graph_line(void)
{
	uint max = 0;
	uint min = 1001;
	uint sum = 0;

	if(!ncpus) return;

	for(uint i = 0; i < ncpus; i++) {
		struct cpudata* cd = &cpustats[i];

		uint load = cd->load;

		if(load > max)
			max = load;
		if(load < min)
			min = load;

		sum += load;
	} if(min > 1000) {
		min = 0;
	}

	uint avg = sum / ncpus;

	struct cpupt* pt = &graph[graphptr];

	pt->max = graph_scale(max);
	pt->avg = graph_scale(avg);

	graphptr = (graphptr + 1) % GRAPHW;
}

static void parse_proc_stat(void)
{
	char* p = databuf;
	char* e = p + datalen;

	while(p < e) {
		char* q = skip_to_eol(p, e);

		if(!q) break;

		*q = '\0';

		if(strncmp(p, "cpu", 3))
			break;

		parse_stat_line(p + 3);

		p = q + 1;
	}

	add_graph_line();
}

static void redraw_graph(void)
{
	uint i, w = GRAPHW;
	uint h = pix_height;

	for(i = 0; i < w; i++) {
		uint k = (graphptr + i + 1) % GRAPHW;
		struct cpupt* pt = &graph[k];

		uint max = pt->max;
		uint avg = pt->avg;

		uint y = 0;
		uint x = 1 + i;

		setcolor(0x007BAC);

		while(y < avg)
			point(x, h - y++ - 1);

		setcolor(0x555555);

		while(y < max)
			point(x, h - y++ - 1);
	}

	advance(w + 2);
}

void put_cpuload(void)
{
	if(load_file("/proc/stat") < 0)
		return;

	parse_proc_stat();

	redraw_graph();
}
