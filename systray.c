#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <err.h>

#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <xcb/xcb_image.h>

#include "common.h"
#include "panel.h"

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*a))

/* Very simple systray area implementation. Loosely based on tint2 code.

   Fixed number of icon slots, dock requests above that limit are ignored
   (so the icons will be left floating I guess). Doesn't really matter since
   for me there's like one, maaaaybe two of them at a time.

   tint2 does a lot more things, for unknown reasons, possibly to support
   various client quirks. This was essentially only written to support Wine
   tray icons, so it doest just enough to make Wine icons work. The whole
   concept doesn't seem to be very well documented.

   The client windows are wrapped in a container windows (parent window or
   parwin below), which clip them and also provide some degree of isolation
   from the clients. The only interaction with the client-owned icon window
   is reparenting it, everything else is done on the parwin. */

int total_icons;

struct atoms {
	int systray_s0;
	int systray_opcode;
	int manager;
} atom;

struct icon {
	int cliwin;
	int parwin;
	int width;
	int offset;
} icons[10];

static struct icon* grab_icon_slot(int cliwin)
{
	int i, n = 10;

	for(i = 0; i < n; i++) {
		struct icon* ico = &icons[i];

		if(ico->cliwin == cliwin)
			return NULL;
	}

	for(i = 0; i < n; i++) {
		struct icon* ico = &icons[i];

		if(!ico->parwin)
			return ico;
	}

	return NULL;
}

static int intern_atom(char* name)
{
	int nlen = strlen(name);

	xcb_intern_atom_cookie_t cookie;
	xcb_intern_atom_reply_t* reply;
	xcb_generic_error_t* err = NULL;

	cookie = xcb_intern_atom(conn, 0, nlen, name);
	reply = xcb_intern_atom_reply(conn, cookie, &err);

	return reply->atom;
}

static int get_systray_owner(void)
{
	xcb_get_selection_owner_cookie_t cookie;
	xcb_get_selection_owner_reply_t* reply;
	xcb_generic_error_t* err = NULL;

	cookie = xcb_get_selection_owner(conn, atom.systray_s0);
	reply = xcb_get_selection_owner_reply(conn, cookie, &err);

	return reply->owner;
}

static void announce_systray(void)
{
	int mask = XCB_EVENT_MASK_STRUCTURE_NOTIFY;
	int dstwin = screen->root;
	int propagate = 0;

	xcb_client_message_event_t ev = {
		.response_type = XCB_CLIENT_MESSAGE,
		.sequence = 0,
		.format = 32,
		.window = screen->root,
		.type = atom.manager,
		.data.data32[0] = XCB_CURRENT_TIME,
		.data.data32[1] = atom.systray_s0,
		.data.data32[2] = panwin,
		.data.data32[3] = 0,
		.data.data32[4] = 0
	};

	xcb_send_event(conn, propagate, dstwin, mask, (void*)&ev);
}

void init_systray(void)
{
	atom.systray_s0 = intern_atom("_NET_SYSTEM_TRAY_S0");
	atom.systray_opcode = intern_atom("_NET_SYSTEM_TRAY_OPCODE");
	atom.manager = intern_atom("MANAGER");

	if(get_systray_owner())
		errx(-1, "another systray is already running");

	xcb_set_selection_owner(conn, panwin, atom.systray_s0, XCB_CURRENT_TIME);

	if(get_systray_owner() != panwin)
		errx(-1, "cannot claim systray ownership");

	announce_systray();
}

static void xcbx_select_events(int win)
{
	int eventmask = XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY;

	int mask = XCB_CW_EVENT_MASK;
	int values[1] = { eventmask };

	xcb_change_window_attributes(conn, win, mask, values);
}

static void add_tray_icon(int cliwin)
{
	struct icon* ico;

	if(!(ico = grab_icon_slot(cliwin)))
		return; /* no slots left, ignore request */

	uint mask = XCB_CW_BACK_PIXEL;
	uint values[1] = { screen->black_pixel };
	int parwin = xcb_generate_id(conn);

	int width = H;
	int offset = total_icons;

	xcb_create_window(conn,
	                  screen->root_depth,
	                  parwin,
	                  panwin,
	                  offset, 0, width, H, 0,
	                  XCB_WINDOW_CLASS_INPUT_OUTPUT,
			  screen->root_visual,
	                  mask, values);

	ico->cliwin = cliwin;
	ico->parwin = parwin;
	ico->width = width;
	ico->offset = offset;

	total_icons += width;

	xcb_unmap_window(conn, cliwin);

	xcbx_select_events(parwin);

	xcb_reparent_window(conn, cliwin, parwin, 0, 0);

	xcb_map_window(conn, parwin);
	xcb_map_window(conn, cliwin);

	redraw_window();
}

static void xcbx_move_window(int win, int x, int y)
{
	int mask = XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y;
	int values[2] = { x, y };

	xcb_configure_window(conn, win, mask, values);
}

static void recalc_icon_offsets(void)
{
	int i, j = 0, n = ARRAY_SIZE(icons);
	int offset = 0;

	for(i = 0; i < n; i++) {
		struct icon* ico = &icons[i];

		if(!ico->parwin)
			continue;

		if(ico->offset != offset) {
			xcbx_move_window(ico->parwin, offset, 0);
			ico->offset = offset;
		}

		offset += ico->width;

		if(j < i) {
			icons[j++] = *ico;
			memset(ico, 0, sizeof(*ico));
		} else {
			j++; /* leave it in place */
		}
	}

	total_icons = offset;
}

static void del_tray_icon(struct icon* ico)
{
	xcb_destroy_window(conn, ico->parwin);

	memset(ico, 0, sizeof(*ico));

	recalc_icon_offsets();

	redraw_window();
}

void handle_client_message(xcb_client_message_event_t* ev)
{
	if(ev->window != panwin)
		return;
	if(ev->type != atom.systray_opcode)
		return;
	if(ev->format != 32)
		return;

	int opcode = ev->data.data32[1];
	int cliwin = ev->data.data32[2];

	if(opcode == 0)
		add_tray_icon(cliwin);
	/* else don't care / not supported */
}

static struct icon* find_icon(int cliwin)
{
	int i, n = ARRAY_SIZE(icons);

	for(i = 0; i < n; i++) {
		struct icon* ico = &icons[i];

		if(!(ico->parwin))
			continue;
		if(ico->cliwin == cliwin)
			return ico;
	}

	return NULL;
}

void handle_reparent_notify(xcb_reparent_notify_event_t* ev)
{
	struct icon* ico;

	if(ev->window == panwin)
		return; /* panel reparent */

	if(!(ico = find_icon(ev->window)))
		return;

	if(ico->parwin == ev->parent)
		return; /* reparented successfully? */

	del_tray_icon(ico);
}

void handle_destroy_notify(xcb_destroy_notify_event_t* ev)
{
	struct icon* ico;

	if(!(ico = find_icon(ev->window)))
		return;

	del_tray_icon(ico);
}
