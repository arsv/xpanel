#include <stdint.h>

typedef unsigned int uint;
typedef unsigned char byte;

extern uint pix_width;
extern uint pix_wused;
extern uint pix_height;
extern uint* image;
extern uint dtms;

extern char databuf[2048];
extern uint datalen;

void advance(uint width);
void moveto(uint x, uint y);
void setcolor(uint c);
void point(uint x, uint y);
void bitmap(byte* data, uint w, uint h);

int load_file(char* name);
char* skip_to_eol(char* p, char* e);
char* parse_int(char* p, uint* v);
char* parse_add(char* p, uint64_t* v);
char* skip_space(char* p);
char* skip_word(char* p);
char* skip_field(char* p);

void init_mailbox(void);
void put_clock(void);
void put_battery(void);
void put_cpuload(void);
void put_netload(void);
void put_mailbox(void);
