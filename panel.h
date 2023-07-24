#define W 500
#define H 20

extern xcb_connection_t* conn;
extern xcb_screen_t* screen;
extern xcb_window_t panwin;
extern xcb_gcontext_t gc;
extern xcb_pixmap_t pix;

extern int total_icons;

void redraw_window(void);
void init_systray(void);
void handle_client_message(xcb_client_message_event_t* ev);
void handle_reparent_notify(xcb_reparent_notify_event_t* ev);
void handle_destroy_notify(xcb_destroy_notify_event_t* ev);
