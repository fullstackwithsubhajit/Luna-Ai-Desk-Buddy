# Luna (DIY OLED Desk Companion)

**Luna** is a customizable, ESP32-C3-powered OLED desk companion created
by **DevWithSubh**. It brings a little personality to a desk through
animated facial expressions, eye movement, blinking, sleep and wake-up
behaviour, and an RTC-powered clock.

> **Project identity and transparency**
>
> Luna is an independent DIY project inspired by the general idea of
> expressive OLED desk companions. It is **not a full clone or exact
> reproduction of Dasai Mochi**, and it is not affiliated with, endorsed
> by, or an official product of Dasai. Luna has its own firmware,
> behaviour, hardware choices, and room for customization. The goal is
> to build a desk companion that can be modified and expanded by its
> owner and by the maker community.

## Features

-   Animated eyes with smooth gaze movement and blinking.
-   Multiple expressions and personality states:
    -   Idle
    -   Playful expressions
    -   Excited
    -   Drowsy
    -   Sleeping
    -   Waking up
-   Push-button interaction.
-   Double-click clock overlay.
-   DS3231 real-time clock support.
-   12-hour time display with AM/PM, date, weekday, and time-based
    greeting.
-   Startup intro displaying **"DevWithSubh Presents"**.
-   Designed to be extended with new expressions, animations, and
    hardware features.

## Hardware

The current build is based on these components:

  -----------------------------------------------------------------------
  Component                           Purpose
  ----------------------------------- -----------------------------------
  ESP32-C3 development board          Main controller

  1.3-inch 128×64 SH1106 OLED         Displays Luna's face and clock

  DS3231 RTC module                   Keeps date and time

  Momentary push button               User interaction

  LiPo battery and charging/power     Portable power system (optional /
  modules                             build-dependent)

  Jumper wires and enclosure          Connections and physical build
  -----------------------------------------------------------------------

**Note:** The exact power components and enclosure may vary between
builds. Check the ratings and wiring of your own modules before
connecting a battery or power converter.

## Pin connections

The current firmware uses the following ESP32-C3 pins:

  -----------------------------------------------------------------------
  Signal                                                     ESP32-C3 pin
  ------------------------------ ----------------------------------------
  OLED SDA                                                         GPIO 8

  OLED SCL                                                         GPIO 9

  DS3231 SDA                                                       GPIO 8

  DS3231 SCL                                                       GPIO 9

  Push button                                                      GPIO 3

  Push button other terminal                                          GND

  OLED VCC                                                            3V3

  OLED GND                                                            GND

  RTC VCC                            3V3 (for the physical RTC module, if
                                        supported by that specific board)

  RTC GND                                                             GND
  -----------------------------------------------------------------------

The OLED and RTC share the I²C bus. This is expected: both devices use
SDA and SCL, but have different I²C addresses.

The button is configured with `INPUT_PULLUP`, so it should connect GPIO
3 to GND when pressed.

**Power warning:** Do not connect a LiPo battery directly to the ESP32
5V pin or to the OLED. Do not connect an adjustable boost converter to
the ESP32 until its output voltage and polarity have been checked with a
multimeter. Confirm the pin labels and voltage requirements of your
exact ESP32-C3 and RTC modules before powering the circuit.

## Software and libraries

The firmware is written in Arduino-style C++ for the ESP32-C3.

Libraries used by the current sketch:

-   `Wire`
-   `Adafruit GFX Library`
-   `Adafruit SH110X`
-   `RTClib`

Install the ESP32 board support package and these libraries through the
Arduino IDE Library Manager (or the equivalent package manager in your
chosen Arduino workflow).

## Circuit Diagram
<img width="819" height="539" alt="image" src="https://github.com/user-attachments/assets/b1fb8b30-0c14-4783-962d-983fc5b6fb99" />


## Getting started

1.  Assemble the ESP32-C3, OLED, RTC, and button using the pin table
    above.
2.  Install the required ESP32 board support and libraries.
3.  Open the Luna `.ino` sketch in Arduino IDE.
4.  Select the correct ESP32-C3 board and serial/upload port.
5.  Verify the I²C wiring and OLED address (`0x3C`) for your modules.
6.  Upload the sketch.
7.  On startup, Luna displays the **DevWithSubh Presents** intro and
    then starts its normal personality loop.

If the RTC has lost its backup power, the sketch can set its time from
the compile date/time as a fallback. Set the RTC to the correct local
time when needed.

## Wokwi simulation

A Wokwi circuit can be used to test the general logic and interaction
flow. The supplied simulation wiring uses an RTC component labelled
**DS1307**, while the physical build uses a **DS3231**. These are
different RTC chips, even though they share a similar I²C interface.

The Wokwi display component may also use an SSD1306 driver, whereas the
physical OLED in this build is an SH1106. If the simulation component or
its libraries differ from the physical hardware, a simulation-specific
display/RTC adaptation may be required. Keep such changes separate from
the physical-hardware firmware so the working SH1106 build is not
accidentally altered.

Simulation does not validate the real battery, charger, boost-converter,
or enclosure wiring.

## Customization

Luna is intended to be modified. Possible areas to customize include:

-   Eye shapes, sizes, positions, and animation timing.
-   Additional facial expressions and personality states.
-   Button gestures and interaction patterns.
-   Startup animation and text.
-   Clock layout, date format, and greetings.
-   Enclosure design, sensors, and other hardware add-ons.

Make a backup or use Git commits before changing the firmware. That
makes it easier to return to a known-working version.

## Project status

This is a personal DIY maker project. Hardware revisions, simulation
support, and firmware features may evolve over time. Not every feature
or optional power component is guaranteed to be present in every build.

## License

The firmware is released under the **MIT License**. See the
[`LICENSE`](LICENSE) file for the full license text.

The MIT License permits use, modification, distribution, and commercial
use, provided the copyright and license notice are retained. If hardware
design files are added later, consider licensing those files separately.

## Credits and disclaimer

Created by **DevWithSubh**.

Luna is an independent project. Any third-party names are used only to
describe inspiration or compatibility; no sponsorship, partnership, or
affiliation is implied.
