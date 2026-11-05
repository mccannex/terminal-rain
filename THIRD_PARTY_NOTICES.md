# Third-party notices

Terminal Rain's original contributions by mccannex are licensed under the MIT License in [LICENSE](LICENSE), including those contributions in earlier releases. That grant does not relicense third-party material. The inherited simulation, SDL2 and font material retain the terms below. Retain these notices and their accompanying license texts when redistributing Terminal Rain.

## Matrix Screen Saver and MatrixSS — CC BY 3.0

Terminal Rain adapts the falling-code stream simulation from **Matrix Screen Saver by Louai Munajim**, originally released in 1999, using **OscarL's MatrixSS** as a reference. The stream speed, trail length and column-spawning logic trace back to that work. Credit is given to both authors; no endorsement is implied.

- Original author and source: Louai Munajim, <http://elouai.com/the_matrix_screensaver.php>.
- Archived original page: <https://web.archive.org/web/19991205035123/http://www.louai.com/coding.html>.
- Reference version: OscarL, MatrixSS, <https://github.com/OscarL/MatrixSS>.
- License evidence: the [upstream README at revision 69b74b4](https://github.com/OscarL/MatrixSS/blob/69b74b484c867e43ca8577f0cabc34701ab3a1be/Readme.md#license) quotes Louai's CC BY 3.0 notice and retains the same license for MatrixSS.

The inherited material remains under **Creative Commons Attribution 3.0 Unported (CC BY 3.0)**: <https://creativecommons.org/licenses/by/3.0/>. The complete terms, including the warranty disclaimer and limitation of liability, are in [licenses/CC-BY-3.0.txt](licenses/CC-BY-3.0.txt).

Terminal Rain changes the original Win32/GDI implementation into a shared C++ simulation rendered with SDL2, adds native Windows/macOS/Linux integration, multiple displays and display scaling, and changes rendering, colors, stream arrival scheduling and lifecycle management. These are adaptations, not an unaltered distribution of the original screensaver. MIT applies to mccannex's new contributions; the original material's CC BY attribution and other terms continue to apply.

## SDL2 — zlib license

SDL2 release-2.30.9 is built from <https://github.com/libsdl-org/SDL> and linked statically into the distributed executables. Its source is obtained by CMake and is not relicensed by Terminal Rain. The full upstream copyright, disclaimer and permission notice are preserved in [licenses/SDL2.txt](licenses/SDL2.txt).

## TerminalVector font — upstream public-domain dedication and qualification

The embedded 8x12 glyph atlas in `core/glyph_atlas_data.cpp` and generated screensaver thumbnails use George Yohng's TerminalVector font: <https://www.yohng.com/software/terminalvector.html>.

George Yohng disclaims his rights and releases the vector version into the public domain. His notice also identifies Bitstream and Microsoft copyrights in the original Terminal font and includes a qualification concerning commercial distribution. This project preserves that notice in full rather than treating it as a grant from those other rights holders.

The unmodified notice is included at [licenses/TerminalVector.txt](licenses/TerminalVector.txt) and, in the source repository, `assets/fonts/TerminalVector.ttf.LICENSE.txt`. MIT does not relicense the font or its derived glyph images.

## Release downloads

`Terminal.Rain-Licenses.zip` accompanies the platform downloads. It contains `LICENSE`, this notice, and all three files in `licenses/`. Keep these documents with copies of the executable or screensaver bundle. The macOS bundle's signature is preserved; the license archive is distributed alongside it.
