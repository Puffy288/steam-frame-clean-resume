#!/usr/bin/python3
"""Close Steam's automatic post-suspend dashboard once; never inhibit sleep."""
import os
from pathlib import Path
import subprocess

import dbus
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import Gio, GLib

LOG = Path.home() / ".local/share/Steam/logs/vrwebhelper_systemui.txt"
COMMAND = Path("/opt/steamvr/bin/linuxarm64/vrcmd")
RESUME_DELAY_MS = 1


class ResumeDashboard:
    def __init__(self):
        self.file = None
        self.monitor = None
        self.pending = 0
        self.active = False
        self.partial = b""

    def stop(self, reason=None):
        self.active = False
        for name in ("pending",):
            source = getattr(self, name)
            if source:
                GLib.source_remove(source)
                setattr(self, name, 0)
        if self.monitor:
            self.monitor.cancel()
            self.monitor = None
        if self.file:
            self.file.close()
            self.file = None
        self.partial = b""
        if reason:
            print(reason, flush=True)

    def sleep_changed(self, sleeping):
        if sleeping:
            self.stop()
            try:
                self.file = LOG.open("rb")
                self.file.seek(0, os.SEEK_END)
            except OSError as error:
                self.stop(f"Skipping this cycle: cannot read SteamVR log ({error}).")
            return
        # A service start, duplicate signal or screen-off event must not close UI.
        if self.file is None or self.active:
            return
        try:
            self.monitor = Gio.File.new_for_path(str(LOG)).monitor_file(
                Gio.FileMonitorFlags.NONE, None)
            self.monitor.set_rate_limit(0)
            self.monitor.connect("changed", self.changed)
            self.active = True
            self.pending = GLib.timeout_add(RESUME_DELAY_MS, self.close_once)
            print("Resume detected; closing the dashboard immediately.", flush=True)
            self.drain()
        except (OSError, GLib.Error) as error:
            self.stop(f"Skipping this cycle: {error}")

    def changed(self, *_):
        self.drain()

    def drain(self):
        if not self.active:
            return
        try:
            disk = LOG.stat()
            opened = os.fstat(self.file.fileno())
            if (disk.st_dev, disk.st_ino) != (opened.st_dev, opened.st_ino):
                self.stop("Skipped: SteamVR log rotated.")
                return
            if disk.st_size < self.file.tell():
                self.stop("Skipped: SteamVR log was reset.")
                return
            data = self.partial + self.file.read(131073)
            if len(data) > 131072:
                self.stop("Skipped: unusually large log update.")
                return
            lines = data.split(b"\n")
            self.partial = lines.pop()
            for raw in lines:
                line = raw.decode("utf-8", "replace")
                if "[Dashboard] [ToggleDashboard]" in line or "[Dashboard] [HideDashboard]" in line:
                    self.stop("Skipped: dashboard was already used or closed.")
                    return
        except OSError as error:
            self.stop(f"Skipped: log read failed ({error}).")

    def close_once(self):
        self.pending = 0
        # Check for a last-moment Steam-button action before sending the request.
        self.drain()
        if not self.active:
            return GLib.SOURCE_REMOVE
        self.stop()
        try:
            result = subprocess.run(
                [str(COMMAND), "--background", "--hidedashboard"],
                capture_output=True, text=True, timeout=3, check=False)
            if result.returncode == 0:
                print("Sent one dashboard-close request. Idle until the next suspend/resume.", flush=True)
            else:
                print(f"Close request failed (exit {result.returncode}); no retry.", flush=True)
        except (OSError, subprocess.TimeoutExpired) as error:
            print(f"Close request failed ({error}); no retry.", flush=True)
        return GLib.SOURCE_REMOVE

def main():
    DBusGMainLoop(set_as_default=True)
    helper = ResumeDashboard()
    bus = dbus.SystemBus()
    bus.add_signal_receiver(
        helper.sleep_changed,
        signal_name="PrepareForSleep",
        dbus_interface="org.freedesktop.login1.Manager",
        bus_name="org.freedesktop.login1",
        path="/org/freedesktop/login1")
    print("Ready. Waiting for suspend/resume; no polling or sleep inhibitor.", flush=True)
    GLib.MainLoop().run()


if __name__ == "__main__":
    main()
