# Build customization

**Attention:** this directory is excluded from Git so you can customize your local build even further without having to fork the project.

## How to use

If you create the file `ViciusPreCustomizeMe.h` **or** `ViciusPostCustomizeMe.h` in this directory, you can override build flags and macros without having to touch the core sources.

As the names suggest; `ViciusPreCustomizeMe.h` is processed **before** the content of `CustomizeMe.h` and `ViciusPostCustomizeMe.h` is processed **after** reading `CustomizeMe.h`, allowing you to undo and overwrite the default values.
