# Sellerie: A GTK+ serial port terminal

 * Based on GTKTerm
 * Original code &copy; Julien Schmitt <http://www.jls-info.com/julien/linux>
 * Improvements by Zach Davis <https://fedorahosted.org/gtkterm/>

## License

>    This program is free software: you can redistribute it and/or modify
>    it under the terms of the GNU General Public License as published by
>    the Free Software Foundation, either version 3 of the License, or
>    (at your option) any later version.
>
>    This program is distributed in the hope that it will be useful,
>    but WITHOUT ANY WARRANTY; without even the implied warranty of
>    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
>    GNU General Public License for more details.
>
>    You should have received a copy of the GNU General Public License
>    along with this program.  If not, see <http://www.gnu.org/licenses/>.


## Command line options
    --help or -h : this help screen
    --config <configuration> or -c : load configuration
    --port <device> or -p : serial port device (default /dev/ttyS0)
    --speed <speed> or -s : serial port speed (default 9600)
    --bits <bits> or -b : number of bits (default 8)
    --stopbits <stopbits> or -t : number of stopbits (default 1)
    --parity <odd | even> or -a : parity (default none)
    --flow <Xon | CTS> or -w : flow control (default none)
    --delay <ms> or -d : end of line delay in ms (default none)
    --char <char> or -r : wait for a special char at end of line (default none)
    --file <filename> or -f : default file to send (default none)
    --echo or -e : switch on local echo

## Keyboard shortcuts
As Sellerie is often used like a terminal emulator,
the shortcut keys are assigned to `<ctrl><shift>`, rather than just
`<ctrl>`.  This allows the user to send keystrokes of the form `<ctrl>X`
and not have Sellerie intercept them.

 * `<ctrl><shift>L` -- Clear screen
 * `<ctrl><shift>R` -- Send file
 * `<ctrl><shift>Q` -- Quit
 * `<ctrl><shift>S` -- Configure port
 * `<ctrl><shift>V` -- Paste
 * `<ctrl><shift>C` -- Copy
 * `<ctrl>B`        -- Send break

## NOTES on RS485:
The RS485 flow control is a software user-space emulation and therefore
may not work for all configurations (won't respond quickly enough).  If this is
the case for your setup, you will need to either use a dedicated RS232 to 
RS485 converter, or look for a kernel level driver.  This is an inherent 
limitation to user space programs.


## Building:
Sellerie has a few dependencies:

  * Gtk+3 (version 3.14 or higher)
  * vte 2.91 (version 0.44 or higher)

Once these dependencies are installed, most people should simply run:

    meson build
    ninja -C build

And to install:

    ninja -C build install

If you wish to install Sellerie someplace other than the default directory, use:

    meson build -Dprefix=/install/directory
    
for an unconfigured build or

    meson config build -Dprefix=/install/directory
    
for an already existing build.

Then build and install as usual.

