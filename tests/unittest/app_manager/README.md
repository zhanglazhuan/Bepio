# Application manager and Settings tests

Run from the repository root with Python and a host C compiler:

```powershell
python tests/unittest/app_manager/run.py
# Set $env:CC to a compiler executable path if gcc is not on PATH.
```

Tests compile the actual manager and Settings MVC sources. They cover invalid and
duplicate registration, capacity, lookup, single-app ownership, repeated open,
back interception, close notification, UI visibility, failed-start cleanup,
callback reentry, reopening from a close callback, Settings navigation, validation,
preferences surviving app closure, and restoration of defaults.

The host headers replace only the platform logging and mutex APIs. Mutexes use single-threaded recursion counters: this suite does not verify scheduling, locking, UART shell input or hardware.
Run the normal firmware build to validate integration with real ESP-IDF APIs.
