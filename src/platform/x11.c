#include "../platform.h"

#ifndef __linux__
#error "x11.c is only for Linux"
#endif //__linux__

#include <GL/glx.h>
#include <X11/Xlib.h>
#include <sys/stat.h>
#include <time.h>

static Display *display;
static Window window;
// handle the polite close request properly to not just be killed by SIGTERM by
// the window manager
static Atom wm_delete_window;

static GLXContext glx_context;

static u32 width;
static u32 height;

static f64 last_frame_time;
static f64 frame_time;

static bool keys[KEY_COUNT];
// also track previous key states to handle 'just pressed' event properly
static bool previous_keys[KEY_COUNT];

static bool mouse_buttons[MOUSE_BUTTON_COUNT];
static bool previous_mouse_buttons[MOUSE_BUTTON_COUNT];

static i32 mouse_x;
static i32 mouse_y;
static i32 mouse_delta_x;
static i32 mouse_delta_y;

static i32 scroll_delta_x;
static i32 scroll_delta_y;

static bool mouse_captured;

static char text_input_buffer[TEXT_BUFFER_SIZE];
static u32 text_input_length = 0;
static bool text_input_available = false;

static key translate_key_code(const unsigned int keycode) {
    switch (keycode) {
    case 38:
        return KEY_A;
    case 56:
        return KEY_B;
    case 54:
        return KEY_C;
    case 40:
        return KEY_D;
    case 26:
        return KEY_E;
    case 41:
        return KEY_F;
    case 42:
        return KEY_G;
    case 43:
        return KEY_H;
    case 31:
        return KEY_I;
    case 44:
        return KEY_J;
    case 45:
        return KEY_K;
    case 46:
        return KEY_L;
    case 58:
        return KEY_M;
    case 57:
        return KEY_N;
    case 32:
        return KEY_O;
    case 33:
        return KEY_P;
    case 24:
        return KEY_Q;
    case 27:
        return KEY_R;
    case 39:
        return KEY_S;
    case 28:
        return KEY_T;
    case 30:
        return KEY_U;
    case 55:
        return KEY_V;
    case 25:
        return KEY_W;
    case 53:
        return KEY_X;
    case 29:
        return KEY_Y;
    case 52:
        return KEY_Z;
    case 19:
        return KEY_0;
    case 10:
        return KEY_1;
    case 11:
        return KEY_2;
    case 12:
        return KEY_3;
    case 13:
        return KEY_4;
    case 14:
        return KEY_5;
    case 15:
        return KEY_6;
    case 16:
        return KEY_7;
    case 17:
        return KEY_8;
    case 18:
        return KEY_9;
    case 65:
        return KEY_SPACE;
    case 36:
        return KEY_ENTER;
    case 9:
        return KEY_ESCAPE;
    case 23:
        return KEY_TAB;
    case 22:
        return KEY_BACKSPACE;
    case 113:
        return KEY_LEFT;
    case 114:
        return KEY_RIGHT;
    case 111:
        return KEY_UP;
    case 116:
        return KEY_DOWN;
    case 50:
        return KEY_LEFT_SHIFT;
    case 62:
        return KEY_RIGHT_SHIFT;
    case 37:
        return KEY_LEFT_CTRL;
    case 105:
        return KEY_RIGHT_CTRL;
    case 64:
        return KEY_LEFT_ALT;
    case 108:
        return KEY_RIGHT_ALT;
    case 67:
        return KEY_F1;
    case 68:
        return KEY_F2;
    case 69:
        return KEY_F3;
    case 70:
        return KEY_F4;
    case 71:
        return KEY_F5;
    case 72:
        return KEY_F6;
    case 73:
        return KEY_F7;
    case 74:
        return KEY_F8;
    case 75:
        return KEY_F9;
    case 76:
        return KEY_F10;
    case 95:
        return KEY_F11;
    case 96:
        return KEY_F12;
    default:
        return KEY_UNKNOWN;
    }
}

u0 initialize(u0) {
    width = 1280;
    height = 720;

    for (u32 i = 0; i < KEY_COUNT; i++) {
        keys[i] = false;
        previous_keys[i] = false;
    }

    for (u32 i = 0; i < MOUSE_BUTTON_COUNT; i++) {
        mouse_buttons[i] = False;
        previous_mouse_buttons[i] = False;
    }

    mouse_x = 0;
    mouse_y = 0;
    mouse_delta_x = 0;
    mouse_delta_y = 0;

    scroll_delta_x = 0;
    scroll_delta_y = 0;

    mouse_captured = false;

    text_input_buffer[0] = '\0';
    text_input_length = 0;
    text_input_available = false;

    const char *display_name = NULL;
    display = XOpenDisplay(display_name);

    const int attributes[] = {
        GLX_RENDER_TYPE,
        GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,
        GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER,
        true,
        GLX_DEPTH_SIZE,
        24,
        GLX_SAMPLE_BUFFERS,
        1,
        GLX_SAMPLES,
        4,
        None,
    };

    int framebuffer_count;
    const GLXFBConfig *framebuffer_configs = glXChooseFBConfig(
        display, DefaultScreen(display), attributes, &framebuffer_count);
    // this conflicts with clang-tidy:
    // ReSharper disable once CppLocalVariableMayBeConst
    GLXFBConfig framebuffer_config = framebuffer_configs[0];

    XVisualInfo *visual = glXGetVisualFromFBConfig(display, framebuffer_config);

    const Colormap colormap = XCreateColormap(
        display, DefaultRootWindow(display), visual->visual, AllocNone);

    XSetWindowAttributes set_window_attributes;
    set_window_attributes.colormap = colormap;
    set_window_attributes.event_mask =
        KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask |
        PointerMotionMask | StructureNotifyMask | FocusChangeMask;

    const int x = 0, y = 0;
    const unsigned int border_width = 0;
    window =
        XCreateWindow(display, DefaultRootWindow(display), x, y, width, height,
                      border_width, visual->depth, InputOutput, visual->visual,
                      CWColormap | CWEventMask, &set_window_attributes);

    XStoreName(display, window, "Game");

    const bool only_if_exists = false;
    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", only_if_exists);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    const bool direct = true;
    glx_context = glXCreateContext(display, visual, NULL, direct);
    glXMakeCurrent(display, window, glx_context);

    XMapWindow(display, window);
    XFlush(display);
}

bool update(u0) {
    struct timespec tp;
    clock_gettime(CLOCK_MONOTONIC, &tp);
    const f64 now = (f64)tp.tv_sec * 1000.0 + (f64)tp.tv_nsec / 1000000.0;

    frame_time = now - last_frame_time;
    last_frame_time = now;

    for (u32 i = 0; i < KEY_COUNT; i++) {
        previous_keys[i] = keys[i];
    }

    for (u32 i = 0; i < MOUSE_BUTTON_COUNT; i++) {
        previous_mouse_buttons[i] = mouse_buttons[i];
    }

    mouse_delta_x = 0;
    mouse_delta_y = 0;
    scroll_delta_x = 0;
    scroll_delta_y = 0;

    XEvent event;

    while (XPending(display)) {
        XNextEvent(display, &event);

        switch (event.type) {
        case ClientMessage: {
            if ((Atom)event.xclient.data.l[0] == wm_delete_window) {
                return false;
            }
        } break;

        case MotionNotify: {
            mouse_delta_x += event.xmotion.x - mouse_x;
            mouse_delta_y += event.xmotion.y - mouse_y;

            mouse_x = event.xmotion.x;
            mouse_y = event.xmotion.y;
        } break;

        case ConfigureNotify: {
            width = event.xconfigure.width;
            height = event.xconfigure.height;
            glViewport(0, 0, (GLsizei)width, (GLsizei)height);
        } break;

        case KeyPress: {
            const key key = translate_key_code(event.xkey.keycode);

            if (key != KEY_UNKNOWN) {
                keys[key] = true;
            }

            // key input
            if (key == KEY_BACKSPACE) {
                if (text_input_length > 0) {
                    text_input_length--;
                    text_input_buffer[text_input_length] = '\0';
                    text_input_available = true;
                }
            } else {
                // convert key event to character
                char buffer[8];
                KeySym keysym;
                const int length = XLookupString(
                    &event.xkey, buffer, sizeof(buffer) - 1, &keysym, NULL);

                if (length > 0) {
                    buffer[length] = '\0';

                    // only accept printable ascii characters
                    for (int i = 0; i < length; i++) {
                        if (buffer[i] >= 32 && buffer[i] < 127) {
                            if (text_input_length < TEXT_BUFFER_SIZE - 1) {
                                text_input_buffer[text_input_length] =
                                    buffer[i];
                                text_input_length++;

                                text_input_buffer[text_input_length] = '\0';
                                text_input_available = true;
                            }
                        }
                    }
                }
            }
        } break;

        case KeyRelease: {
            // if a KeyRelease is immediately followed by a KeyPress with the
            // same time and keycode it is an auto-repeat and both shall be
            // ignored
            if (XPending(display)) {
                XEvent next;
                XPeekEvent(display, &next);

                if (next.type == KeyPress &&
                    next.xkey.time == event.xkey.time &&
                    next.xkey.keycode == event.xkey.keycode) {
                    // consume a fake KeyPress
                    XNextEvent(display, &next);

                    break;
                }
            }

            const key key = translate_key_code(event.xkey.keycode);

            if (key != KEY_UNKNOWN) {
                keys[key] = false;
            }
        } break;

        case ButtonPress: {
            if (event.xbutton.button == Button1) {
                mouse_buttons[MOUSE_LEFT] = true;
            } else if (event.xbutton.button == Button2) {
                mouse_buttons[MOUSE_MIDDLE] = true;
            } else if (event.xbutton.button == Button3) {
                mouse_buttons[MOUSE_RIGHT] = true;
            } else if (event.xbutton.button == 4) {
                scroll_delta_y += 1;
            } else if (event.xbutton.button == 5) {
                scroll_delta_y -= 1;
            } else if (event.xbutton.button == 6) {
                scroll_delta_x -= 1;
            } else if (event.xbutton.button == 7) {
                scroll_delta_x += 1;
            }
        } break;

        case FocusOut: {
            for (u32 i = 0; i < KEY_COUNT; i++) {
                keys[i] = false;
            }
            for (u32 i = 0; i < MOUSE_BUTTON_COUNT; i++) {
                mouse_buttons[i] = false;
            }
        } break;

        case ButtonRelease: {
            if (event.xbutton.button == Button1) {
                mouse_buttons[MOUSE_LEFT] = false;
            } else if (event.xbutton.button == Button2) {
                mouse_buttons[MOUSE_MIDDLE] = false;
            } else if (event.xbutton.button == Button3) {
                mouse_buttons[MOUSE_RIGHT] = false;
            }
        } break;

        default:
            break;
        }
    }

    if (mouse_captured) {
        const i32 center_x = (i32)width / 2;
        const i32 center_y = (i32)height / 2;
        XWarpPointer(display, None, window, 0, 0, 0, 0, center_x, center_y);
        mouse_x = center_x;
        mouse_y = center_y;
    }

    return true;
}

u0 swap_buffers(u0) { glXSwapBuffers(display, window); }

u0 quit(u0) {
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, glx_context);
    XDestroyWindow(display, window);
    XCloseDisplay(display);
}

u0 get_window_size(u32 *w, u32 *h) {
    *w = width;
    *h = height;
}

f64 get_frame_time_ms(u0) { return frame_time; }

bool is_key_down(const key key) {
    if (key >= KEY_COUNT) {
        return false;
    }

    return keys[key];
}

bool is_key_pressed(const key key) {
    if (key >= KEY_COUNT) {
        return false;
    }

    return keys[key] && !previous_keys[key];
}

bool is_key_released(const key key) {
    if (key >= KEY_COUNT) {
        return false;
    }

    return !keys[key] && previous_keys[key];
}

u0 get_mouse_delta(i32 *dx, i32 *dy) {
    *dx = mouse_delta_x;
    *dy = mouse_delta_y;
}

u0 get_mouse_position(i32 *x, i32 *y) {
    *x = mouse_x;
    *y = mouse_y;
}

bool is_mouse_button_down(const u32 button) {
    if (button >= MOUSE_BUTTON_COUNT) {
        return false;
    }

    return mouse_buttons[button];
}

bool is_mouse_button_pressed(const u32 button) {
    if (button >= MOUSE_BUTTON_COUNT) {
        return false;
    }

    return mouse_buttons[button] && !previous_mouse_buttons[button];
}

bool is_mouse_button_released(const u32 button) {
    if (button >= MOUSE_BUTTON_COUNT) {
        return false;
    }

    return !mouse_buttons[button] && previous_mouse_buttons[button];
}

u0 set_mouse_captured(const bool captured) {
    mouse_captured = captured;

    if (captured) {
        const Pixmap pixmap = XCreatePixmap(display, window, 1, 1, 1);
        XColor color = {0};
        const Cursor cursor =
            XCreatePixmapCursor(display, pixmap, pixmap, &color, &color, 0, 0);
        XDefineCursor(display, window, cursor);
        XFreePixmap(display, pixmap);

        const i32 center_x = (i32)width / 2;
        const i32 center_y = (i32)height / 2;
        XWarpPointer(display, None, window, 0, 0, 0, 0, center_x, center_y);
        mouse_x = center_x;
        mouse_y = center_y;
    } else {
        XUndefineCursor(display, window);
    }
}

bool is_mouse_captured(u0) { return mouse_captured; }

u0 get_scroll_delta(i32 *dx, i32 *dy) {
    *dx = scroll_delta_x;
    *dy = scroll_delta_y;
}

u0 get_text_input(char *buffer, u32 *length) {
    if (buffer != NULL) {
        for (u32 i = 0; i <= text_input_length; i++) {
            buffer[i] = text_input_buffer[i];
        }
    }

    if (length != NULL) {
        *length = text_input_length;
    }
}

u0 clear_text_input(u0) {
    text_input_buffer[0] = '\0';
    text_input_length = 0;
    text_input_available = false;
}

bool has_text_input(u0) { return text_input_available; }
