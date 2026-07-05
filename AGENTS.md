# Agent Notes

- Do not run PlatformIO builds, uploads, serial monitors, or OTA test commands by default.
- The user builds, uploads, and tests OTA changes locally.
- Only run `pio`, `/Users/aki/.platformio/penv/bin/pio`, or OTA-related commands when the user explicitly asks for it.
- For firmware edits, use static checks and summarize the expected impact; leave hardware validation to the user unless requested.
