Also see the Hackaday writeup for this project:

  https://hackaday.io/project/163169-o-blinkenbaum


== Notes ==

The SimpleStripLights class is simple in that it doesn't deal with
24-bit color (uses 6-bit color in the faders, for example). It's not
terribly simple in other regards.

== PROTOCOL ==

This is a character-oriented protocol; all of the '#' placeholders are
single bytes.

* Mode-setting commands (all single bytes)

r raw mode
  1## when in raw mode, set pixel ## to current color and fade prefs
T twinkle mode
W wipe mode
! chase mode
t tardis mode
C (solid) color mode
p pulse mode

* Data-setting commands (multiple bytes)

f#  set fade preference
F#  set fade mode preference (currently 0=normal, 1=inverted logic)
c### set color
x### set second color
R# set repeat preference
b# set brightness (0-255)
^# respond to broadcast packets (0=no; 1=yes; default = yes)



r raw mode

  Sets 1 pixel at a time. Use 'c###' to set the color; then '1##' to
  set a pixel to that color. Honors current fade preference.

T twinkle mode

  Pixels fade in and out randomly, using the current primary and
  secondary color.

W wipe mode

  A single color begins to fade in from the first pixel. After a short
  delay, then the second. And third. And so on. Each pixel stays on
  indefinitely.

! chase mode

  A pulse of fading in/out color "chases" from the start to the end of
  the strip. This is the same as "wipe mode", except that each pixel
  fades to black after hitting its target fade-in.

p pulse mode

  This is a two-color chase. As soon as the pixel hits its target
  color, it fades out like chase mode; then when it hits black, it
  changes target colors to the secondary.

t tardis mode

  One single color (default: blue, which gives it the name) pulses
  in/out. For me, this evokes the dematerialization sound while I
  think of the console column moving up and down :)

C color mode

  Set the whole strip to a given color, immediately (honors fade).


Note that the brightness stuff is broken, but gives off some
interesting disco-like effects :)
