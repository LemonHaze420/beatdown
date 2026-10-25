#include "app/internal.h"

namespace app_detail
{

uint32_t keyButton(int key)
{
    switch(key)
    {
    case sk::KEY_UP:    return XINPUT_GAMEPAD_DPAD_UP;
    case sk::KEY_DOWN:  return XINPUT_GAMEPAD_DPAD_DOWN;
    case sk::KEY_LEFT:  return XINPUT_GAMEPAD_DPAD_LEFT;
    case sk::KEY_RIGHT: return XINPUT_GAMEPAD_DPAD_RIGHT;
    case sk::KEY_ENTER: return HOST_INPUT_KEY_CONFIRM;
    case sk::KEY_ESC:   return HOST_INPUT_KEY_CANCEL;
    case 'P':           return HOST_INPUT_KEY_PAUSE;
    case 'Q':           return HOST_INPUT_KEY_QUIT;
    case 'X':           return HOST_INPUT_KEY_ATTACK;
    case 'C':           return HOST_INPUT_KEY_HEAVY;
    case 'V':           return HOST_INPUT_KEY_GRAB;
    case 'Z':           return HOST_INPUT_KEY_GUARD;
    case ' ':           return HOST_INPUT_KEY_DODGE;
    case 'F':           return HOST_INPUT_KEY_INTERACT;
    case 'E':           return HOST_INPUT_KEY_USE_FIRST;
    case 'R':           return HOST_INPUT_KEY_USE_SECOND;
    }
    return 0;
}

void keyDown(int key)
{
    if(key == sk::KEY_F5)
    {
        g_game.save(g_savePath);
        return;
    }
    if(key == sk::KEY_F9)
    {
        g_game.load(g_savePath);
        return;
    }
    g_keyboardButtons |= keyButton(key);
}

void keyUp(int key)
{
    g_keyboardButtons &= ~keyButton(key);
}

uint32_t mapHostInput(uint32_t buttons)
{
    uint32_t input = 0;
    if((buttons & (XINPUT_GAMEPAD_DPAD_UP |
                   HOST_INPUT_STICK_UP)) != 0)
        input |= GAME_INPUT_UP;
    if((buttons & (XINPUT_GAMEPAD_DPAD_DOWN |
                   HOST_INPUT_STICK_DOWN)) != 0)
        input |= GAME_INPUT_DOWN;
    if((buttons & (XINPUT_GAMEPAD_DPAD_LEFT |
                   HOST_INPUT_STICK_LEFT)) != 0)
        input |= GAME_INPUT_LEFT;
    if((buttons & (XINPUT_GAMEPAD_DPAD_RIGHT |
                   HOST_INPUT_STICK_RIGHT)) != 0)
        input |= GAME_INPUT_RIGHT;

    const GameScreen screen = g_game.screen();
    if((buttons & XINPUT_GAMEPAD_START) != 0)
        input |= screen == GAME_SCREEN_GAMEPLAY ?
            GAME_INPUT_PAUSE : GAME_INPUT_START;
    if((buttons & XINPUT_GAMEPAD_A) != 0)
        input |= screen == GAME_SCREEN_GAMEPLAY ? GAME_INPUT_GUARD :
                 screen == GAME_SCREEN_TITLE ?
                 GAME_INPUT_START : GAME_INPUT_CONFIRM;
    if((buttons & XINPUT_GAMEPAD_B) != 0)
        input |= screen == GAME_SCREEN_GAMEPLAY ?
            GAME_INPUT_GRAB : GAME_INPUT_CANCEL;
    if((buttons & XINPUT_GAMEPAD_BACK) != 0)
        input |= screen == GAME_SCREEN_GAMEPLAY ?
            GAME_INPUT_INTERACT : GAME_INPUT_CANCEL;
    if((buttons & XINPUT_GAMEPAD_X) != 0)
        input |= GAME_INPUT_PUNCH;
    if((buttons & XINPUT_GAMEPAD_Y) != 0)
        input |= GAME_INPUT_KICK;
    if((buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0)
        input |= GAME_INPUT_GUARD;
    if((buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0)
        input |= GAME_INPUT_DODGE;
    if((buttons & XINPUT_GAMEPAD_LEFT_THUMB) != 0)
        input |= screen == GAME_SCREEN_GAMEPLAY ?
            GAME_INPUT_USE_FIRST : GAME_INPUT_DODGE;
    if((buttons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0)
        input |= screen == GAME_SCREEN_GAMEPLAY ?
            GAME_INPUT_USE_SECOND : GAME_INPUT_CONFIRM;

    if((buttons & HOST_INPUT_KEY_CONFIRM) != 0)
        input |= screen == GAME_SCREEN_TITLE ?
            GAME_INPUT_START : GAME_INPUT_CONFIRM;
    if((buttons & HOST_INPUT_KEY_CANCEL) != 0)
        input |= screen == GAME_SCREEN_GAMEPLAY ?
            GAME_INPUT_PAUSE : GAME_INPUT_CANCEL;
    if((buttons & HOST_INPUT_KEY_PAUSE) != 0)
        input |= GAME_INPUT_PAUSE;
    if((buttons & HOST_INPUT_KEY_QUIT) != 0)
        input |= GAME_INPUT_QUIT;
    if((buttons & HOST_INPUT_KEY_ATTACK) != 0)
        input |= GAME_INPUT_ATTACK;
    if((buttons & HOST_INPUT_KEY_HEAVY) != 0)
        input |= GAME_INPUT_HEAVY;
    if((buttons & HOST_INPUT_KEY_GRAB) != 0)
        input |= GAME_INPUT_GRAB;
    if((buttons & HOST_INPUT_KEY_GUARD) != 0)
        input |= GAME_INPUT_GUARD;
    if((buttons & HOST_INPUT_KEY_DODGE) != 0)
        input |= GAME_INPUT_DODGE;
    if((buttons & HOST_INPUT_KEY_INTERACT) != 0)
        input |= GAME_INPUT_INTERACT;
    if((buttons & HOST_INPUT_KEY_USE_FIRST) != 0)
        input |= GAME_INPUT_USE_FIRST;
    if((buttons & HOST_INPUT_KEY_USE_SECOND) != 0)
        input |= GAME_INPUT_USE_SECOND;
    return input;
}

float normalizedThumb(short value)
{
    return value < 0 ?
        static_cast<float>(value) / 32768.0f :
        static_cast<float>(value) / 32767.0f;
}

void normalizeStick(short rawX, short rawY, float deadZone,
                    float* outputX, float* outputY)
{
    float x = normalizedThumb(rawX);
    float y = normalizedThumb(rawY);
    const float magnitude = sqrtf(x * x + y * y);
    if(magnitude <= deadZone)
    {
        *outputX = 0.0f;
        *outputY = 0.0f;
        return;
    }
    const float clampedMagnitude = magnitude > 1.0f ? 1.0f : magnitude;
    const float scaledMagnitude =
        (clampedMagnitude - deadZone) / (1.0f - deadZone);
    const float scale = scaledMagnitude / magnitude;
    *outputX = x * scale;
    *outputY = y * scale;
}

ControllerFrame pollController(int playerIndex)
{
    ControllerFrame frame = {};
    XINPUT_STATE state;
    memset(&state, 0, sizeof(state));
    if(XInputGetState(static_cast<DWORD>(playerIndex), &state) !=
       ERROR_SUCCESS)
        return frame;

    frame.connected = true;
    frame.buttons = state.Gamepad.wButtons;
    frame.leftX = normalizedThumb(state.Gamepad.sThumbLX);
    frame.leftY = normalizedThumb(state.Gamepad.sThumbLY);
    normalizeStick(
        state.Gamepad.sThumbRX, state.Gamepad.sThumbRY,
        static_cast<float>(XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) / 32767.0f,
        &frame.rightX, &frame.rightY);

    const float menuThreshold = 0.75f;
    if(frame.leftX < -menuThreshold)
        frame.buttons |= HOST_INPUT_STICK_LEFT;
    if(frame.leftX > menuThreshold)
        frame.buttons |= HOST_INPUT_STICK_RIGHT;
    if(frame.leftY < -menuThreshold)
        frame.buttons |= HOST_INPUT_STICK_DOWN;
    if(frame.leftY > menuThreshold)
        frame.buttons |= HOST_INPUT_STICK_UP;
    return frame;
}

uint32_t pressedInput(const GameInputState& state)
{
    const uint32_t navigation =
        XINPUT_GAMEPAD_DPAD_UP | XINPUT_GAMEPAD_DPAD_DOWN |
        XINPUT_GAMEPAD_DPAD_LEFT | XINPUT_GAMEPAD_DPAD_RIGHT |
        HOST_INPUT_STICK_UP | HOST_INPUT_STICK_DOWN |
        HOST_INPUT_STICK_LEFT | HOST_INPUT_STICK_RIGHT;
    return mapHostInput(state.pressedButtons |
                        (state.repeatedButtons & navigation));
}

void updateControllerRumble(void)
{
    for(int player = 0; player < 2; ++player)
    {
        XINPUT_VIBRATION vibration;
        memset(&vibration, 0, sizeof(vibration));
        const GameRumbleState& state = g_game.rumbleState(player);
        const bool sceneRumble = player == 0 && g_sceneRumbleActive &&
            g_game.runtimeState().options.vibration != 0;
        if(sceneRumble)
        {
            vibration.wLeftMotorSpeed = static_cast<WORD>(
                g_sceneRumbleLow * 65535.0f + 0.5f);
            vibration.wRightMotorSpeed = static_cast<WORD>(
                g_sceneRumbleHigh * 65535.0f + 0.5f);
        }
        else if(state.active)
        {
            vibration.wLeftMotorSpeed = static_cast<WORD>(
                state.lowMotor * 65535.0f + 0.5f);
            vibration.wRightMotorSpeed = static_cast<WORD>(
                state.highMotor * 65535.0f + 0.5f);
        }
        XInputSetState(static_cast<DWORD>(player), &vibration);
    }
}

}
