# That editor

It’s *that* editor.

Some more information at: https://github.com/bisqwit/compiler_series/tree/master/ep1/dostools

## What

Provided strictly AS-IS, with a mild warning that you really do not want to
use this editor. Seriously. It won’t do you any good.
Get a *real editor* (https://joe-editor.sourceforge.io/) instead.

## Why does it exist

This editor was written only because Joe would not run in DOSBox.
I used DOSBox only because I had too slow hardware to do desktop recording
in real time, and DOSBox contains a full-featured simulated environment
with a built-in recorder that works regardless of host system speed.

There *were* other syntax-coloring editors for DOS, and I actually
wanted to and *did* use Borland C++ 3.1 IDE for some of my earlier
videos. But then I wanted to make a video with wider screen,
and despite my best efforts, I could not binary-patch Borland C++
to perfectly cooperate with screens that have other width
than 80 characters.
So I was cornered and had no choice but to make my own editor.
I could no longer remember how to use Turbo Vision,
and I couldn’t bother to study, so I wrote the editor entirely from scratch.

## Why 16-bit DOS

I wrote the editor for *16-bit* DOS because I thought there would
be significant troubles trying to mix 16-bit interrupt callbacks with
32-bit protected-mode code. Also I don’t think I knew back then,
that DJGPP has been as modernized as it indeed has. So I used Borland C++ 3.1.

This compiler by Borland was created before C++ was standardized, and it required
me to make many sacrifices about style / sanity in the source code.
For example, it did not support namespaces or templates. No STL!
As such, the code is not representative of good programming practices
for C++ programming, not by a long shot.

Incidentally, because it’s 16-bit, it also has serious memory limits
and other bugs associated with it.
*Eventually* this *forced* me to port it for 32-bit DJGPP despite my initial fears.
I completed this port on 2018-01-12 in just a couple of hours
(huh, maybe the coding style was not *that* bad after all).
It can be found in the 32bit directory.
Note that despite this DJGPP port being compiled on very modern GCC 7.2.0
with support for C++17, the codebase is still almost exactly the same
as the Borland C++ version.

## How to use it

Did you miss the part where I warned you really don’t want to use this editor?
You did? Ok. The editor uses most of the same inputs as Joe, my favorite editor.
You can find the list of keybindings in the `doc/` subdirectory.

## How does it work

### Text storage

It represents the editor buffer as a vector of lines. Each line is a vector
of an element type that encodes both the character and its current color
attribute. This color attribute used to be a VGA-compatible 8-bit attribute
byte, but in commits
[7321f15a](https://github.com/bisqwit/that_editor/commit/7321f15a0f31bb86646a54745e042cd459964fcb)
and 
[05eaf3c7](https://github.com/bisqwit/that_editor/commit/05eaf3c7bb614ee0803763a4acebd3b83ecf9d27)
I added support
for xterm-256color compatible extended color attributes. This requires
[special support](https://github.com/bisqwit/compiler_series/blob/master/ep1/dostools/dosbox/0016-Add-support-for-xterm-256color.patch) from DOSBox.

### Syntax highlighting

Syntax highlighting operates on a state machine that is modelled after
the syntax highlighting engine in Joe. In fact, this editor uses the exact
same JSF files to configure the syntax highlighting. You can learn more
about the JSF system in the JSF files that come with Joe.

Syntax highlighting is applied in real time using a virtual callback
that supports two options: Get next character,
and recolor some previous section using a select attribute.
The source code file is continuously scanned from beginning to the end
until everything has been scanned at least once since the last update.

### Mario

The Mario animation at the top uses the same principle as Norton tools
did on DOS to show an arrow mouse cursor in text mode.

It reads the font for those characters that are currently under Mario,
treats those characters as bitmaps, adds Mario into them,
and then installs the modified characters in the font and replaces
the character indexes on screen in that spot to refer to the modified
characters.
