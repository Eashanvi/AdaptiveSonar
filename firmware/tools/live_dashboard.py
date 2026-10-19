#!/usr/bin/env python3
"""
live_dashboard.py - read the board's telemetry over USB-serial and plot it
live.  This is the successor to phase1_dashboard.py: same idea, but instead
of loading a .mat file that MATLAB wrote earlier, it reads what the hardware
is deciding right now, while someone turns the pots.

    pip install pyserial matplotlib
    python3 Tools/live_dashboard.py /dev/ttyUSB0
    python3 Tools/live_dashboard.py COM5            (Windows)

Telemetry format, one line per ping, emitted by sonar_app.c:

  PING,idx,depth,turb,temp,sal,depth_state,turb_state,c,alpha,
       waveform,window,mode,f0,f1,pulse_ms,amp,range_res,gain_db,
       pri_ms,duty_pct,power_mw,cycles,samples
"""
import sys, collections, threading, queue
import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

FIELDS = ["idx", "depth", "turb", "temp", "sal", "depth_state", "turb_state",
          "c", "alpha", "waveform", "window", "mode", "f0", "f1", "pulse_ms",
          "amp", "range_res", "gain_db", "pri_ms", "duty_pct", "power_mw",
          "cycles", "samples"]
NUM = {"idx", "depth", "turb", "temp", "sal", "c", "alpha", "f0", "f1",
       "pulse_ms", "amp", "range_res", "gain_db", "pri_ms", "duty_pct",
       "power_mw", "cycles", "samples"}

N = 200
hist = {k: collections.deque(maxlen=N) for k in NUM}
latest = {}
q = queue.Queue()

INK, ACC, ACC2 = "#0f2b3d", "#1b7f9e", "#c2521f"


def reader(port):
    import serial
    s = serial.Serial(port, 921600, timeout=1)
    while True:
        try:
            line = s.readline().decode("ascii", "ignore").strip()
        except Exception:
            continue
        if line.startswith("PING,"):
            q.put(line)
        elif line:
            print(line)


def drain():
    while not q.empty():
        parts = q.get().split(",")[1:]
        if len(parts) < len(FIELDS):
            continue
        row = dict(zip(FIELDS, parts))
        for k in NUM:
            try:
                row[k] = float(row[k])
                hist[k].append(row[k])
            except ValueError:
                pass
        latest.update(row)


def main(port):
    threading.Thread(target=reader, args=(port,), daemon=True).start()

    fig, ax = plt.subplots(2, 3, figsize=(14, 7))
    fig.suptitle("Adaptive Sonar Transmitter - live from the board",
                 fontsize=13, weight="bold", color=INK)
    for a in ax.ravel():
        a.grid(alpha=.3)

    def update(_):
        drain()
        if not hist["idx"]:
            return
        x = list(hist["idx"])

        specs = [
            (ax[0][0], [("depth", ACC), ("turb", ACC2)],
             "environment (m / %)"),
            (ax[0][1], [("f0", ACC), ("f1", ACC2)], "commanded band (Hz)"),
            (ax[0][2], [("pulse_ms", ACC)], "pulse duration (ms)"),
            (ax[1][0], [("amp", ACC)], "amplitude"),
            (ax[1][1], [("range_res", ACC)], "range resolution (m)"),
            (ax[1][2], [("power_mw", ACC2)], "average power (mW)"),
        ]
        for a, series, title in specs:
            a.clear(); a.grid(alpha=.3)
            for key, col in series:
                if hist[key]:
                    a.plot(x[-len(hist[key]):], list(hist[key]),
                           color=col, lw=1.4, label=key)
            a.set_title(title, loc="left", fontsize=10, weight="bold",
                        color=INK)
            if len(series) > 1:
                a.legend(fontsize=7, frameon=False)

        if latest:
            fig.suptitle(
                "Adaptive Sonar Transmitter - live   |   %s / %s   |   "
                "%s + %s   |   mode %s   |   c=%s m/s  alpha=%s dB/km"
                % (latest.get("depth_state", "?"), latest.get("turb_state", "?"),
                   latest.get("waveform", "?"), latest.get("window", "?"),
                   latest.get("mode", "?"), latest.get("c", "?"),
                   latest.get("alpha", "?")),
                fontsize=11, weight="bold", color=INK)

    ani = FuncAnimation(fig, update, interval=300, cache_frame_data=False)
    plt.tight_layout(rect=[0, 0, 1, 0.95])
    plt.show()
    _ = ani


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    main(sys.argv[1])
