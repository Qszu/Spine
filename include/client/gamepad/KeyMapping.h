/*
	This file is part of Spine.

    Spine is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Spine is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
// Copyright 2018 Clockwork Origins

#ifndef __SPINE_GAMEPAD_KEYMAPPING_H__
#define __SPINE_GAMEPAD_KEYMAPPING_H__

#include <map>

#include <Windows.h>

namespace spine {
namespace gamepad {

	enum class DIK_KeyCodes {
		KEY_ESCAPE = 1,
		KEY_1 = 2,
		KEY_2 = 3,
		KEY_3 = 4,
		KEY_4 = 5,
		KEY_5 = 6,
		KEY_6 = 7,
		KEY_7 = 8,
		KEY_8 = 9,
		KEY_9 = 10,
		KEY_0 = 11,
		KEY_MINUS = 12,
		KEY_EQUALS = 13,
		KEY_BACK = 14,
		KEY_TAB = 15,
		KEY_Q = 16,
		KEY_W = 17,
		KEY_E = 18,
		KEY_R = 19,
		KEY_T = 20,
		KEY_Y = 21,
		KEY_U = 22,
		KEY_I = 23,
		KEY_O = 24,
		KEY_P = 25,
		KEY_LBRACKET = 26,
		KEY_RBRACKET = 27,
		KEY_RETURN = 28,
		KEY_LCONTROL = 29,
		KEY_A = 30,
		KEY_S = 31,
		KEY_D = 32,
		KEY_F = 33,
		KEY_G = 34,
		KEY_H = 35,
		KEY_J = 36,
		KEY_K = 37,
		KEY_L = 38,
		KEY_SEMICOLON = 39,
		KEY_APOSTROPHE = 40,
		KEY_GRAVE = 41,
		KEY_LSHIFT = 42,
		KEY_BACKSLASH = 43,
		KEY_Z = 44,
		KEY_X = 45,
		KEY_C = 46,
		KEY_V = 47,
		KEY_B = 48,
		KEY_N = 49,
		KEY_M = 50,
		KEY_COMMA = 51,
		KEY_PERIOD = 52,
		KEY_SLASH = 53,
		KEY_RSHIFT = 54,
		KEY_MULTIPLY = 55,
		KEY_LMENU = 56,
		KEY_SPACE = 57,
		KEY_CAPITAL = 58,
		KEY_F1 = 59,
		KEY_F2 = 60,
		KEY_F3 = 61,
		KEY_F4 = 62,
		KEY_F5 = 63,
		KEY_F6 = 64,
		KEY_F7 = 65,
		KEY_F8 = 66,
		KEY_F9 = 67,
		KEY_F10 = 68,
		KEY_NUMLOCK = 69,
		KEY_SCROLL = 70,
		KEY_NUMPAD7 = 71,
		KEY_NUMPAD8 = 72,
		KEY_NUMPAD9 = 73,
		KEY_SUBTRACT = 74,
		KEY_NUMPAD4 = 75,
		KEY_NUMPAD5 = 76,
		KEY_NUMPAD6 = 77,
		KEY_ADD = 78,
		KEY_NUMPAD1 = 79,
		KEY_NUMPAD2 = 80,
		KEY_NUMPAD3 = 81,
		KEY_NUMPAD0 = 82,
		KEY_DECIMAL = 83,
		KEY_OEM_102 = 86,
		KEY_F11 = 87,
		KEY_F12 = 88,
		KEY_F13 = 100,
		KEY_F14 = 101,
		KEY_F15 = 102,
		KEY_KANA = 112,
		KEY_ABNT_C1 = 115,
		KEY_CONVERT = 121,
		KEY_NOCONVERT = 123,
		KEY_YEN = 124,
		KEY_ABNT_C2 = 125,
		KEY_NUMPADEQUALS = 141,
		KEY_PREVTRACK = 144,
		KEY_AT = 145,
		KEY_COLON = 146,
		KEY_UNDERLINE = 147,
		KEY_KANJI = 148,
		KEY_STOP = 149,
		KEY_AX = 150,
		KEY_UNLABELED = 151,
		KEY_NEXTTRACK = 153,
		KEY_NUMPADENTER = 156,
		KEY_RCONTROL = 157,
		KEY_MUTE = 160,
		KEY_CALCULATOR = 161,
		KEY_PLAYPAUSE = 162,
		KEY_MEDIASTOP = 164,
		KEY_VOLUMEDOWN = 174,
		KEY_VOLUMEUP = 176,
		KEY_WEBHOME = 178,
		KEY_NUMPADCOMMA = 179,
		KEY_DIVIDE = 181,
		KEY_SYSRQ = 183,
		KEY_RMENU = 184,
		KEY_PAUSE = 197,
		KEY_HOME = 199,
		KEY_UPARROW = 200,
		KEY_PRIOR = 201,
		KEY_LEFTARROW = 203,
		KEY_RIGHTARROW = 205,
		KEY_END = 207,
		KEY_DOWNARROW = 208,
		KEY_NEXT = 209,
		KEY_INSERT = 210,
		KEY_DELETE = 211,
		KEY_LWIN = 219,
		KEY_RWIN = 220,
		KEY_APPS = 221,
		KEY_POWER = 222,
		KEY_SLEEP = 223,
		KEY_WAKE = 227,
		KEY_WEBSEARCH = 229,
		KEY_WEBFAVORITES = 230,
		KEY_WEBREFRESH = 231,
		KEY_WEBSTOP = 232,
		KEY_WEBFORWARD = 233,
		KEY_WEBBACK = 234,
		KEY_MYCOMPUTER = 235,
		KEY_MAIL = 236,
		KEY_MEDIASELECT = 237,

		MOUSE_BUTTONLEFT = 524, //linke Maustaste
		MOUSE_BUTTONRIGHT = 525, //rechte Maustaste
		MOUSE_BUTTONMID = 526, //mittlere Maustaste
		MOUSE_XBUTTON1 = 527, //Sonderbuttons...
		MOUSE_XBUTTON2 = 528,
		MOUSE_XBUTTON3 = 529,
		MOUSE_XBUTTON4 = 530,
		MOUSE_XBUTTON5 = 531,
	};

	enum class WindowsKeyCodes {
		KEY_ESCAPE = VK_ESCAPE,
		KEY_1 = 0x31,
		KEY_2 = 0x32,
		KEY_3 = 0x33,
		KEY_4 = 0x34,
		KEY_5 = 0x35,
		KEY_6 = 0x36,
		KEY_7 = 0x37,
		KEY_8 = 0x38,
		KEY_9 = 0x39,
		KEY_0 = 0x30,
		KEY_MINUS = VK_OEM_MINUS,
		KEY_EQUALS = 13,
		KEY_BACK = VK_BACK,
		KEY_TAB = VK_TAB,
		KEY_Q = 'Q',
		KEY_W = 'W',
		KEY_E = 'E',
		KEY_R = 'R',
		KEY_T = 'T',
		KEY_Y = 'Y',
		KEY_U = 'U',
		KEY_I = 'I',
		KEY_O = 'O',
		KEY_P = 'P',
		KEY_LBRACKET = 26,
		KEY_RBRACKET = 27,
		KEY_RETURN = VK_RETURN,
		KEY_LCONTROL = VK_LCONTROL,
		KEY_A = 'A',
		KEY_S = 'S',
		KEY_D = 'D',
		KEY_F = 'F',
		KEY_G = 'G',
		KEY_H = 'H',
		KEY_J = 'J',
		KEY_K = 'K',
		KEY_L = 'L',
		KEY_SEMICOLON = 39,
		KEY_APOSTROPHE = 40,
		KEY_GRAVE = 41,
		KEY_LSHIFT = VK_LSHIFT,
		KEY_BACKSLASH = 43,
		KEY_Z = 'Z',
		KEY_X = 'X',
		KEY_C = 'C',
		KEY_V = 'V',
		KEY_B = 'B',
		KEY_N = 'N',
		KEY_M = 'M',
		KEY_COMMA = 51,
		KEY_PERIOD = 52,
		KEY_SLASH = 53,
		KEY_RSHIFT = VK_RSHIFT,
		KEY_MULTIPLY = VK_MULTIPLY,
		KEY_LMENU = VK_LMENU,
		KEY_SPACE = VK_SPACE,
		KEY_CAPITAL = VK_CAPITAL,
		KEY_F1 = VK_F1,
		KEY_F2 = VK_F2,
		KEY_F3 = VK_F3,
		KEY_F4 = VK_F4,
		KEY_F5 = VK_F5,
		KEY_F6 = VK_F6,
		KEY_F7 = VK_F7,
		KEY_F8 = VK_F8,
		KEY_F9 = VK_F9,
		KEY_F10 = VK_F10,
		KEY_NUMLOCK = VK_NUMLOCK,
		KEY_SCROLL = VK_SCROLL,
		KEY_NUMPAD7 = VK_NUMPAD7,
		KEY_NUMPAD8 = VK_NUMPAD8,
		KEY_NUMPAD9 = VK_NUMPAD9,
		KEY_SUBTRACT = VK_SUBTRACT,
		KEY_NUMPAD4 = VK_NUMPAD4,
		KEY_NUMPAD5 = VK_NUMPAD5,
		KEY_NUMPAD6 = VK_NUMPAD6,
		KEY_ADD = VK_ADD,
		KEY_NUMPAD1 = VK_NUMPAD1,
		KEY_NUMPAD2 = VK_NUMPAD2,
		KEY_NUMPAD3 = VK_NUMPAD3,
		KEY_NUMPAD0 = VK_NUMPAD0,
		KEY_DECIMAL = VK_DECIMAL,
		KEY_OEM_102 = VK_OEM_102,
		KEY_F11 = VK_F11,
		KEY_F12 = VK_F12,
		KEY_F13 = VK_F13,
		KEY_F14 = VK_F14,
		KEY_F15 = VK_F15,
		KEY_KANA = VK_KANA,
		KEY_ABNT_C1 = 115,
		KEY_CONVERT = VK_CONVERT,
		KEY_NOCONVERT = VK_NONCONVERT,
		KEY_YEN = 124,
		KEY_ABNT_C2 = 125,
		KEY_NUMPADEQUALS = 141,
		KEY_PREVTRACK = 144,
		KEY_AT = 145,
		KEY_COLON = 146,
		KEY_UNDERLINE = 147,
		KEY_KANJI = VK_KANJI,
		KEY_STOP = 149,
		KEY_AX = 150,
		KEY_UNLABELED = 151,
		KEY_NEXTTRACK = 153,
		KEY_NUMPADENTER = 156,
		KEY_RCONTROL = VK_RCONTROL,
		KEY_MUTE = VK_VOLUME_MUTE,
		KEY_CALCULATOR = 161,
		KEY_PLAYPAUSE = 162,
		KEY_MEDIASTOP = 164,
		KEY_VOLUMEDOWN = VK_VOLUME_DOWN,
		KEY_VOLUMEUP = VK_VOLUME_UP,
		KEY_WEBHOME = 178,
		KEY_NUMPADCOMMA = 179,
		KEY_DIVIDE = VK_DIVIDE,
		KEY_SYSRQ = 183,
		KEY_RMENU = 184,
		KEY_PAUSE = 197,
		KEY_HOME = 199,
		KEY_UPARROW = VK_UP,
		KEY_PRIOR = 201,
		KEY_LEFTARROW = VK_LEFT,
		KEY_RIGHTARROW = VK_RIGHT,
		KEY_END = VK_END,
		KEY_DOWNARROW = VK_DOWN,
		KEY_NEXT = 209,
		KEY_INSERT = VK_INSERT,
		KEY_DELETE = VK_DELETE,
		KEY_LWIN = VK_LWIN,
		KEY_RWIN = VK_RWIN,
		KEY_APPS = 221,
		KEY_POWER = 222,
		KEY_SLEEP = 223,
		KEY_WAKE = 227,
		KEY_WEBSEARCH = 229,
		KEY_WEBFAVORITES = 230,
		KEY_WEBREFRESH = 231,
		KEY_WEBSTOP = 232,
		KEY_WEBFORWARD = 233,
		KEY_WEBBACK = 234,
		KEY_MYCOMPUTER = 235,
		KEY_MAIL = 236,
		KEY_MEDIASELECT = 237,

		MOUSE_BUTTONLEFT = VK_LBUTTON, //linke Maustaste
		MOUSE_BUTTONRIGHT = VK_RBUTTON, //rechte Maustaste
		MOUSE_BUTTONMID = VK_MBUTTON, //mittlere Maustaste
		MOUSE_XBUTTON1 = 527, //Sonderbuttons...
		MOUSE_XBUTTON2 = 528,
		MOUSE_XBUTTON3 = 529,
		MOUSE_XBUTTON4 = 530,
		MOUSE_XBUTTON5 = 531,
	};

	class KeyMapping {
	public:
		KeyMapping();

		void updateState(DIK_KeyCodes keyCode, bool pressed);

	private:
		std::map<DIK_KeyCodes, WindowsKeyCodes> _keyMap;
	};

} /* namespace gamepad */
} /* namespace spine */

#endif /* __SPINE_GAMEPAD_XBOXCONTROLLER_H__ */