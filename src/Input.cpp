#include "Input.h"
#include "State8080.h"

void set_port_bit(uint8_t &port, uint8_t mask, bool pressed) {
    if (pressed)
        port |= mask;
    else
        port &= (uint8_t)~mask;
}

void handle_key(State8080 &state, SDL_Keycode sym, bool pressed) {
    switch (sym) {
        case SDLK_c:  // coin
            set_port_bit(state.port1, IN1_COIN, pressed);
            break;
        case SDLK_1:  // P1 start
            set_port_bit(state.port1, IN1_P1_START, pressed);
            break;
        case SDLK_2:  // P2 start
            set_port_bit(state.port1, IN1_P2_START, pressed);
            break;
        case SDLK_SPACE:  // P1 shoot
            set_port_bit(state.port1, IN1_P1_SHOOT, pressed);
            break;
        case SDLK_LEFT:  // P1 left
            set_port_bit(state.port1, IN1_P1_LEFT, pressed);
            break;
        case SDLK_RIGHT:  // P1 right
            set_port_bit(state.port1, IN1_P1_RIGHT, pressed);
            break;
        case SDLK_w:  // P2 shoot
            set_port_bit(state.port2, IN2_P2_SHOOT, pressed);
            break;
        case SDLK_a:  // P2 left
            set_port_bit(state.port2, IN2_P2_LEFT, pressed);
            break;
        case SDLK_d:  // P2 right
            set_port_bit(state.port2, IN2_P2_RIGHT, pressed);
            break;
        case SDLK_t:  // tilt
            set_port_bit(state.port2, IN2_TILT, pressed);
            break;
        default:
            break;
    }
}

void handle_game_button(State8080 &state, int button, bool pressed) {
    switch (button) {
        case SDL_CONTROLLER_BUTTON_START:
            set_port_bit(state.port1, IN1_P1_START, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_BACK:
            set_port_bit(state.port1, IN1_COIN, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            set_port_bit(state.port1, IN1_P1_LEFT, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
            set_port_bit(state.port1, IN1_P1_RIGHT, pressed);
            break;
        case SDL_CONTROLLER_BUTTON_A:
        case SDL_CONTROLLER_BUTTON_B:
        case SDL_CONTROLLER_BUTTON_X:
            set_port_bit(state.port1, IN1_P1_SHOOT, pressed);
            break;
        default:
            break;
    }
}
