#!/usr/bin/env python3
"""The hwmon receiver on a terminal (what `ssh -tt` gives the remote command, or a UART console).

Usage: hwmon_pty_test.py <path to the compiled hwmoncore_test>
Runs the real bootstrap command on a pseudo terminal, waits for "H", sends the script (in one write and in
small pieces), expects the versions and samples, then does what "Apply" does: STOP and the script again.
A tty must not lose a script that has already arrived (bash's `read -N` did, which made the first
connection do nothing).
"""
import os
import pty
import select
import subprocess
import sys
import tempfile
import time


def read_for(fd, seconds):
    out = b""
    end = time.time() + seconds
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                out += os.read(fd, 65536)
            except OSError:
                break
    return out


def count(out, prefix):
    return len([l for l in out.replace(b"\r", b"").split(b"\n") if l.startswith(prefix)])


def main():
    tool = sys.argv[1]
    d = tempfile.mkdtemp()
    subprocess.run([tool, "--dump", d], check=True)
    boot = open(os.path.join(d, "boot")).read()
    frame = open(os.path.join(d, "frame"), "rb").read()
    ok = True
    for mode in ("one write", "small pieces"):
        pid, fd = pty.fork()
        if pid == 0:
            os.execvp("sh", ["sh", "-c", boot])
        hello = read_for(fd, 1.0)

        def send():
            if mode == "one write":
                os.write(fd, frame)
            else:
                for i in range(0, len(frame), 40):
                    os.write(fd, frame[i:i + 40])
                    time.sleep(0.005)

        send()
        first = read_for(fd, 1.3)
        os.write(fd, b"STOP\n")          # what "Apply" does
        time.sleep(0.3)
        send()
        second = read_for(fd, 1.3)
        os.write(fd, b"STOP\n")
        read_for(fd, 0.3)
        os.kill(pid, 9)
        os.waitpid(pid, 0)
        good = (hello.startswith(b"H") and count(first, b"I\t") == 1 and count(first, b"S\t") >= 8
                and count(second, b"I\t") == 1 and count(second, b"S\t") >= 8 and b"not found" not in first + second)
        print("%s: hello=%s first(I=%d S=%d) second(I=%d S=%d) -> %s" % (
            mode, hello.startswith(b"H"), count(first, b"I\t"), count(first, b"S\t"),
            count(second, b"I\t"), count(second, b"S\t"), "ok" if good else "FAIL"))
        ok = ok and good
    print("ALL OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
