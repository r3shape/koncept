#include <koncept/core/input/koncept_input.h>

static struct internal_input_state {
    struct devices {
        u8 keyboard[256];
        i16 mouse_delta[2];
        u8 mouse_buttons[KC_MOUSE_MAX_BUTTONS];
    } devices[2];
    _koncept_events_api* events_api;
} internal_input_state = {0};


void _update_impl(void) {
    memcpy(&internal_input_state.devices[0], &internal_input_state.devices[1], sizeof(internal_input_state.devices[1]));
    
    // reset current rawinput mouse deltas
    internal_input_state.devices[1].mouse_delta[0] = 0.0;
    internal_input_state.devices[1].mouse_delta[1] = 0.0;
}

void _reset_impl(void) {
    COREX_FORJ(0, 2, 1) {
        internal_input_state.devices[j].mouse_delta[0] = 0;
        internal_input_state.devices[j].mouse_delta[1] = 0;
    }
    
    COREX_FORJ(0, 2, 1)
        COREX_FORI(0, 256, 1)
            internal_input_state.devices[j].keyboard[i] = 0;
    COREX_FORJ(0, 2, 1)
        COREX_FORI(0, KC_MOUSE_MAX_BUTTONS, 1)
            internal_input_state.devices[j].mouse_buttons[i] = 0;
}

u8 _key_is_up_impl(KC_Keyboard_Key key) {
    return internal_input_state.devices[1].keyboard[key] == COREX_FALSE;
}

u8 _key_was_up_impl(KC_Keyboard_Key key) {
    return internal_input_state.devices[0].keyboard[key] == COREX_FALSE;
}

u8 _key_is_down_impl(KC_Keyboard_Key key) {
    return internal_input_state.devices[1].keyboard[key] == COREX_TRUE;
}

u8 _key_was_down_impl(KC_Keyboard_Key key) {
    return internal_input_state.devices[0].keyboard[key] == COREX_TRUE;
}

u8 _button_is_up_impl(KC_Mouse_Button button) {
    return internal_input_state.devices[1].mouse_buttons[button] == COREX_FALSE;
}

u8 _button_was_up_impl(KC_Mouse_Button button) {
    return internal_input_state.devices[0].mouse_buttons[button] == COREX_FALSE;
}

u8 _button_is_down_impl(KC_Mouse_Button button) {
    return internal_input_state.devices[1].mouse_buttons[button] == COREX_TRUE;
}

u8 _button_was_down_impl(KC_Mouse_Button button) {
    return internal_input_state.devices[0].mouse_buttons[button] == COREX_TRUE;
}

void _mouse_get_position_impl(i16* x, i16* y) {
    *x = internal_input_state.devices[1].mouse_delta[0]; *y = internal_input_state.devices[1].mouse_delta[1];
}

void _mouse_get_last_position_impl(i16* x, i16* y) {
    *x = internal_input_state.devices[0].mouse_delta[0]; *y = internal_input_state.devices[0].mouse_delta[1];
}

void _process_mouse_wheel_input_impl(i8 z_delta) {
    if (!internal_input_state.events_api) return;

    KC_Event event;
    event.i8[0] = z_delta;
    internal_input_state.events_api->push_event(KC_EVENT_MOUSE_WHEEL, event);
}

void _process_mouse_move_input_impl(i16 x, i16 y) {
    if (!internal_input_state.events_api) return;

    if (x != 0 || y != 0) {
        internal_input_state.devices[0].mouse_delta[0] = internal_input_state.devices[1].mouse_delta[0];
        internal_input_state.devices[0].mouse_delta[1] = internal_input_state.devices[1].mouse_delta[1];

        internal_input_state.devices[1].mouse_delta[0] += x; // WM_INPUT returns mouse deltas so just accumulate them
        internal_input_state.devices[1].mouse_delta[1] += y;

        KC_Event event;
        event.u16[0] = internal_input_state.devices[1].mouse_delta[0];
        event.u16[1] = internal_input_state.devices[1].mouse_delta[1];
        internal_input_state.events_api->push_event(KC_EVENT_MOUSE_MOVE, event);
    }
}

void _process_key_input_impl(KC_Keyboard_Key key, u8 pressed) {
    if (!internal_input_state.events_api) return;

    if (internal_input_state.devices[1].keyboard[key] != pressed) {
        internal_input_state.devices[1].keyboard[key] = pressed;

        KC_Event event;
        event.u16[0] = key;
        internal_input_state.events_api->push_event(pressed ? KC_EVENT_KEY_PRESSED : KC_EVENT_KEY_RELEASED, event);
    }
}

void _process_mouse_button_input_impl(KC_Mouse_Button button, u8 pressed) {
    if (!internal_input_state.events_api) return;

    if (internal_input_state.devices[1].mouse_buttons[button] != pressed) {
        internal_input_state.devices[1].mouse_buttons[button] = pressed;

        KC_Event event;
        event.u16[0] = button;
        internal_input_state.events_api->push_event(pressed ? KC_EVENT_BUTTON_PRESSED : KC_EVENT_BUTTON_RELEASED, event);
    }
}

u8 _koncept_init_input(_koncept_events_api* events_api, _koncept_input_api* api) {
    if (!events_api || !api) return COREX_FALSE;
    
    internal_input_state.events_api = events_api;

    api->reset = _reset_impl;
    api->update = _update_impl;
    api->key_is_up = _key_is_up_impl;
    api->key_was_up = _key_was_up_impl;
    api->key_is_down = _key_is_down_impl;
    api->key_was_down = _key_was_down_impl;
    api->button_is_up = _button_is_up_impl;
    api->button_was_up = _button_was_up_impl;
    api->button_is_down = _button_is_down_impl;
    api->button_was_down = _button_was_down_impl;
    api->mouse_get_position = _mouse_get_position_impl;
    api->mouse_get_last_position = _mouse_get_last_position_impl;

    api->_process_mouse_wheel_input = _process_mouse_wheel_input_impl;
    api->_process_mouse_move_input = _process_mouse_move_input_impl;
    api->_process_key_input = _process_key_input_impl;
    api->_process_mouse_button_input = _process_mouse_button_input_impl;

    return COREX_TRUE;
}

u8 _koncept_cleanup_input(_koncept_input_api* api) {
    if (!api) return COREX_FALSE;
    
    internal_input_state.events_api = NULL;

    api->update = NULL;
    api->key_is_up = NULL;
    api->key_was_up = NULL;
    api->key_is_down = NULL;
    api->key_was_down = NULL;
    api->button_is_up = NULL;
    api->button_was_up = NULL;
    api->button_is_down = NULL;
    api->button_was_down = NULL;
    api->mouse_get_position = NULL;
    api->mouse_get_last_position = NULL;

    api->_process_mouse_wheel_input = NULL;
    api->_process_mouse_move_input = NULL;
    api->_process_key_input = NULL;
    api->_process_mouse_button_input = NULL;

    return COREX_TRUE;
}

