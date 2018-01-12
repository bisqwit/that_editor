# That editor

It’s that editor.

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

## Why 16-bit DOS

I wrote the editor in 16-bit DOS because I thought there would otherwise
be significant troubles trying to mix 16-bit interrupt callbacks with
32-bit protected-mode code. So I used Borland C++ 3.1.

This compiler by Borland was created before C++ was standardized, and it required
me to make many sacrifices about style / sanity in the source code.
As such, the code is not representative of good programming practices
for C++ programming, not by a long shot.

Incidentally, because it’s 16-bit, it also has serious memory limits
and other bugs associated with it. This forced me to, eventually,
port it for 32-bit DJGPP despite my initial fears.
I completed this port on 2018-01-12. It can be found in the 32bit directory.
Note that despite this DJGPP port being compiled on very modern GCC 7.2.0
with support for C++17, the codebase is still almost exactly the same
as the Borland C++ version.
