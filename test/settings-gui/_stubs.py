# SPDX-FileCopyrightText: 2026 Nguyen Hoang Ky <nhktmdzhg@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later
"""
Headless stand-ins for the qtpy and dbus dependencies of settings-gui.

Importing this module installs them into ``sys.modules``, so the test suite
only needs the Python standard library and no Qt binding, D-Bus daemon or
display server.
"""

import sys
import types
import unittest.mock
import zlib
from pathlib import Path

# settings-gui lives at the repository root
SETTINGS_GUI_DIR = Path(__file__).resolve().parents[2] / "settings-gui"

# Real Qt::Modifier values, kept bit-distinct so flag arithmetic
# (``a | b``, ``a & b``) stays meaningful
_QT_MODIFIER_FLAGS = {
    ("Qt", "ShiftModifier"): 0x02000000,
    ("Qt", "ControlModifier"): 0x04000000,
    ("Qt", "AltModifier"): 0x08000000,
    ("Qt", "MetaModifier"): 0x10000000,
}


class _FakeMeta(type):
    """Serves arbitrary class-level attributes (e.g. ``QIcon.fromTheme``)."""

    def __getattr__(cls, name):
        if name.startswith("__"):
            raise AttributeError(name)
        return unittest.mock.MagicMock()


class _LayoutItem:
    """Minimal ``QLayoutItem`` replacement exposing ``widget()``."""

    def __init__(self, widget):
        self._widget = widget

    def widget(self):
        return self._widget


class _FakeObject(metaclass=_FakeMeta):
    """Inert base class for the fake Qt classes.

    Provides a minimal layout container protocol (``count`` / ``itemAt`` /
    ``takeAt`` / ``addWidget``) and resolves other attributes to cached
    ``MagicMock`` objects.
    """

    def __init__(self, *args, **kwargs):
        self._items = []

    def addWidget(self, widget):
        self._items.append(widget)

    def count(self):
        return len(self._items)

    def itemAt(self, index):
        return _LayoutItem(self._items[index])

    def takeAt(self, index):
        return _LayoutItem(self._items.pop(index))

    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        mock = unittest.mock.MagicMock()
        setattr(self, name, mock)
        return mock


class _QtNamespace:
    """Attribute bag whose members are stable, unique integers."""

    def __init__(self, namespace):
        self._namespace = namespace

    def __getattr__(self, name):
        if name.startswith("__"):
            raise AttributeError(name)
        known = _QT_MODIFIER_FLAGS.get((self._namespace, name))
        if known is not None:
            value = known
        else:
            value = zlib.crc32(f"{self._namespace}.{name}".encode()) & 0x0FFFFFFF
        setattr(self, name, value)
        return value


class _Signal:
    """Stand-in for ``qtpy.QtCore.Signal``; instance access returns a ``MagicMock``."""

    def __init__(self, *types_):
        self.types_ = types_

    def __set_name__(self, owner, name):
        self._attr = f"_signal_{name}"

    def __get__(self, obj, objtype=None):
        if obj is None:
            return self
        mock = obj.__dict__.get(self._attr)
        if mock is None:
            mock = unittest.mock.MagicMock()
            setattr(obj, self._attr, mock)
        return mock


def _fake_class_module(name):
    """Builds a module that fabricates subclassable fake classes on demand."""
    module = types.ModuleType(name)

    def _fabricate(attr):
        if attr.startswith("__"):
            raise AttributeError(attr)
        fake = _FakeMeta(attr, (_FakeObject,), {})
        setattr(module, attr, fake)
        return fake

    module.__getattr__ = _fabricate
    return module


def _scalar(base, name):
    """Builds a typed dbus scalar stand-in (e.g. ``dbus.Int32``)."""

    def __new__(cls, value=0):
        return base.__new__(cls, base(value))

    return type(name, (base,), {"__new__": __new__})


def build_dbus_module():
    """Builds a fresh ``dbus`` stand-in module."""
    module = types.ModuleType("dbus")

    class DBusException(Exception):
        """Stand-in for ``dbus.DBusException``."""

    class Interface:
        """Stand-in for ``dbus.Interface``."""

        def __init__(self, proxy, interface_name):
            self.proxy = proxy
            self.interface_name = interface_name

    class Dictionary(dict):
        def __init__(self, data=None, signature=None):
            super().__init__(data or {})
            self.signature = signature

    class Array(list):
        def __init__(self, data=None, signature=None):
            super().__init__(data or [])
            self.signature = signature

    class Struct(tuple):
        def __new__(cls, data=(), signature=None):
            instance = super().__new__(cls, tuple(data))
            instance.signature = signature
            return instance

    module.SessionBus = unittest.mock.MagicMock(name="dbus.SessionBus")
    module.DBusException = DBusException
    module.Interface = Interface
    module.Dictionary = Dictionary
    module.Array = Array
    module.Struct = Struct
    module.Byte = _scalar(int, "Byte")
    module.Boolean = _scalar(int, "Boolean")
    module.Int16 = _scalar(int, "Int16")
    module.Int32 = _scalar(int, "Int32")
    module.Int64 = _scalar(int, "Int64")
    module.UInt16 = _scalar(int, "UInt16")
    module.UInt32 = _scalar(int, "UInt32")
    module.UInt64 = _scalar(int, "UInt64")
    module.Double = _scalar(float, "Double")
    module.String = _scalar(str, "String")
    return module


def build_qtpy_module():
    """Builds a fresh ``qtpy`` package stand-in and registers its submodules."""
    qtpy = types.ModuleType("qtpy")
    qtpy.__path__ = []

    core = _fake_class_module("qtpy.QtCore")
    core.Qt = _QtNamespace("Qt")
    core.QEvent = _QtNamespace("QEvent")
    core.Signal = _Signal
    gui = _fake_class_module("qtpy.QtGui")
    widgets = _fake_class_module("qtpy.QtWidgets")

    qtpy.QtCore = core
    qtpy.QtGui = gui
    qtpy.QtWidgets = widgets

    sys.modules["qtpy"] = qtpy
    sys.modules["qtpy.QtCore"] = core
    sys.modules["qtpy.QtGui"] = gui
    sys.modules["qtpy.QtWidgets"] = widgets
    return qtpy


def install():
    """Installs the stand-ins and puts settings-gui on ``sys.path`` (idempotent)."""
    global _installed
    if not _installed:
        sys.modules["dbus"] = build_dbus_module()
        build_qtpy_module()
        _installed = True
    if str(SETTINGS_GUI_DIR) not in sys.path:
        sys.path.insert(0, str(SETTINGS_GUI_DIR))


def dbus_stub():
    """Returns the installed ``dbus`` stand-in module."""
    install()
    return sys.modules["dbus"]


_installed = False

install()
