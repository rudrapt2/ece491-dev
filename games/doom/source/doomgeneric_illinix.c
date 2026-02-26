#include "doomkeys.h"
#include "m_argv.h"
#include "doomgeneric.h"
#include "../usr/syscall.h"
#include "../usr/viohi.h"

#include "../usr/string.h"
#include "../usr/io.h"

#define QUEUE_SIZE 16
//temp
#define IOCTL_GETFBUF       100 // arg is void **
#define IOCTL_MAPFBUF       101 // arg is void **
#define IOCTL_GETFBCONF     102 // arg is struct fbuf_conf *
#define IOCTL_SETFBCONF     103 // arg is const struc fbuf_conf *

static unsigned short s_KeyQueue[QUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

static unsigned short s_MouseQueue[QUEUE_SIZE];
static unsigned int s_MouseQueueWriteIndex = 0;
static unsigned int s_MouseQueueReadIndex = 0;

unsigned long long ticks;
int gpu_fd;
int input_pipe_out;
int alarm_pipe_in;

static const unsigned char doom_key_map[VKEY_SCALE + 1] = {
  [VKEY_RESERVED] = 0,
  [VKEY_ESC] = KEY_ESCAPE,
  [VKEY_1] = '1',
  [VKEY_2] = '2',
  [VKEY_3] = '3',
  [VKEY_4] = '4',
  [VKEY_5] = '5',
  [VKEY_6] = '6',
  [VKEY_7] = '7',
  [VKEY_8] = '8',
  [VKEY_9] = '9',
  [VKEY_0] = '0',
  [VKEY_MINUS] = KEY_MINUS,
  [VKEY_EQUAL] = KEY_EQUALS,
  [VKEY_BACKSPACE] = KEY_BACKSPACE,
  [VKEY_TAB] = KEY_TAB,
  [VKEY_Q] = 'q',
  [VKEY_W] = 'w',
  [VKEY_E] = 'e',
  [VKEY_R] = 'r',
  [VKEY_T] = 't',
  [VKEY_Y] = 'y',
  [VKEY_U] = 'u',
  [VKEY_I] = 'i',
  [VKEY_O] = 'o',
  [VKEY_P] = 'p',
  [VKEY_LEFTBRACE] = '[',
  [VKEY_RIGHTBRACE] = ']',
  [VKEY_ENTER] = KEY_ENTER,
  [VKEY_LEFTCTRL] = KEY_FIRE,
  [VKEY_A] = 'a',
  [VKEY_S] = 's',
  [VKEY_D] = 'd',
  [VKEY_F] = 'f',
  [VKEY_G] = 'g',
  [VKEY_H] = 'h',
  [VKEY_J] = 'j',
  [VKEY_K] = 'k',
  [VKEY_L] = 'l',
  [VKEY_SEMICOLON] = ';',
  [VKEY_APOSTROPHE] = '\'',
  [VKEY_GRAVE] = '`',
  [VKEY_LEFTSHIFT] = KEY_RSHIFT,
  [VKEY_BACKSLASH] = '\\',
  [VKEY_Z] = 'z',
  [VKEY_X] = 'x',
  [VKEY_C] = 'c',
  [VKEY_V] = 'v',
  [VKEY_B] = 'b',
  [VKEY_N] = 'n',
  [VKEY_M] = 'm',
  [VKEY_COMMA] = ',',
  [VKEY_DOT] = '.',
  [VKEY_SLASH] = '/',
  [VKEY_RIGHTSHIFT] = KEY_RSHIFT,
  [VKEY_KPASTERISK] = KEYP_MULTIPLY,
  [VKEY_LEFTALT] = KEY_LALT,
  [VKEY_SPACE] = KEY_USE,
  [VKEY_CAPSLOCK] = KEY_CAPSLOCK,
  [VKEY_F1] = KEY_F1,
  [VKEY_F2] = KEY_F2,
  [VKEY_F3] = KEY_F3,
  [VKEY_F4] = KEY_F4,
  [VKEY_F5] = KEY_F5,
  [VKEY_F6] = KEY_F6,
  [VKEY_F7] = KEY_F7,
  [VKEY_F8] = KEY_F8,
  [VKEY_F9] = KEY_F9,
  [VKEY_F10] = KEY_F10,
  [VKEY_NUMLOCK] = KEY_NUMLOCK,
  [VKEY_SCROLLLOCK] = KEY_SCRLCK,
  [VKEY_KP7] = KEYP_7,
  [VKEY_KP8] = KEYP_8,
  [VKEY_KP9] = KEYP_9,
  [VKEY_KPMINUS] = KEYP_MINUS,
  [VKEY_KP4] = KEYP_4,
  [VKEY_KP5] = KEYP_5,
  [VKEY_KP6] = KEYP_6,
  [VKEY_KPPLUS] = KEYP_PLUS,
  [VKEY_KP1] = KEYP_1,
  [VKEY_KP2] = KEYP_2,
  [VKEY_KP3] = KEYP_3,
  [VKEY_KP0] = KEYP_0,
  [VKEY_KPDOT] = KEYP_PERIOD,
  [VKEY_F11] = KEY_F11,
  [VKEY_F12] = KEY_F12,
  [VKEY_KPENTER] = KEYP_ENTER,
  [VKEY_RIGHTCTRL] = KEY_FIRE,
  [VKEY_KPSLASH] = KEYP_DIVIDE,
  [VKEY_RIGHTALT] = KEY_RALT,
  [VKEY_HOME] = KEY_HOME,
  [VKEY_UP] = KEY_UPARROW,
  [VKEY_PAGEUP] = KEY_PGUP,
  [VKEY_LEFT] = KEY_LEFTARROW,
  [VKEY_RIGHT] = KEY_RIGHTARROW,
  [VKEY_END] = KEY_END,
  [VKEY_DOWN] = KEY_DOWNARROW,
  [VKEY_PAGEDOWN] = KEY_PGDN,
  [VKEY_INSERT] = KEY_INS,
  [VKEY_DELETE] = KEY_DEL,
  [VKEY_KPEQUAL] = KEYP_EQUALS,
  [VKEY_PAUSE] = KEY_PAUSE,
};

void DG_Init(){
  gpu_fd = _open(-1, "dev/viogpu0");

  if (gpu_fd < 0) {
    _print("failed to open gpu device");
    _exit();
  }
  
  int result = _ioctl(gpu_fd, IOC_MAPBUF, &DG_ScreenBuffer);
  if (result != 0) {
    _print("failed to obtain frame buffer");
    _close(gpu_fd);
    _exit();
  }


}

static unsigned char convertToDoomKey(unsigned int key)
{
  return doom_key_map[key];
}

//This is borrowed from xlib version of doomgeneric
static void addKeyToQueue(int pressed, unsigned int keyCode)
{
	unsigned char key = convertToDoomKey(keyCode);

	unsigned short keyData = (pressed << 8) | key;

	s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
	s_KeyQueueWriteIndex++;
	s_KeyQueueWriteIndex %= QUEUE_SIZE;
}

static void sleepGetKey(uint32_t us) {
  struct viohi_event evt;
  _write(alarm_pipe_in, &us, sizeof(uint32_t));
  while (1) {
    _read(input_pipe_out, &evt, sizeof(evt));
    if (evt.code == 0) return; // alarm value
    if (evt.code >= BTN_MOUSE && evt.code <= BTN_TASK) continue; //TODO mouse input
    else {
      if(evt.value){
        addKeyToQueue(1, evt.code);
      } else {
        addKeyToQueue(0, evt.code);
      }
    }
  }
}

void DG_DrawFrame()
{
  sleepGetKey(0);
  _write(gpu_fd, NULL, 0);
}

void DG_SleepMs(uint32_t ms)
{
  sleepGetKey(1000*ms);
}

uint32_t DG_GetTicksMs()
{
  unsigned long long time;
  asm ("rdtime %0" : "=r"(time));
  return (uint32_t)(time / 10000);
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
  if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex) {
    //key queue is empty
    return 0;
  }
  else {
    unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
    s_KeyQueueReadIndex++;
    s_KeyQueueReadIndex %= QUEUE_SIZE;

    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;

    return 1;
  }

  return 0;
}

void DG_SetWindowTitle(const char * title) 
{ 
  // unused
}

void main(int argc, char **argv)
{
  input_pipe_out = (uint8_t)argv[0][0];
  alarm_pipe_in = (uint8_t)argv[1][0];
  printf("input pipe out is: %d\n", input_pipe_out);
  printf("alarm pipe in is: %d\n", alarm_pipe_in);

  doomgeneric_Create(argc - 2, argv + 2);
  
  for (;;)
    doomgeneric_Tick();    

}