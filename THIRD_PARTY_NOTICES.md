# Third-party notices

CoverPlayer uses the following third-party components. Release packages must
include their applicable license texts.

The ARM64 packages include `licenses/` with Debian's package copyright files,
the common license texts they reference, and `package-versions.txt` for the
exact bundled runtime libraries. The package build fails if an included
library has no identifiable Debian copyright file.

Switch alpha packages statically link the devkitPro Switch portlibs. They
include license texts and the exact toolchain/package versions in `licenses/`.
The `relink/` directory includes CoverPlayer source, the corresponding mpg123
1.31.3 source and Switch patch, and instructions for rebuilding with a modified
LGPL decoder. Roboto Mono is embedded in the NRO with its Apache 2.0 license.
Additional Switch dependencies include SDL2_image (zlib), HarfBuzz (MIT),
libpng, libjpeg-turbo, libwebp (BSD), zlib, bzip2, libnx (ISC), Mesa and
libdrm_nouveau (MIT), newlib, and GCC runtime libraries with their runtime
exception. Their notices are included in the Switch package.

- SDL2, distributed under the zlib license.
- SDL2_ttf, distributed under the zlib license.
- FreeType, distributed under the FreeType Project License.
- Roboto Mono Bold, distributed under the Apache License 2.0.
- MinGW-w64/GCC runtime libraries used by the Windows development build. Their
  license texts and GCC Runtime Library Exception terms must accompany a
  distributable Windows package.

- libmpg123, distributed under the GNU Lesser General Public License version
  2.1. The Windows development build links dynamically to `libmpg123-0.dll`.
  Its complete license text must accompany distributable packages.
