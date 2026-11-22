#define HL_NAME(n) sdl_##n

#include <hl.h>
#include "hlsystem.h"

#include <locale.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_vulkan.h>

#ifdef HL_ANDROID
#	define SDL_MAIN_HANDLED
#	include <SDL3/SDL_main.h>
#endif

#if defined(HL_IOS) || defined(HL_TVOS)
#	include <OpenGLES/ES3/gl.h>
#	include <OpenGLES/ES3/glext.h>
#endif

#ifndef SDL_MAJOR_VERSION
#	error "SDL3 SDK not found in hl/include/sdl/"
#endif

#define TWIN _ABSTRACT(sdl_window)
#define TGL _ABSTRACT(sdl_gl)
#define _SURF _ABSTRACT(sdl_surface)

typedef struct {
	int x;
	int y;
	int w;
	int h;
	int style;
} wsave_pos;

typedef enum {
	Quit,
	MouseMove,
	MouseLeave,
	MouseDown,
	MouseUp,
	MouseWheel,
	WindowState,
	KeyDown,
	KeyUp,
	TextInput,
	GControllerAdded = 100,
	GControllerRemoved,
	GControllerDown,
	GControllerUp,
	GControllerAxis,
	TouchDown = 200,
	TouchUp,
	TouchMove,
	JoystickAxisMotion = 300,
	JoystickBallMotion,
	JoystickHatMotion,
	JoystickButtonDown,
	JoystickButtonUp,
	JoystickAdded,
	JoystickRemoved,
	DropStart = 400,
	DropFile,
	DropText,
	DropEnd,
	KeyMapChanged = 500,
} event_type;

typedef enum {
	Show,
	Hide,
	Expose,
	Move,
	Resize,
	Minimize,
	Maximize,
	Restore,
	Enter,
	Leave,
	Focus,
	Blur,
	Close
} ws_change;

typedef struct {
	hl_type *t;
	event_type type;
	int mouseX;
	int mouseY;
	int mouseXRel;
	int mouseYRel;
	int button;
	double wheelDelta;
	double wheelDeltaX;
	ws_change state;
	int keyCode;
	int scanCode;
	bool keyRepeat;
	int reference;
	int value;
	int __unused;
	int window;
	vbyte *dropFile;
} event_data;

static bool isGlOptionsSet = false;
static bool hint_window_grab_keyboard = false;
static bool relative_mouse_mode = false;

static vclosure *live_resize_cb = NULL;
static bool in_live_watch = false;

static bool SDLCALL sdl_live_event_watch(void *userdata, SDL_Event *e) {
	(void)userdata;
	if (!in_live_watch && live_resize_cb != NULL && hl_get_thread() != NULL) {
		if (e->type == SDL_EVENT_WINDOW_EXPOSED || e->type == SDL_EVENT_WINDOW_RESIZED) {
			in_live_watch = true;
			hl_call0(void, live_resize_cb);
			in_live_watch = false;
		}
	}
	return true;
}

HL_PRIM void HL_NAME(set_live_resize_callback)(vclosure *cb) {
	if (live_resize_cb != NULL) {
		hl_remove_root(&live_resize_cb);
		SDL_RemoveEventWatch(sdl_live_event_watch, NULL);
		live_resize_cb = NULL;
	}
	if (cb != NULL) {
		live_resize_cb = cb;
		hl_add_root(&live_resize_cb);
		SDL_AddEventWatch(sdl_live_event_watch, NULL);
	}
}

HL_PRIM bool HL_NAME(init_once)() {
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
#ifdef HL_ANDROID
	SDL_SetMainReady();
#endif
	if (!SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
		hl_error("SDL_Init failed: %s", hl_to_utf16(SDL_GetError()));
		return false;
	}
	setlocale(LC_ALL, "C");
#ifdef _WIN32
	timeBeginPeriod(1);
#endif
	if (!isGlOptionsSet) {
#ifdef HL_MOBILE
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
#endif
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	}
	return true;
}

HL_PRIM void HL_NAME(gl_options)(int major, int minor, int depth, int stencil, int flags, int samples) {
	isGlOptionsSet = true;
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, depth);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, stencil);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, (flags & 1) ? 1 : 0);
	if (flags & 2)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	else if (flags & 4)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	else if (flags & 8)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	else {
#ifdef HL_MOBILE
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif
	}

	if (samples > 1) {
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, samples);
	}
}

HL_PRIM bool HL_NAME(hint_value)(vbyte *name, vbyte *value) {
	if (strcmp((char*)name, "SDL_GRAB_KEYBOARD") == 0)
		hint_window_grab_keyboard = value != NULL;
	return SDL_SetHint((char*)name, (char*)value);
}

HL_PRIM int HL_NAME(event_poll)(SDL_Event *e) {
	return SDL_PollEvent(e) ? 1 : 0;
}

HL_PRIM bool HL_NAME(event_loop)(event_data *event) {
	while (true) {
		SDL_Event e;
		if (!SDL_PollEvent(&e)) break;

		if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST) {
			event->type = WindowState;
			event->window = (int)e.window.windowID;
			switch (e.type) {
			case SDL_EVENT_WINDOW_SHOWN:
				event->state = Show;
				break;
			case SDL_EVENT_WINDOW_HIDDEN:
				event->state = Hide;
				break;
			case SDL_EVENT_WINDOW_EXPOSED:
				event->state = Expose;
				break;
			case SDL_EVENT_WINDOW_MOVED:
				event->state = Move;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				event->state = Resize;
				break;
			case SDL_EVENT_WINDOW_MINIMIZED:
				event->state = Minimize;
				break;
			case SDL_EVENT_WINDOW_MAXIMIZED:
				event->state = Maximize;
				break;
			case SDL_EVENT_WINDOW_RESTORED:
				event->state = Restore;
				break;
			case SDL_EVENT_WINDOW_MOUSE_ENTER:
				event->state = Enter;
				break;
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				event->state = Leave;
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				event->state = Focus;
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				event->state = Blur;
				break;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				event->state = Close;
				break;
			default:
				continue;
			}
			return true;
		}

		switch (e.type) {
		case SDL_EVENT_QUIT:
			event->type = Quit;
			break;
		case SDL_EVENT_MOUSE_MOTION:
			event->type = MouseMove;
			event->window = (int)e.motion.windowID;
			event->mouseX = (int)SDL_roundf(e.motion.x);
			event->mouseY = (int)SDL_roundf(e.motion.y);
			event->mouseXRel = (int)SDL_roundf(e.motion.xrel);
			event->mouseYRel = (int)SDL_roundf(e.motion.yrel);
			break;
		case SDL_EVENT_KEY_DOWN:
			event->type = KeyDown;
			event->window = (int)e.key.windowID;
			event->keyCode = (int)e.key.key;
			event->scanCode = (int)e.key.scancode;
			event->keyRepeat = e.key.repeat;
			break;
		case SDL_EVENT_KEY_UP:
			event->type = KeyUp;
			event->window = (int)e.key.windowID;
			event->keyCode = (int)e.key.key;
			event->scanCode = (int)e.key.scancode;
			event->keyRepeat = false;
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
			event->type = MouseDown;
			event->window = (int)e.button.windowID;
			event->button = e.button.button;
			event->mouseX = (int)SDL_roundf(e.button.x);
			event->mouseY = (int)SDL_roundf(e.button.y);
			break;
		case SDL_EVENT_MOUSE_BUTTON_UP:
			event->type = MouseUp;
			event->window = (int)e.button.windowID;
			event->button = e.button.button;
			event->mouseX = (int)SDL_roundf(e.button.x);
			event->mouseY = (int)SDL_roundf(e.button.y);
			break;
		case SDL_EVENT_FINGER_DOWN:
			event->type = TouchDown;
			event->mouseX = (int)SDL_roundf(e.tfinger.x * 10000.0f);
			event->mouseY = (int)SDL_roundf(e.tfinger.y * 10000.0f);
			event->reference = (int)e.tfinger.fingerID;
			break;
		case SDL_EVENT_FINGER_MOTION:
			event->type = TouchMove;
			event->mouseX = (int)SDL_roundf(e.tfinger.x * 10000.0f);
			event->mouseY = (int)SDL_roundf(e.tfinger.y * 10000.0f);
			event->reference = (int)e.tfinger.fingerID;
			break;
		case SDL_EVENT_FINGER_UP:
			event->type = TouchUp;
			event->mouseX = (int)SDL_roundf(e.tfinger.x * 10000.0f);
			event->mouseY = (int)SDL_roundf(e.tfinger.y * 10000.0f);
			event->reference = (int)e.tfinger.fingerID;
			break;
		case SDL_EVENT_MOUSE_WHEEL: {
			double wy = (double)e.wheel.y;
			double wx = (double)e.wheel.x;
			if (e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED) {
				wy = -wy;
				wx = -wx;
			}
			event->type = MouseWheel;
			event->window = (int)e.wheel.windowID;
			event->wheelDelta = wy;
			event->wheelDeltaX = wx;
			break;
		}
		case SDL_EVENT_TEXT_EDITING:
			continue;
		case SDL_EVENT_TEXT_INPUT: {
			int code = 0;
			size_t len = strlen(e.text.text);
			if (len > 4) len = 4;
			memcpy(&code, e.text.text, len);
			event->type = TextInput;
			event->window = (int)e.text.windowID;
			event->keyCode = code;
			break;
		}
		case SDL_EVENT_GAMEPAD_ADDED:
			event->type = GControllerAdded;
			event->reference = (int)e.gdevice.which;
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			event->type = GControllerRemoved;
			event->reference = (int)e.gdevice.which;
			break;
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			event->type = GControllerDown;
			event->reference = (int)e.gbutton.which;
			event->button = e.gbutton.button;
			break;
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
			event->type = GControllerUp;
			event->reference = (int)e.gbutton.which;
			event->button = e.gbutton.button;
			break;
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			event->type = GControllerAxis;
			event->reference = (int)e.gaxis.which;
			event->button = e.gaxis.axis;
			event->value = e.gaxis.value;
			break;
		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
			event->type = JoystickAxisMotion;
			event->reference = (int)e.jaxis.which;
			event->button = e.jaxis.axis;
			event->value = e.jaxis.value;
			break;
		case SDL_EVENT_JOYSTICK_BALL_MOTION:
			event->type = JoystickBallMotion;
			event->reference = (int)e.jball.which;
			event->button = e.jball.ball;
			event->mouseXRel = e.jball.xrel;
			event->mouseYRel = e.jball.yrel;
			break;
		case SDL_EVENT_JOYSTICK_HAT_MOTION:
			event->type = JoystickHatMotion;
			event->reference = (int)e.jhat.which;
			event->button = e.jhat.hat;
			event->value = e.jhat.value;
			break;
		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
			event->type = JoystickButtonDown;
			event->reference = (int)e.jbutton.which;
			event->button = e.jbutton.button;
			break;
		case SDL_EVENT_JOYSTICK_BUTTON_UP:
			event->type = JoystickButtonUp;
			event->reference = (int)e.jbutton.which;
			event->button = e.jbutton.button;
			break;
		case SDL_EVENT_JOYSTICK_ADDED:
			event->type = JoystickAdded;
			event->reference = (int)e.jdevice.which;
			break;
		case SDL_EVENT_JOYSTICK_REMOVED:
			event->type = JoystickRemoved;
			event->reference = (int)e.jdevice.which;
			break;
		case SDL_EVENT_DROP_BEGIN:
			event->type = DropStart;
			event->window = (int)e.drop.windowID;
			break;
		case SDL_EVENT_DROP_FILE:
		case SDL_EVENT_DROP_TEXT: {
			vbyte *bytes = hl_copy_bytes((vbyte*)e.drop.data, (int)strlen(e.drop.data) + 1);
			event->type = e.type == SDL_EVENT_DROP_FILE ? DropFile : DropText;
			event->dropFile = bytes;
			event->window = (int)e.drop.windowID;
			break;
		}
		case SDL_EVENT_DROP_COMPLETE:
			event->type = DropEnd;
			event->window = (int)e.drop.windowID;
			break;
		case SDL_EVENT_KEYMAP_CHANGED:
			event->type = KeyMapChanged;
			break;
		default:
			continue;
		}
		return true;
	}
	return false;
}

HL_PRIM void HL_NAME(quit)() {
	if (live_resize_cb != NULL) {
		hl_remove_root(&live_resize_cb);
		SDL_RemoveEventWatch(sdl_live_event_watch, NULL);
		live_resize_cb = NULL;
	}
	SDL_Quit();
#ifdef _WIN32
	timeEndPeriod(1);
#endif
}

HL_PRIM void HL_NAME(delay)(int time) {
	hl_blocking(true);
	SDL_Delay((Uint32)time);
	hl_blocking(false);
}

HL_PRIM int HL_NAME(get_screen_width)() {
	int count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&count);
	const SDL_DisplayMode *e = NULL;
	if (displays != NULL && count > 0)
		e = SDL_GetCurrentDisplayMode(displays[0]);
	SDL_free(displays);
	return e != NULL ? e->w : 0;
}

HL_PRIM int HL_NAME(get_screen_height)() {
	int count = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&count);
	const SDL_DisplayMode *e = NULL;
	if (displays != NULL && count > 0)
		e = SDL_GetCurrentDisplayMode(displays[0]);
	SDL_free(displays);
	return e != NULL ? e->h : 0;
}

HL_PRIM int HL_NAME(get_screen_width_of_window)(SDL_Window *win) {
	SDL_DisplayID id = win != NULL ? SDL_GetDisplayForWindow(win) : SDL_GetPrimaryDisplay();
	const SDL_DisplayMode *e = SDL_GetCurrentDisplayMode(id);
	return e != NULL ? e->w : 0;
}

HL_PRIM int HL_NAME(get_screen_height_of_window)(SDL_Window *win) {
	SDL_DisplayID id = win != NULL ? SDL_GetDisplayForWindow(win) : SDL_GetPrimaryDisplay();
	const SDL_DisplayMode *e = SDL_GetCurrentDisplayMode(id);
	return e != NULL ? e->h : 0;
}

HL_PRIM int HL_NAME(get_framerate)(SDL_Window *win) {
	SDL_DisplayID id = win != NULL ? SDL_GetDisplayForWindow(win) : SDL_GetPrimaryDisplay();
	const SDL_DisplayMode *e = SDL_GetCurrentDisplayMode(id);
	return e != NULL ? (int)SDL_roundf(e->refresh_rate) : 60;
}

HL_PRIM void HL_NAME(message_box)(vbyte *title, vbyte *text, bool error) {
	hl_blocking(true);
	SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : 0, (char*)title, (char*)text, NULL);
	hl_blocking(false);
}

HL_PRIM void HL_NAME(set_vsync)(bool v) {
	SDL_GL_SetSwapInterval(v ? 1 : 0);
}

HL_PRIM bool HL_NAME(detect_win32)() {
#ifdef _WIN32
	return true;
#else
	return false;
#endif
}

HL_PRIM void HL_NAME(text_input)(bool enable) {
	SDL_SetEventEnabled(SDL_EVENT_TEXT_INPUT, enable);
	SDL_SetEventEnabled(SDL_EVENT_TEXT_EDITING, enable);
	SDL_Window **windows = SDL_GetWindows(NULL);
	if (windows) {
		for (int i = 0; windows[i]; ++i) {
			if (enable)
				SDL_StartTextInput(windows[i]);
			else
				SDL_StopTextInput(windows[i]);
		}
		SDL_free(windows);
	}
}

HL_PRIM int HL_NAME(set_relative_mouse_mode)(bool enable) {
	int retval = 0;
	SDL_Window **windows = SDL_GetWindows(NULL);
	if (windows) {
		for (int i = 0; windows[i]; ++i) {
			if (!SDL_SetWindowRelativeMouseMode(windows[i], enable)) {
				retval = -1;
			}
		}
		SDL_free(windows);
	}
	if (retval == 0) {
		relative_mouse_mode = enable;
	}
	return retval;
}

HL_PRIM bool HL_NAME(get_relative_mouse_mode)() {
	return relative_mouse_mode;
}

HL_PRIM int HL_NAME(capture_mouse)(bool enable) {
	return SDL_CaptureMouse(enable) ? 0 : -1;
}

HL_PRIM int HL_NAME(warp_mouse_global)(int x, int y) {
	return SDL_WarpMouseGlobal((float)x, (float)y) ? 0 : -1;
}

HL_PRIM void HL_NAME(warp_mouse_in_window)(SDL_Window *window, int x, int y) {
	SDL_WarpMouseInWindow(window, (float)x, (float)y);
}

HL_PRIM void HL_NAME(set_window_grab)(SDL_Window *window, bool grabbed) {
	if (hint_window_grab_keyboard)
		SDL_SetWindowKeyboardGrab(window, grabbed);
	SDL_SetWindowMouseGrab(window, grabbed);
}

HL_PRIM bool HL_NAME(get_window_grab)(SDL_Window *window) {
	return SDL_GetWindowMouseGrab(window);
}

HL_PRIM int HL_NAME(get_global_mouse_state)(int *x, int *y) {
	float fx = 0.0f, fy = 0.0f;
	SDL_MouseButtonFlags retval = SDL_GetGlobalMouseState(&fx, &fy);
	*x = (int)SDL_roundf(fx);
	*y = (int)SDL_roundf(fy);
	return (int)retval;
}

HL_PRIM const char *HL_NAME(detect_keyboard_layout)() {
	SDL_Keycode q = SDL_GetKeyFromScancode(SDL_SCANCODE_Q, SDL_KMOD_NONE, false);
	SDL_Keycode w = SDL_GetKeyFromScancode(SDL_SCANCODE_W, SDL_KMOD_NONE, false);
	SDL_Keycode y = SDL_GetKeyFromScancode(SDL_SCANCODE_Y, SDL_KMOD_NONE, false);

	if (q == 'q' && w == 'w' && y == 'y') return "qwerty";
	if (q == 'a' && w == 'z' && y == 'y') return "azerty";
	if (q == 'q' && w == 'w' && y == 'z') return "qwertz";
	if (q == 'q' && w == 'z' && y == 'y') return "qzerty";
	return "unknown";
}

DEFINE_PRIM(_BOOL, init_once, _NO_ARG);
DEFINE_PRIM(_VOID, gl_options, _I32 _I32 _I32 _I32 _I32 _I32);
DEFINE_PRIM(_BOOL, event_loop, _DYN);
DEFINE_PRIM(_I32, event_poll, _STRUCT);
DEFINE_PRIM(_VOID, quit, _NO_ARG);
DEFINE_PRIM(_VOID, delay, _I32);
DEFINE_PRIM(_I32, get_screen_width, _NO_ARG);
DEFINE_PRIM(_I32, get_screen_height, _NO_ARG);
DEFINE_PRIM(_I32, get_screen_width_of_window, TWIN);
DEFINE_PRIM(_I32, get_screen_height_of_window, TWIN);
DEFINE_PRIM(_I32, get_framerate, TWIN);
DEFINE_PRIM(_VOID, message_box, _BYTES _BYTES _BOOL);
DEFINE_PRIM(_VOID, set_vsync, _BOOL);
DEFINE_PRIM(_BOOL, detect_win32, _NO_ARG);
DEFINE_PRIM(_VOID, text_input, _BOOL);
DEFINE_PRIM(_I32, set_relative_mouse_mode, _BOOL);
DEFINE_PRIM(_BOOL, get_relative_mouse_mode, _NO_ARG);
DEFINE_PRIM(_I32, capture_mouse, _BOOL);
DEFINE_PRIM(_I32, warp_mouse_global, _I32 _I32);
DEFINE_PRIM(_VOID, warp_mouse_in_window, TWIN _I32 _I32);
DEFINE_PRIM(_VOID, set_window_grab, TWIN _BOOL);
DEFINE_PRIM(_BOOL, get_window_grab, TWIN);
DEFINE_PRIM(_I32, get_global_mouse_state, _REF(_I32) _REF(_I32));
DEFINE_PRIM(_BYTES, detect_keyboard_layout, _NO_ARG);
DEFINE_PRIM(_BOOL, hint_value, _BYTES _BYTES);
DEFINE_PRIM(_VOID, set_live_resize_callback, _FUN(_VOID, _NO_ARG));

// Window

HL_PRIM SDL_Window *HL_NAME(win_create_ex)(int x, int y, int width, int height, int sdlFlags) {
	if ((sdlFlags & (
#ifdef HL_MAC
		SDL_WINDOW_METAL |
#endif
		SDL_WINDOW_VULKAN)) == 0) {
		sdlFlags |= SDL_WINDOW_OPENGL;
	}

#ifdef HL_MOBILE
	SDL_Window *win = SDL_CreateWindow("", width, height, SDL_WINDOW_BORDERLESS | (SDL_WindowFlags)sdlFlags);
#else
	SDL_PropertiesID props = SDL_CreateProperties();
	SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "");
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER, width);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER, height);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_X_NUMBER, x);
	SDL_SetNumberProperty(props, SDL_PROP_WINDOW_CREATE_Y_NUMBER, y);
	if (sdlFlags & SDL_WINDOW_OPENGL)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_OPENGL_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_VULKAN)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_VULKAN_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_BORDERLESS)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_BORDERLESS_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_FULLSCREEN)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_HIDDEN)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIDDEN_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_RESIZABLE)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_RESIZABLE_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_MINIMIZED)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MINIMIZED_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_MAXIMIZED)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MAXIMIZED_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_MOUSE_FOCUS)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MOUSE_GRABBED_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_HIGH_PIXEL_DENSITY)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_HIGH_PIXEL_DENSITY_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_ALWAYS_ON_TOP)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_ALWAYS_ON_TOP_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_UTILITY)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_UTILITY_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_TOOLTIP)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_TOOLTIP_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_POPUP_MENU)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MENU_BOOLEAN, true);
	if (sdlFlags & SDL_WINDOW_MODAL)
		SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_MODAL_BOOLEAN, true);

	SDL_Window *win = SDL_CreateWindowWithProperties(props);
	SDL_DestroyProperties(props);
#endif

#ifdef HL_WIN
	if ((SDL_GetWindowFlags(win) & SDL_WINDOW_INPUT_FOCUS) == 0) {
		SDL_HideWindow(win);
		SDL_ShowWindow(win);
	}
	SDL_RaiseWindow(win);
#endif

	SDL_StartTextInput(win);
	return win;
}

HL_PRIM SDL_Window *HL_NAME(win_create)(int width, int height) {
	return HL_NAME(win_create_ex)(SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, 0);
}

HL_PRIM SDL_GLContext HL_NAME(win_get_glcontext)(SDL_Window *win) {
	return SDL_GL_CreateContext(win);
}

HL_PRIM bool HL_NAME(win_set_fullscreen)(SDL_Window *win, int mode) {
#ifdef HL_WIN
	SDL_PropertiesID props = SDL_GetWindowProperties(win);
	HWND wnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
	wsave_pos *save = (wsave_pos*)SDL_GetPointerProperty(props, "save", NULL);
	if (save && mode != 2) {
		SetWindowLong(wnd, GWL_STYLE, save->style);
		SetWindowPos(wnd, NULL, save->x, save->y, save->w, save->h, 0);
		SDL_SetWindowSize(win, save->w, save->h);
		free(save);
		SDL_SetPointerProperty(props, "save", NULL);
		save = NULL;
	}
#endif

	switch (mode) {
	case 0: // WINDOWED
		return SDL_SetWindowFullscreen(win, false);

	case 1: { // FULLSCREEN
		SDL_DisplayID display = SDL_GetDisplayForWindow(win);
		const SDL_DisplayMode *fs_mode = SDL_GetDesktopDisplayMode(display);
		if (fs_mode != NULL)
			SDL_SetWindowFullscreenMode(win, fs_mode);
		return SDL_SetWindowFullscreen(win, true);
	}

	case 2: // BORDERLESS
#ifdef HL_WIN
	{
		if (save != NULL)
			return true;
		if (!SDL_SetWindowFullscreen(win, false))
			return false;
		HMONITOR hmon = MonitorFromWindow(wnd, MONITOR_DEFAULTTONEAREST);
		MONITORINFO mi = { sizeof(mi) };
		RECT r;
		if (!GetMonitorInfo(hmon, &mi))
			return false;
		GetWindowRect(wnd, &r);
		save = (wsave_pos*)malloc(sizeof(wsave_pos));
		save->x = r.left;
		save->y = r.top;
		save->w = r.right - r.left;
		save->h = r.bottom - r.top;
		save->style = GetWindowLong(wnd, GWL_STYLE);
		SDL_SetPointerProperty(props, "save", save);
		SetWindowLong(wnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		SetWindowPos(wnd, NULL, mi.rcMonitor.left, mi.rcMonitor.top,
			mi.rcMonitor.right - mi.rcMonitor.left,
			mi.rcMonitor.bottom - mi.rcMonitor.top + 2, 0);
		return true;
	}
#else
		SDL_SetWindowFullscreenMode(win, NULL);
		return SDL_SetWindowFullscreen(win, true);
#endif
	}
	return false;
}

HL_PRIM bool HL_NAME(win_set_display_mode)(SDL_Window *win, int width, int height, int framerate) {
	SDL_DisplayMode mode;
	SDL_DisplayID display = SDL_GetDisplayForWindow(win);
	if (SDL_GetClosestFullscreenDisplayMode(display, width, height, (float)framerate, true, &mode)) {
		return SDL_SetWindowFullscreenMode(win, &mode);
	}
	return false;
}

HL_PRIM int HL_NAME(win_display_handle)(SDL_Window *win) {
	return (int)SDL_GetDisplayForWindow(win);
}

HL_PRIM void HL_NAME(win_set_title)(SDL_Window *win, vbyte *title) {
	SDL_SetWindowTitle(win, (char*)title);
}

HL_PRIM void HL_NAME(win_set_icon)(SDL_Window *win, SDL_Surface *s) {
	SDL_SetWindowIcon(win, s);
}

HL_PRIM void HL_NAME(win_set_position)(SDL_Window *win, int x, int y) {
	SDL_SetWindowPosition(win, x, y);
}

HL_PRIM void HL_NAME(win_get_position)(SDL_Window *win, int *x, int *y) {
	SDL_GetWindowPosition(win, x, y);
}

HL_PRIM void HL_NAME(win_set_size)(SDL_Window *win, int width, int height) {
	SDL_WindowFlags flags = SDL_GetWindowFlags(win);
	if (flags & SDL_WINDOW_MAXIMIZED) {
		SDL_RestoreWindow(win);
	}
	SDL_SetWindowSize(win, width, height);
}

HL_PRIM void HL_NAME(win_set_min_size)(SDL_Window *win, int width, int height) {
	SDL_SetWindowMinimumSize(win, width, height);
}

HL_PRIM void HL_NAME(win_set_max_size)(SDL_Window *win, int width, int height) {
	SDL_SetWindowMaximumSize(win, width, height);
}

HL_PRIM void HL_NAME(win_set_maximized)(SDL_Window *win, bool maximize) {
	if (maximize) {
		SDL_MaximizeWindow(win);
	} else {
		SDL_RestoreWindow(win);
	}
}

HL_PRIM bool HL_NAME(win_maximized)(SDL_Window *win) {
	return (SDL_GetWindowFlags(win) & SDL_WINDOW_MAXIMIZED) != 0;
}

HL_PRIM void HL_NAME(win_get_size)(SDL_Window *win, int *width, int *height) {
	SDL_GetWindowSize(win, width, height);
}

HL_PRIM void HL_NAME(win_get_min_size)(SDL_Window *win, int *width, int *height) {
	SDL_GetWindowMinimumSize(win, width, height);
}

HL_PRIM void HL_NAME(win_get_max_size)(SDL_Window *win, int *width, int *height) {
	SDL_GetWindowMaximumSize(win, width, height);
}

HL_PRIM double HL_NAME(win_get_display_scale)(SDL_Window *win) {
	return (double)SDL_GetWindowDisplayScale(win);
}

HL_PRIM double HL_NAME(win_get_opacity)(SDL_Window *win) {
	return (double)SDL_GetWindowOpacity(win);
}

HL_PRIM bool HL_NAME(win_set_opacity)(SDL_Window *win, double opacity) {
	return SDL_SetWindowOpacity(win, (float)opacity);
}

HL_PRIM void HL_NAME(win_resize)(SDL_Window *win, int mode) {
	switch (mode) {
	case 0:
		SDL_MaximizeWindow(win);
		break;
	case 1:
		SDL_MinimizeWindow(win);
		break;
	case 2:
		SDL_RestoreWindow(win);
		break;
	case 3:
		SDL_ShowWindow(win);
		break;
	case 4:
		SDL_HideWindow(win);
		break;
	default:
		break;
	}
}

HL_PRIM void HL_NAME(win_raise)(SDL_Window *win) {
	SDL_RaiseWindow(win);
}

HL_PRIM void HL_NAME(win_swap_window)(SDL_Window *win) {
#if defined(HL_IOS) || defined(HL_TVOS)
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	SDL_GetWindowWMInfo(win, &info);
	glBindFramebuffer(GL_FRAMEBUFFER, info.info.uikit.framebuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, info.info.uikit.colorbuffer);
#endif
	SDL_GL_SwapWindow(win);
}

HL_PRIM void HL_NAME(win_render_to)(SDL_Window *win, SDL_GLContext gl) {
	SDL_GL_MakeCurrent(win, gl);
}

HL_PRIM int HL_NAME(win_get_id)(SDL_Window *window) {
	return (int)SDL_GetWindowID(window);
}

HL_PRIM void HL_NAME(win_destroy)(SDL_Window *win, SDL_GLContext gl) {
	SDL_StopTextInput(win);
	SDL_DestroyWindow(win);
	if (gl != NULL)
		SDL_GL_DestroyContext(gl);
}

DEFINE_PRIM(TWIN, win_create_ex, _I32 _I32 _I32 _I32 _I32);
DEFINE_PRIM(TWIN, win_create, _I32 _I32);
DEFINE_PRIM(TGL, win_get_glcontext, TWIN);
DEFINE_PRIM(_BOOL, win_set_fullscreen, TWIN _I32);
DEFINE_PRIM(_BOOL, win_set_display_mode, TWIN _I32 _I32 _I32);
DEFINE_PRIM(_I32, win_display_handle, TWIN);
DEFINE_PRIM(_VOID, win_resize, TWIN _I32);
DEFINE_PRIM(_VOID, win_raise, TWIN);
DEFINE_PRIM(_VOID, win_set_title, TWIN _BYTES);
DEFINE_PRIM(_VOID, win_set_icon, TWIN _SURF);
DEFINE_PRIM(_VOID, win_set_position, TWIN _I32 _I32);
DEFINE_PRIM(_VOID, win_get_position, TWIN _REF(_I32) _REF(_I32));
DEFINE_PRIM(_VOID, win_set_size, TWIN _I32 _I32);
DEFINE_PRIM(_VOID, win_set_min_size, TWIN _I32 _I32);
DEFINE_PRIM(_VOID, win_set_max_size, TWIN _I32 _I32);
DEFINE_PRIM(_VOID, win_set_maximized, TWIN _BOOL);
DEFINE_PRIM(_BOOL, win_maximized, TWIN);
DEFINE_PRIM(_VOID, win_get_size, TWIN _REF(_I32) _REF(_I32));
DEFINE_PRIM(_VOID, win_get_min_size, TWIN _REF(_I32) _REF(_I32));
DEFINE_PRIM(_VOID, win_get_max_size, TWIN _REF(_I32) _REF(_I32));
DEFINE_PRIM(_F64, win_get_display_scale, TWIN);
DEFINE_PRIM(_F64, win_get_opacity, TWIN);
DEFINE_PRIM(_BOOL, win_set_opacity, TWIN _F64);
DEFINE_PRIM(_VOID, win_swap_window, TWIN);
DEFINE_PRIM(_VOID, win_render_to, TWIN TGL);
DEFINE_PRIM(_VOID, win_destroy, TWIN TGL);
DEFINE_PRIM(_I32, win_get_id, TWIN);

// Vulkan

HL_PRIM varray *HL_NAME(vk_get_instance_extensions)() {
	Uint32 count = 0;
	char const * const *names = SDL_Vulkan_GetInstanceExtensions(&count);
	if (!names) return NULL;
	varray *arr = hl_alloc_array(&hlt_bytes, (int)count);
	for (Uint32 i = 0; i < count; i++)
		hl_aptr(arr, vbyte*)[i] = (vbyte*)names[i];
	return arr;
}

DEFINE_PRIM(_ARR, vk_get_instance_extensions, _NO_ARG);

#ifdef HL_64
HL_PRIM VkSurfaceKHR HL_NAME(win_vk_create_surface)(SDL_Window *win, VkInstance instance) {
	VkSurfaceKHR surface = NULL;
	if (!SDL_Vulkan_CreateSurface(win, instance, NULL, &surface))
		return NULL;
	return surface;
}

DEFINE_PRIM(_ABSTRACT(vk_surface), win_vk_create_surface, TWIN _ABSTRACT(vk_instance));
#endif

// Game controller (SDL3 Gamepad)

HL_PRIM int HL_NAME(gctrl_count)() {
	int count = 0;
	SDL_JoystickID *pads = SDL_GetGamepads(&count);
	SDL_free(pads);
	return count;
}

HL_PRIM SDL_Gamepad *HL_NAME(gctrl_open)(int id) {
	return SDL_OpenGamepad((SDL_JoystickID)id);
}

HL_PRIM void HL_NAME(gctrl_close)(SDL_Gamepad *controller) {
	SDL_CloseGamepad(controller);
}

HL_PRIM int HL_NAME(gctrl_get_axis)(SDL_Gamepad *controller, int axisIdx) {
	return SDL_GetGamepadAxis(controller, (SDL_GamepadAxis)axisIdx);
}

HL_PRIM bool HL_NAME(gctrl_get_button)(SDL_Gamepad *controller, int btnIdx) {
	return SDL_GetGamepadButton(controller, (SDL_GamepadButton)btnIdx);
}

HL_PRIM int HL_NAME(gctrl_get_id)(SDL_Gamepad *controller) {
	return (int)SDL_GetGamepadID(controller);
}

HL_PRIM vbyte *HL_NAME(gctrl_get_name)(SDL_Gamepad *controller) {
	return (vbyte*)SDL_GetGamepadName(controller);
}

#define TGCTRL _ABSTRACT(sdl_gamecontroller)
DEFINE_PRIM(_I32, gctrl_count, _NO_ARG);
DEFINE_PRIM(TGCTRL, gctrl_open, _I32);
DEFINE_PRIM(_VOID, gctrl_close, TGCTRL);
DEFINE_PRIM(_I32, gctrl_get_axis, TGCTRL _I32);
DEFINE_PRIM(_BOOL, gctrl_get_button, TGCTRL _I32);
DEFINE_PRIM(_I32, gctrl_get_id, TGCTRL);
DEFINE_PRIM(_BYTES, gctrl_get_name, TGCTRL);

HL_PRIM SDL_Haptic *HL_NAME(haptic_open)(SDL_Gamepad *controller) {
	return SDL_OpenHapticFromJoystick(SDL_GetGamepadJoystick(controller));
}

HL_PRIM void HL_NAME(haptic_close)(SDL_Haptic *haptic) {
	SDL_CloseHaptic(haptic);
}

HL_PRIM int HL_NAME(haptic_rumble_init)(SDL_Haptic *haptic) {
	return SDL_InitHapticRumble(haptic) ? 0 : -1;
}

HL_PRIM int HL_NAME(haptic_rumble_play)(SDL_Haptic *haptic, double strength, int length) {
	return SDL_PlayHapticRumble(haptic, (float)strength, (Uint32)length) ? 0 : -1;
}

#define THAPTIC _ABSTRACT(sdl_haptic)
DEFINE_PRIM(THAPTIC, haptic_open, TGCTRL);
DEFINE_PRIM(_VOID, haptic_close, THAPTIC);
DEFINE_PRIM(_I32, haptic_rumble_init, THAPTIC);
DEFINE_PRIM(_I32, haptic_rumble_play, THAPTIC _F64 _I32);

// Joystick

HL_PRIM int HL_NAME(joy_count)() {
	int count = 0;
	SDL_JoystickID *sticks = SDL_GetJoysticks(&count);
	SDL_free(sticks);
	return count;
}

HL_PRIM SDL_Joystick *HL_NAME(joy_open)(int idx) {
	return SDL_OpenJoystick((SDL_JoystickID)idx);
}

HL_PRIM void HL_NAME(joy_close)(SDL_Joystick *joystick) {
	SDL_CloseJoystick(joystick);
}

HL_PRIM int HL_NAME(joy_get_axis)(SDL_Joystick *joystick, int axisIdx) {
	return SDL_GetJoystickAxis(joystick, axisIdx);
}

HL_PRIM int HL_NAME(joy_get_hat)(SDL_Joystick *joystick, int hatIdx) {
	return SDL_GetJoystickHat(joystick, hatIdx);
}

HL_PRIM bool HL_NAME(joy_get_button)(SDL_Joystick *joystick, int btnIdx) {
	return SDL_GetJoystickButton(joystick, btnIdx);
}

HL_PRIM int HL_NAME(joy_get_id)(SDL_Joystick *joystick) {
	return (int)SDL_GetJoystickID(joystick);
}

HL_PRIM vbyte *HL_NAME(joy_get_name)(SDL_Joystick *joystick) {
	return (vbyte*)SDL_GetJoystickName(joystick);
}

#define TJOY _ABSTRACT(sdl_joystick)
DEFINE_PRIM(_I32, joy_count, _NO_ARG);
DEFINE_PRIM(TJOY, joy_open, _I32);
DEFINE_PRIM(_VOID, joy_close, TJOY);
DEFINE_PRIM(_I32, joy_get_axis, TJOY _I32);
DEFINE_PRIM(_I32, joy_get_hat, TJOY _I32);
DEFINE_PRIM(_BOOL, joy_get_button, TJOY _I32);
DEFINE_PRIM(_I32, joy_get_id, TJOY);
DEFINE_PRIM(_BYTES, joy_get_name, TJOY);

HL_PRIM varray *HL_NAME(get_joysticks)() {
	int count = 0;
	SDL_JoystickID *sticks = SDL_GetJoysticks(&count);
	varray *result = hl_alloc_array(&hlt_i32, count);
	SDL_JoystickID *idx = hl_aptr(result, SDL_JoystickID);
	for (int i = 0; i < count; i++)
		idx[i] = sticks[i];
	SDL_free(sticks);
	return result;
}

DEFINE_PRIM(_ARR, get_joysticks, _NO_ARG);

// Surface

HL_PRIM SDL_Surface *HL_NAME(surface_from)(vbyte *ptr, int width, int height, int depth, int pitch, int rmask, int gmask, int bmask, int amask) {
	return SDL_CreateSurfaceFrom(width, height,
		SDL_GetPixelFormatForMasks(depth, (Uint32)rmask, (Uint32)gmask, (Uint32)bmask, (Uint32)amask),
		ptr, pitch);
}

HL_PRIM void HL_NAME(free_surface)(SDL_Surface *s) {
	SDL_DestroySurface(s);
}

DEFINE_PRIM(_SURF, surface_from, _BYTES _I32 _I32 _I32 _I32 _I32 _I32 _I32 _I32);
DEFINE_PRIM(_VOID, free_surface, _SURF);

// Cursor & System

HL_PRIM void HL_NAME(show_cursor)(bool b) {
	if (b)
		SDL_ShowCursor();
	else
		SDL_HideCursor();
}

HL_PRIM bool HL_NAME(is_cursor_visible)() {
	return SDL_CursorVisible();
}

HL_PRIM SDL_Cursor *HL_NAME(cursor_create)(SDL_Surface *s, int hotX, int hotY) {
	return SDL_CreateColorCursor(s, hotX, hotY);
}

HL_PRIM SDL_Cursor *HL_NAME(cursor_create_system)(int kind) {
	return SDL_CreateSystemCursor((SDL_SystemCursor)kind);
}

HL_PRIM void HL_NAME(free_cursor)(SDL_Cursor *c) {
	SDL_DestroyCursor(c);
}

HL_PRIM void HL_NAME(set_cursor)(SDL_Cursor *c) {
	SDL_SetCursor(c);
}

HL_PRIM bool HL_NAME(set_clipboard_text)(char *text) {
	return SDL_SetClipboardText(text);
}

HL_PRIM char *HL_NAME(get_clipboard_text)() {
	char *chr = SDL_GetClipboardText();
	if (chr == NULL)
		return NULL;
	vbyte *bytes = hl_copy_bytes((vbyte*)chr, (int)strlen(chr) + 1);
	SDL_free(chr);
	return (char*)bytes;
}

HL_PRIM void HL_NAME(set_drag_and_drop_enabled)(bool enabled) {
	SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, enabled);
}

HL_PRIM bool HL_NAME(get_drag_and_drop_enabled)() {
	return SDL_EventEnabled(SDL_EVENT_DROP_FILE);
}

HL_PRIM varray *HL_NAME(get_displays)() {
	int n = 0;
	SDL_DisplayID *displays = SDL_GetDisplays(&n);
	varray *arr = hl_alloc_array(&hlt_dynobj, n);

	for (int i = 0; i < n; i++) {
		vdynamic *obj = (vdynamic*)hl_alloc_dynobj();
		SDL_Rect rect = {0};
		SDL_DisplayID display = displays[i];
		SDL_GetDisplayBounds(display, &rect);
		hl_dyn_seti(obj, hl_hash_utf8("right"), &hlt_i32, rect.x + rect.w);
		hl_dyn_seti(obj, hl_hash_utf8("bottom"), &hlt_i32, rect.y + rect.h);
		hl_dyn_seti(obj, hl_hash_utf8("left"), &hlt_i32, rect.x);
		hl_dyn_seti(obj, hl_hash_utf8("top"), &hlt_i32, rect.y);
		hl_dyn_seti(obj, hl_hash_utf8("handle"), &hlt_i32, (int)display);
		const char *name = SDL_GetDisplayName(display);
		if (name == NULL) name = "";
		hl_dyn_setp(obj, hl_hash_utf8("name"), &hlt_bytes, hl_copy_bytes((vbyte*)name, (int)strlen(name) + 1));
		hl_aptr(arr, vdynamic*)[i] = obj;
	}

	SDL_free(displays);
	return arr;
}

HL_PRIM varray *HL_NAME(get_display_modes)(int display_id) {
	int count = 0;
	SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes((SDL_DisplayID)display_id, &count);
	varray *arr = hl_alloc_array(&hlt_dynobj, count);

	for (int i = 0; i < count; i++) {
		SDL_DisplayMode *mode = modes[i];
		vdynamic *obj = (vdynamic*)hl_alloc_dynobj();
		hl_dyn_seti(obj, hl_hash_utf8("width"), &hlt_i32, mode->w);
		hl_dyn_seti(obj, hl_hash_utf8("height"), &hlt_i32, mode->h);
		hl_dyn_seti(obj, hl_hash_utf8("framerate"), &hlt_i32, (int)SDL_roundf(mode->refresh_rate));
		hl_aptr(arr, vdynamic*)[i] = obj;
	}

	SDL_free(modes);
	return arr;
}

HL_PRIM vdynobj *HL_NAME(get_current_display_mode)(int display_id, bool registry) {
	const SDL_DisplayMode *mode = registry
		? SDL_GetDesktopDisplayMode((SDL_DisplayID)display_id)
		: SDL_GetCurrentDisplayMode((SDL_DisplayID)display_id);
	if (mode == NULL) {
		printf("can't find mode for %d\n", display_id);
		return NULL;
	}
	vdynamic *obj = (vdynamic*)hl_alloc_dynobj();
	hl_dyn_seti(obj, hl_hash_utf8("width"), &hlt_i32, mode->w);
	hl_dyn_seti(obj, hl_hash_utf8("height"), &hlt_i32, mode->h);
	hl_dyn_seti(obj, hl_hash_utf8("framerate"), &hlt_i32, (int)SDL_roundf(mode->refresh_rate));
	return (vdynobj*)obj;
}

#define MAX_DEVICES 16
HL_PRIM varray *HL_NAME(get_devices)() {
	varray *a = hl_alloc_array(&hlt_bytes, MAX_DEVICES);
#ifdef _WIN32
	int i = 0;
	DISPLAY_DEVICE inf;
	inf.cb = sizeof(inf);
	while (i < MAX_DEVICES && EnumDisplayDevices(NULL, i, &inf, 0)) {
		hl_aptr(a, vbyte*)[i] = hl_copy_bytes((vbyte*)inf.DeviceString, ((int)wcslen(inf.DeviceString) + 1) * 2);
		i++;
	}
#endif
	return a;
}

HL_PRIM vbyte *HL_NAME(get_error)() {
	return (vbyte*)SDL_GetError();
}

#define _CURSOR _ABSTRACT(sdl_cursor)
DEFINE_PRIM(_VOID, show_cursor, _BOOL);
DEFINE_PRIM(_BOOL, is_cursor_visible, _NO_ARG);
DEFINE_PRIM(_CURSOR, cursor_create, _SURF _I32 _I32);
DEFINE_PRIM(_CURSOR, cursor_create_system, _I32);
DEFINE_PRIM(_VOID, free_cursor, _CURSOR);
DEFINE_PRIM(_VOID, set_cursor, _CURSOR);
DEFINE_PRIM(_BOOL, set_clipboard_text, _BYTES);
DEFINE_PRIM(_BYTES, get_clipboard_text, _NO_ARG);
DEFINE_PRIM(_VOID, set_drag_and_drop_enabled, _BOOL);
DEFINE_PRIM(_BOOL, get_drag_and_drop_enabled, _NO_ARG);
DEFINE_PRIM(_ARR, get_displays, _NO_ARG);
DEFINE_PRIM(_ARR, get_display_modes, _I32);
DEFINE_PRIM(_DYN, get_current_display_mode, _I32 _BOOL);
DEFINE_PRIM(_ARR, get_devices, _NO_ARG);
DEFINE_PRIM(_BYTES, get_error, _NO_ARG);

// SDL3 Dialogs API

typedef struct {
	vclosure *closure;
	SDL_DialogFileFilter *filters;
	int filters_size;
} dialog_data;

static dialog_data *CreateFileDialogData(vclosure *callback, varray *filters) {
	dialog_data *data = (dialog_data*)malloc(sizeof(dialog_data));
	data->closure = callback;
	hl_add_root(&data->closure);

	data->filters_size = filters ? filters->size : 0;

	if (data->filters_size > 0) {
		SDL_DialogFileFilter *sdl_filters = (SDL_DialogFileFilter*)malloc(sizeof(SDL_DialogFileFilter) * filters->size);

		for (int i = 0; i < data->filters_size; i++) {
			vdynamic *filter = hl_aptr(filters, vdynamic*)[i];
			const char *name = (const char*)hl_dyn_getp(filter, hl_hash_utf8("name"), &hlt_bytes);
			const char *pattern = (const char*)hl_dyn_getp(filter, hl_hash_utf8("pattern"), &hlt_bytes);

			sdl_filters[i].name = strdup(name);
			sdl_filters[i].pattern = strdup(pattern);
		}

		data->filters = sdl_filters;
	} else {
		data->filters = NULL;
	}

	return data;
}

static void SDLCALL FileDialogCallback(void *userdata, const char * const *filelist, int filter) {
	(void)filter;
	bool on_unregistered_thread = !hl_get_thread();
	if (on_unregistered_thread) {
		vdynamic *ctx;
		hl_register_thread(&ctx);
	}

	dialog_data *data = (dialog_data*)userdata;
	int count = 0;

	varray *array = NULL;
	if (filelist) {
		const char * const *p = filelist;
		while (*p++)
			count++;

		array = hl_alloc_array(&hlt_bytes, count);
		vbyte **array_ptr = (vbyte**)hl_aptr(array, vbyte*);

		for (int i = 0; i < count; i++) {
			size_t len = strlen(filelist[i]) + 1;
			vbyte *bytes = hl_alloc_bytes((int)len);
			memcpy(bytes, filelist[i], len);
			array_ptr[i] = bytes;
		}
	}

	hl_call1(void, data->closure, varray*, array);
	hl_remove_root(&data->closure);

	for (int i = 0; i < data->filters_size; i++) {
		free((void*)data->filters[i].name);
		free((void*)data->filters[i].pattern);
	}

	free(data->filters);
	free(data);

	if (on_unregistered_thread)
		hl_unregister_thread();
}

HL_PRIM void HL_NAME(show_open_file_dialog)(vclosure *callback, SDL_Window *window, varray *filters, vstring *default_location, bool allow_many) {
	const char *location = default_location ? hl_to_utf8(default_location->bytes) : NULL;
	dialog_data *data = CreateFileDialogData(callback, filters);
	SDL_ShowOpenFileDialog(FileDialogCallback, data, window, data->filters, data->filters_size, location, allow_many);
}

HL_PRIM void HL_NAME(show_open_folder_dialog)(vclosure *callback, SDL_Window *window, vstring *default_location, bool allow_many) {
	const char *location = default_location ? hl_to_utf8(default_location->bytes) : NULL;
	dialog_data *data = CreateFileDialogData(callback, NULL);
	SDL_ShowOpenFolderDialog(FileDialogCallback, data, window, location, allow_many);
}

HL_PRIM void HL_NAME(show_save_file_dialog)(vclosure *callback, SDL_Window *window, varray *filters, vstring *default_location) {
	const char *location = default_location ? hl_to_utf8(default_location->bytes) : NULL;
	dialog_data *data = CreateFileDialogData(callback, filters);
	SDL_ShowSaveFileDialog(FileDialogCallback, data, window, data->filters, data->filters_size, location);
}

DEFINE_PRIM(_VOID, show_open_file_dialog, _FUN(_VOID, _ARR) TWIN _ARR _STRING _BOOL);
DEFINE_PRIM(_VOID, show_open_folder_dialog, _FUN(_VOID, _ARR) TWIN _STRING _BOOL);
DEFINE_PRIM(_VOID, show_save_file_dialog, _FUN(_VOID, _ARR) TWIN _ARR _STRING);