Introduction
===================

A couple of years ago, I received an R4i Save Dongle from the R4i team as part of a promotion they
were running.  As somebody who loans their games to friends often, it's come in handy quite a few
times.  Being a mostly-Linux user, however, I was frustrated by not being able to back up my 3DS 
saves without first rebooting into Windows; Wine doesn't appear to support HID devices and there
were some issues (possibly timing-related) when using the device in a virtual machine.

So, I set to work reversing the dongle's official software so I could write my own implementation
that would work across platforms.

**005Tools** currently only works with the _R4i Save Dongle_, but it is written with extensibility
in mind.  Since that's the only device I have, though, I doubt it will be supporting any other
devices in the near future.

Changelog v0.1b
===================

 * Writing to 3DS cards works now
 * Improved save size detection for 3DS cards
 * Fixed progress bar on Win32
 * Added some error detection

Known Issues
===================

 * Card detection for DS cards that don't have their size reported (EEPROM) isn't implemented
 * Certain games have a specific flag set in official software, not implemented yet

Notes
===================

The official app writes 3DS cards in full and then writes the first 16kB *again*. 005tools
mimics this but I haven't bothered to change how the progress bar works, so don't be surprised
when the progress bar resets after writing all the data.

Also, when writing 3DS/very large cards, you'll notice the progress bar freeze after the first
128kB is written.  The official software does the same thing, and writing resumes ~7 seconds
later.  I have no idea why it does this, but it does.

Tests
===================
I'm just one man, and I only have a handful of games, but here are the ones I've tested.
Testing using a cycle of READ, ERASE, PLAY, WRITE, READ, DIFF, PLAY, where DIFF is a simple
diff -qs on the uploaded data and the read data.  

As a general rule, there aren't any problems reading, so if you come across a game that won't
write then you can just use the official software in Windows to write your backup.  If you
do come across a game that won't work with any function of this application, please open a new
issue, I may be able to fix it.

Games are EUR unless otherwise stated ( **R** = READ, **E** = ERASE, **W** = WRITE ).

| 3DS GAME                   | Size  | R | E | W |
|:---------------------------|:-----:|:-:|:-:|:-:|
| Mario Kart 7               | 512kB | ✔ | ✔ | ✔ |
| Star Fox 64 3D             | 128kB | ✔ | ✔ | ✔ |
| Theatrhythm Final Fantasy  | 128kB | ✔ | ✔ | ✔ |  
| Ocarina of Time 3D         | 128kB | ✔ | ✔ | ✔ |
| Pilotwings Resort          | 128kB | ✔ | ✔ | ✔ |
| Super Street Figher IV     | 128kB | ✔ | ✔ | ✔ |
| New Super Mario Bros. 2    | 128kB | ✔ | ✔ | ✔ |
| Mario Kart 7               | 512kB | ✔ | ✔ | ✔ |

| DS GAME                    | Size  | R | E | W |
|:---------------------------|:-----:|:-:|:-:|:-:|
| Nintendogs (Labrador)      | 256kB | ✔ | ✔ | ✔ |
| Fire Emblem Shadow Dragon  | 256kB | ✔ | ✔ | ✔ |
| Animal Crossing Wild World | 256kB | ✔ | ✔ | ✔ |
| New Super Mario Bros.      | 8kB¹  | ✔ | ✔ | ✔ |
| Super Mario 64 DS          | 8kB¹  | ✔ | ✔ | ✔ |
| Feel The Magic XY:XX       | 8kB¹  | ✔ | ✔ | ✔ |
| Metroid Prime Hunters: FH² | 512B¹ | ✔ | ✗ | ✗ |

¹ Save size not detected because the R4i firmware doesn't detect it, this appears to be due to the
  save type (EEPROM vs Flash)  
² I suspect that, but haven't tested, the official software will not write to this card either.
