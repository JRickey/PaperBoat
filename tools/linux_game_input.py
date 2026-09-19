#!/usr/bin/env python3
"""Send a reproducible keyboard-input timeline to a game window under X11."""

from __future__ import annotations

import argparse
import ctypes
import ctypes.util
import sys
import time


Window = ctypes.c_ulong


class X11Input:
    def __init__(self, display_name: str | None):
        x11_name = ctypes.util.find_library("X11")
        xtst_name = ctypes.util.find_library("Xtst")
        if x11_name is None or xtst_name is None:
            raise RuntimeError("libX11 and libXtst are required")

        self.x11 = ctypes.CDLL(x11_name)
        self.xtst = ctypes.CDLL(xtst_name)
        self.x11.XOpenDisplay.argtypes = [ctypes.c_char_p]
        self.x11.XOpenDisplay.restype = ctypes.c_void_p
        self.x11.XDefaultRootWindow.argtypes = [ctypes.c_void_p]
        self.x11.XDefaultRootWindow.restype = Window
        self.x11.XQueryTree.argtypes = [
            ctypes.c_void_p,
            Window,
            ctypes.POINTER(Window),
            ctypes.POINTER(Window),
            ctypes.POINTER(ctypes.POINTER(Window)),
            ctypes.POINTER(ctypes.c_uint),
        ]
        self.x11.XQueryTree.restype = ctypes.c_int
        self.x11.XFetchName.argtypes = [ctypes.c_void_p, Window, ctypes.POINTER(ctypes.c_char_p)]
        self.x11.XFetchName.restype = ctypes.c_int
        self.x11.XFree.argtypes = [ctypes.c_void_p]
        self.x11.XStringToKeysym.argtypes = [ctypes.c_char_p]
        self.x11.XStringToKeysym.restype = ctypes.c_ulong
        self.x11.XKeysymToKeycode.argtypes = [ctypes.c_void_p, ctypes.c_ulong]
        self.x11.XKeysymToKeycode.restype = ctypes.c_ubyte
        self.x11.XSetInputFocus.argtypes = [ctypes.c_void_p, Window, ctypes.c_int, ctypes.c_ulong]
        self.x11.XRaiseWindow.argtypes = [ctypes.c_void_p, Window]
        self.x11.XFlush.argtypes = [ctypes.c_void_p]
        self.xtst.XTestFakeKeyEvent.argtypes = [ctypes.c_void_p, ctypes.c_uint, ctypes.c_int, ctypes.c_ulong]
        self.xtst.XTestFakeKeyEvent.restype = ctypes.c_int

        encoded_display = display_name.encode() if display_name else None
        self.display = self.x11.XOpenDisplay(encoded_display)
        if not self.display:
            raise RuntimeError(f"could not open X display {display_name or '<environment>'}")

    def _name(self, window: int) -> str:
        value = ctypes.c_char_p()
        if not self.x11.XFetchName(self.display, Window(window), ctypes.byref(value)) or not value.value:
            return ""
        try:
            return value.value.decode(errors="replace")
        finally:
            self.x11.XFree(value)

    def _children(self, window: int) -> list[int]:
        root = Window()
        parent = Window()
        children = ctypes.POINTER(Window)()
        count = ctypes.c_uint()
        if not self.x11.XQueryTree(
            self.display,
            Window(window),
            ctypes.byref(root),
            ctypes.byref(parent),
            ctypes.byref(children),
            ctypes.byref(count),
        ):
            return []
        try:
            return [children[i] for i in range(count.value)]
        finally:
            if children:
                self.x11.XFree(children)

    def find_window(self, title_fragment: str, timeout: float) -> int:
        deadline = time.monotonic() + timeout
        root = self.x11.XDefaultRootWindow(self.display)
        while time.monotonic() < deadline:
            pending = [root]
            while pending:
                window = pending.pop()
                if title_fragment.casefold() in self._name(window).casefold():
                    return window
                pending.extend(self._children(window))
            time.sleep(0.1)
        raise RuntimeError(f"window containing {title_fragment!r} did not appear within {timeout:g}s")

    def focus(self, window: int) -> None:
        self.x11.XRaiseWindow(self.display, Window(window))
        self.x11.XSetInputFocus(self.display, Window(window), 1, 0)
        self.x11.XFlush(self.display)

    def press(self, key: str, hold: float) -> None:
        keysym = self.x11.XStringToKeysym(key.encode())
        if not keysym:
            raise RuntimeError(f"unknown X11 key name: {key}")
        keycode = self.x11.XKeysymToKeycode(self.display, keysym)
        if not keycode:
            raise RuntimeError(f"no keycode for X11 key name: {key}")
        self.xtst.XTestFakeKeyEvent(self.display, keycode, True, 0)
        self.x11.XFlush(self.display)
        time.sleep(hold)
        self.xtst.XTestFakeKeyEvent(self.display, keycode, False, 0)
        self.x11.XFlush(self.display)


def parse_events(value: str) -> list[tuple[float, str]]:
    events = []
    for event in value.split(","):
        delay, separator, key = event.partition(":")
        if not separator or not key:
            raise ValueError(f"invalid event {event!r}; expected DELAY:KEY")
        events.append((float(delay), key))
    return events


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--display", help="X display, for example :99")
    parser.add_argument("--window", default="Paper", help="case-insensitive title fragment")
    parser.add_argument("--wait", type=float, default=30.0, help="seconds to wait for the window")
    parser.add_argument("--hold", type=float, default=0.10, help="key-down duration in seconds")
    parser.add_argument(
        "events",
        help="comma-separated DELAY:KEY events; each delay is relative to the prior event",
    )
    args = parser.parse_args()

    try:
        events = parse_events(args.events)
        xinput = X11Input(args.display)
        window = xinput.find_window(args.window, args.wait)
        xinput.focus(window)
        for delay, key in events:
            time.sleep(delay)
            xinput.press(key, args.hold)
            print(f"pressed {key} after {delay:g}s", flush=True)
    except (RuntimeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
