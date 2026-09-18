# Xbox / UWP

The `uwp-xbox` branch builds an x64 MSIX bundle for Xbox Developer Mode. The
package contains Paper Boat's executable and non-copyrighted port archive. It
does not contain a Paper Mario ROM or ROM-derived game assets.

## Game assets

Paper Boat supports the US ROM whose SHA-1 is
`3837f44cda784b466c9a2d99df70d77c322b97a0`.

A desktop build can generate the required archive without opening a window:

```sh
Paperboat --extract-to "/path/to/Paper Mario.z64" "/output/directory"
```

Copy the generated file to `E:\PaperBoat\pm64.o2r` on an Xbox-formatted media
USB drive. Optional `paperboat-hd.o2r` and a `mods` directory may be placed in
the same folder. Saves and configuration remain in the app's LocalState so
package updates do not overwrite them.

## Controls

The Xbox View/Back button opens and closes the port menu. Controller navigation
is enabled by default for the UWP build; use the D-pad to move, A to select, and
B to go back.

## Build

The `build-uwp` GitHub Actions workflow uses the supported Windows clang-cl
native build, wraps the resulting DLL in a CoreWindow UWP package, signs it,
and uploads `Paper-Boat-UWP-US-x64`.
