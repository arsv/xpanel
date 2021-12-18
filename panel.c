#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <poll.h>
#include <err.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <xcb/xcb_image.h>

#include "common.h"

#define W 500
#define H 20

uint timer_fd;
uint xconn_fd;

xcb_connection_t* conn;
xcb_screen_t* screen;
xcb_window_t win;
xcb_gcontext_t gc;
xcb_pixmap_t pix;

uint dtms;
struct timespec prevtime;

char databuf[2048];
uint datalen;

uint win_width;
uint win_height;
uint win_mapped;

uint pix_width;
uint pix_height;
uint pix_wused;

uint* image;

static void clear_image(void)
{
	uint w = pix_width;
	uint h = pix_height;

	for(uint i = 0; i < w*h; i++)
		image[i] = 0;

	pix_wused = 0;
}

static void init_connection(void)
{
	const xcb_setup_t* setup;
	xcb_screen_iterator_t iter;

	conn = xcb_connect(NULL, NULL);
	setup = xcb_get_setup(conn);

	iter = xcb_setup_roots_iterator(setup);
	screen = iter.data;

	xconn_fd = xcb_get_file_descriptor(conn);
}

static void create_window(void)
{
	uint mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	uint values[2] = { screen->black_pixel, XCB_EVENT_MASK_EXPOSURE };
	/* xcb-util-wm, but I'm not bringing a whole library in just for this */
	uint wmhints[9] = { (1<<1), 0, 0, 0, 0, 0, 0, 0, 0 };

	win = xcb_generate_id(conn);
	gc = xcb_generate_id(conn);

	xcb_create_window(conn,
	                  screen->root_depth,
	                  win,
	                  screen->root,
	                  0, 0, H, H, 0,
	                  XCB_WINDOW_CLASS_INPUT_OUTPUT,
			  screen->root_visual,
	                  mask, values);

	/* one of these calls should be enough, but let's do both */

	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
			XCB_ATOM_WM_HINTS, XCB_ATOM_WM_HINTS, 32,
			 sizeof(wmhints) >> 2, &wmhints);

	xcb_change_property(conn, XCB_PROP_MODE_REPLACE, win,
			XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8,
			15, "dockapp\0DockApp");

	xcb_create_gc(conn, gc, win, 0, NULL);

	xcb_map_window(conn, win);

	xcb_flush(conn);

	win_width = H;
	win_height = H;
}

static void init_image_buf(void)
{
	xcb_shm_query_version_reply_t* reply;
	xcb_shm_segment_info_t info;

	reply = xcb_shm_query_version_reply(conn,
			xcb_shm_query_version(conn), NULL);

	if(!reply || !reply->shared_pixmaps)
		errx(-1, "shm error");

	info.shmid = shmget(IPC_PRIVATE, W*H*4, IPC_CREAT | 0777);
	info.shmaddr = shmat(info.shmid, 0, 0);
	info.shmseg = xcb_generate_id(conn);

	image = (void*)info.shmaddr;

	xcb_shm_attach(conn, info.shmseg, info.shmid, 0);

	pix = xcb_generate_id(conn);

	xcb_shm_create_pixmap(conn, pix, win, W, H,
			screen->root_depth, info.shmseg, 0);

	shmctl(info.shmid, IPC_RMID, 0);

	pix_width = W;
	pix_wused = 0;
	pix_height = H;
}

static void repaint_window(void)
{
	if(!pix_wused) return;

	uint w = win_width;
	uint h = win_height;

	xcb_copy_area(conn, pix, win, gc, 0, 0, 0, 0, w, h);

	xcb_flush(conn);
}

static void resize_window(uint width)
{
	uint value_mask = XCB_CONFIG_WINDOW_WIDTH;
	uint value_list[] = { width };

	xcb_configure_window(conn, win, value_mask, value_list);

	if(!win_width)
		xcb_map_window(conn, win);

	win_width = width;
}

static void redraw_window(void)
{
	if(pix_wused == win_width) {
		repaint_window();
	} else if(pix_wused) {
		resize_window(pix_wused);
		repaint_window();
	} else {
		win_width = 0;
		xcb_unmap_window(conn, win);
		xcb_flush(conn);
	}
}

static void check_xconn(void)
{
	xcb_generic_event_t* evt;

	while((evt = xcb_poll_for_event(conn))) {
		uint type = evt->response_type;

		if(type == XCB_EXPOSE)
			repaint_window();
	}
}

static void update_dtms(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	uint64_t s0 = prevtime.tv_sec;
	uint64_t s1 = ts.tv_sec;
	uint64_t dtns;

	if(s0 + 1 == s1) {
		dtns = 1000000000 + ts.tv_nsec - prevtime.tv_nsec;
	} else if(s0 == s1) {
		dtns = ts.tv_nsec - prevtime.tv_nsec;
	} else {
		dtns = 0;
	}

	dtms = dtns / 1000000;

	prevtime = ts;
}

static void update_image(void)
{
	update_dtms();
	clear_image();

	put_mailbox();
	put_netload();
	put_cpuload();
	put_battery();
	put_clock();
}

static void check_timer(void)
{
	byte buf[32];
	int ret, fd = timer_fd;

	if((ret = read(fd, buf, sizeof(buf))) < 0)
		err(-1, "read timerfd");
	if(!ret)
		return;

	update_image();
	redraw_window();
}

static void open_timer_fd(void)
{
	int fd, ret;

	struct itimerspec its = {
		.it_interval = { 0, 500000000 },
		.it_value = { 1, 500000000 }
	};

	if((fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK)) < 0)
		err(-1, "timerfd_create");
	if((ret = timerfd_settime(fd, TFD_TIMER_ABSTIME, &its, NULL)) < 0)
		err(-1, "timerfd_settime");

	timer_fd = fd;
}

static void poll_fds(void)
{
	int ret;
	struct pollfd pfds[2] = {
		{ .events = POLLIN, .fd = timer_fd },
		{ .events = POLLIN, .fd = xconn_fd }
	};

	if((ret = poll(pfds, 2, -1)) < 0)
		err(-1, "poll");

	if(pfds[0].revents & POLLIN)
		check_timer();
	if(pfds[0].revents & ~POLLIN)
		errx(-1, "lost timerfd");

	if(pfds[1].revents & POLLIN)
		check_xconn();
	if(pfds[1].revents & ~POLLIN)
		errx(-1, "lost xconnfd");
}

int main(void)
{
	init_connection();
	create_window();
	init_image_buf();
	init_mailbox();

	open_timer_fd();

	update_image();
	redraw_window();

	while(1) poll_fds();
}
