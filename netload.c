#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <sys/un.h>
#include <string.h>

#include "common.h"

#define MAXDEV 4
#define GRAPHW 60

#define MISSING 0
#define PRESENT 1
#define RUNNING 2

static struct netdev {
	char ifname[IFNAMSIZ];
	uint active;

	uint64_t rx;
	uint64_t tx;

	uint ptr;

	struct netpt {
		byte rx;
		byte tx;
	} graph[GRAPHW];

} netdevs[MAXDEV];

static int sockfd;

static char* strcbrk(char* p, char x)
{
	char c;

	while((c = *p) && (c != x))
		p++;

	return c ? p : NULL;
}

static struct netdev* find_device_slot(char* ifn)
{
	for(uint i = 0; i < MAXDEV; i++) {
		struct netdev* nd = &netdevs[i];

		if(strncmp(nd->ifname, ifn, sizeof(nd->ifname)))
			continue;

		return nd;
	}

	return NULL;
}

static struct netdev* grab_device_slot(char* ifn)
{
	for(uint i = 0; i < MAXDEV; i++) {
		struct netdev* nd = &netdevs[i];

		if(nd->ifname[0])
			continue;

		strncpy(nd->ifname, ifn, sizeof(nd->ifname));

		return nd;
	}

	return NULL;
}

uint binlog(uint64_t v)
{
	uint ret = 0;

	while(v) {
		ret++;
		v = v >> 1;
	}

	return ret;
}

uint log_scale(uint64_t total)
{
	if(!total)
		return 0;

	uint log = binlog(total);

	if(log < 8)
		return 1;

	uint max = pix_height - 1;
	uint bar = log - 7;

	if(bar >= max)
		return max;

	return bar;
}

static int socket_fd(void)
{
	int fd;

	if((fd = sockfd))
		return fd;

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);

	sockfd = fd;

	return fd;
}

static uint get_dev_flags(char* ifn)
{
	struct ifreq ifr;
	int ret;

	int fd = socket_fd();
	int len = strlen(ifn);

	if(len > sizeof(ifr.ifr_ifrn))
		return IFF_LOOPBACK;

	memset(&ifr, 0, sizeof(ifr));
	memcpy(ifr.ifr_name, ifn, len);

	if((ret = ioctl(fd, SIOCGIFFLAGS, &ifr)) < 0)
		return IFF_LOOPBACK;

	return ifr.ifr_ifru.ifru_ivalue;
}

uint calc_txbar(uint64_t rx, uint64_t tx, uint bar)
{
	uint64_t total = rx + tx;

	if(!total) return 0;

	uint txbar = (bar*tx)/total;

	if(bar == 1)
		return rx > tx ? 0 : 1;

	if(txbar > bar)
		txbar = bar;

	return txbar;
}

static void add_graph_point(char* ifn, uint64_t rx, uint64_t tx)
{
	struct netdev* nd;
	uint64_t drx, dtx;
	uint flags = get_dev_flags(ifn);

	if(flags & IFF_LOOPBACK)
		return;

	if((nd = find_device_slot(ifn))) {
		drx = rx - nd->rx;
		dtx = tx - nd->tx;
		nd->rx = rx;
		nd->tx = tx;
	} else if(!(nd = grab_device_slot(ifn))) {
		return;
	} else { /* initial values */
		drx = 0;
		dtx = 0;
		nd->rx = rx;
		nd->tx = tx;
	}

	if(flags & IFF_RUNNING)
		nd->active = RUNNING;
	else
		nd->active = PRESENT;

	uint ptr = nd->ptr;

	uint bar = log_scale(drx + dtx);
	uint gtx = calc_txbar(drx, dtx, bar);
	uint grx = bar - gtx;

	struct netpt* pt = &nd->graph[ptr];

	pt->rx = grx;
	pt->tx = gtx;

	nd->ptr = (ptr + 1) % GRAPHW;
}

static void parse_net_line(char* p)
{
	char* ifn = skip_space(p);
	uint64_t rx = 0;
	uint64_t tx = 0;

	if(!(p = strcbrk(ifn, ':')))
		return;

	*p++ = '\0';

	p = skip_space(p);

	if(!(p = parse_add(p, &rx)))
		return;

	for(uint i = 0; i < 7; i++) {
		if(!(p = skip_word(p)))
			return;
		p = skip_space(p);
	}

	if(!(p = parse_add(p, &tx)))
		return;

	add_graph_point(ifn, rx, tx);
}

static void parse_net_stats(void)
{
	char* p = databuf;
	char* e = p + datalen;
	uint line = 0;

	/* skip header */
	while(p < e) {
		if(line++ > 2) break;

		char* q = skip_to_eol(p, e);

		if(!q) return;

		p = q + 1;
	}

	while(p < e) {
		char* q = skip_to_eol(p, e);

		if(!q) break;

		*q = '\0';

		p = skip_space(p);

		parse_net_line(p);

		p = q + 1;
	}
}

static void remove_dev_marks(void)
{
	int i, n = MAXDEV;

	for(i = 0; i < n; i++)
		netdevs[i].active = MISSING;
}

static void drop_stale_entries(void)
{
	int i, n = MAXDEV;

	for(i = 0; i < n; i++) {
		struct netdev* nd = &netdevs[i];

		if(nd->active != MISSING)
			continue;

		memset(nd, 0, sizeof(*nd));
	}
}

static void redraw_net_graph(struct netdev* nd)
{
	uint i, w = GRAPHW;
	uint h = pix_height;

	for(i = 0; i < w; i++) {
		uint ptr = nd->ptr;
		uint k = (ptr + i + 1) % GRAPHW;
		struct netpt* pt = &nd->graph[k];

		uint rx = pt->rx;
		uint tx = pt->tx;

		uint y = 0;
		uint x = 1 + i;

		setcolor(0xB4893B);

		while(tx-- > 0)
			point(x, h - y++ - 1);

		setcolor(0xA91598);

		while(rx-- > 0)
			point(x, h - y++ - 1);
	}

	advance(w + 2);
}

static void redraw_net_graphs(void)
{
	uint i, n = MAXDEV;

	for(i = 0; i < n; i++) {
		struct netdev* nd = &netdevs[i];

		if(nd->active != RUNNING)
			continue;

		redraw_net_graph(nd);
	}
}

void put_netload(void)
{
	if(load_file("/proc/net/dev") < 0)
		return;

	remove_dev_marks();

	parse_net_stats();

	drop_stale_entries();

	redraw_net_graphs();
}
