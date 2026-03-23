#ifndef GAME_PLATFORM_H
#define GAME_PLATFORM_H

#include "types.h"
#include <glad/glad.h>

typedef u32 key;

#define KEY_UNKNOWN 0
#define KEY_A 1
#define KEY_B 2
#define KEY_C 3
#define KEY_D 4
#define KEY_E 5
#define KEY_F 6
#define KEY_G 7
#define KEY_H 8
#define KEY_I 9
#define KEY_J 10
#define KEY_K 11
#define KEY_L 12
#define KEY_M 13
#define KEY_N 14
#define KEY_O 15
#define KEY_P 16
#define KEY_Q 17
#define KEY_R 18
#define KEY_S 19
#define KEY_T 20
#define KEY_U 21
#define KEY_V 22
#define KEY_W 23
#define KEY_X 24
#define KEY_Y 25
#define KEY_Z 26
#define KEY_0 27
#define KEY_1 28
#define KEY_2 29
#define KEY_3 30
#define KEY_4 31
#define KEY_5 32
#define KEY_6 33
#define KEY_7 34
#define KEY_8 35
#define KEY_9 36
#define KEY_SPACE 37
#define KEY_ENTER 38
#define KEY_ESCAPE 39
#define KEY_TAB 40
#define KEY_BACKSPACE 41
#define KEY_LEFT 42
#define KEY_RIGHT 43
#define KEY_UP 44
#define KEY_DOWN 45
#define KEY_LEFT_SHIFT 46
#define KEY_RIGHT_SHIFT 47
#define KEY_LEFT_CTRL 48
#define KEY_RIGHT_CTRL 49
#define KEY_LEFT_ALT 50
#define KEY_RIGHT_ALT 51
#define KEY_F1 52
#define KEY_F2 53
#define KEY_F3 54
#define KEY_F4 55
#define KEY_F5 56
#define KEY_F6 57
#define KEY_F7 58
#define KEY_F8 59
#define KEY_F9 60
#define KEY_F10 61
#define KEY_F11 62
#define KEY_F12 63
#define KEY_COUNT 64

bool is_key_down(key key);

bool is_key_pressed(key key);

bool is_key_released(key key);

u0 get_mouse_delta(i32 *dx, i32 *dy);

u0 get_scroll_delta(i32 *dx, i32 *dy);

u0 get_mouse_position(i32 *x, i32 *y);

u0 set_mouse_captured(bool captured);

bool is_mouse_captured(u0);

#define MOUSE_LEFT 0
#define MOUSE_MIDDLE 1
#define MOUSE_RIGHT 2
#define MOUSE_BUTTON_COUNT 3

bool is_mouse_button_down(u32 button);

bool is_mouse_button_pressed(u32 button);

bool is_mouse_button_released(u32 button);

u0 initialize(u0);

bool update(u0);

u0 quit(u0);

u0 swap_buffers(u0);

u0 get_window_size(u32 *w, u32 *h);

f64 get_frame_time_ms(u0);

#define TEXT_BUFFER_SIZE 256

u0 get_text_input(char *buffer, u32 *length);

u0 clear_text_input(u0);

bool has_text_input(u0);

GLADloadproc get_proc_address(u0);

#endif // GAME_PLATFORM_H
