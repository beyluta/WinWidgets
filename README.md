<p align="center">
  <a href="https://github.com/beyluta/WinWidgets">
    <img src="https://img.shields.io/github/v/release/beyluta/WinWidgets?label=Version&color=green" alt="version badge">
  </a>
</p>

<br />
<div align="center">
  <a href="https://github.com/beyluta/WinWidgets">
    <img src="assets/imgs/icon-cropped.png" alt="Logo" width="120" height="80">
  </a>

  <h3 align="center">WinWidgets</h3>
  <div align="center">
    <img src="https://img.icons8.com/color/48/000000/windows-11.png" alt="Windows logo" width="40" />&nbsp;
    <img src="https://img.icons8.com/color/48/000000/linux.png" alt="Linux logo" width="40" />
  </div>

  <p align="center">
    Open-Source Widget application for Windows and Linux
    <br />
    <br />
    <a href="https://github.com/beyluta/WinWidgets/issues">Report Bug</a>
    ·
    <a href="https://github.com/beyluta/WinWidgets/issues">Request Feature</a>
    ·
    <a href="https://github.com/beyluta/WinWidgets/discussions/40">Submit Widget</a>
  </p>
</div>

## About

**WinWidgets** makes web-based desktop widgets easy to develop.
Use a mix of `HTML`, `CSS`, and `JavaScript` to create your own widgets on the fly.

This is what makes this project interesting:

- Made by humans for humans; no vibe-coding.
- Focus on creating your widgets with all the usual web tools to your disposal.
- Develop complex widgets using JavaScript.
- Create your widgets using your preferred tools.
- Listen to native OS events and act on them programmatically.

## Support

These are the platforms officially supported by WinWidgets.

| Platform | Availability | Supported Version |
| -------- | ------------ | ----------------- |
| Windows  | ✅           | Windows 11        |
| Linux    | ⚠️ (W.I.P)   | Arch              |
| MacOS    | ❌           | N/A               |

> The software may run on operating systems or distributions not
> listed here but it isn't guaranteed.

## Screenshots

![Purple Theme](assets/imgs/purple_theme.png)

![Calm Theme](assets/imgs/calm_theme.png)

![Default Theme](assets/imgs/default_theme.png)

## Building

This is a brief guide for all supported platforms to compile and run the application.

### Windows prerequisites

Download the following applications using Chocolatey:

```bash
choco install git msys2 mingw make llvm
```

> You may also download them manually. If you choose to do so:
> `msys2` and `mingw` must be in the PATH environment variables.

After installing, open the MSYS2 terminal application and install these dependencies:

```bash
pacman -S mingw-w64-x86_64-curl mingw-w64-x86_64-libzip
```

`curl` and its dependencies will be installed by default in `C:/tools/msys64`. Verify
this path is correct and update the `MINGW64` variable in the makefile if needed.

### Linux prerequisites

For Linux you need the packages `gtk-3.0`, `appindicator3` and `webkitgtk-4.1`.
Make sure to get their corresponding `-dev` packages as well or else you
won't be able to compile.

### Common commands

These are commands that work for all supported platforms.
Read them carefully to successfully compile the application.

When building for the first time you must run the following command to fetch dependencies:

```bash
git submodule update --init --recursive
```

(Optional) Consider running the following when pulling remote changes to update dependencies:

```bash
git submodule update --recursive --remote
```

Compile the binary for debugging with the following command:

```bash
make debug
```

Compile the binary for release with the following command:

```bash
make release
```

Execute the program with:

```bash
make run
```

## Contributing

> Please disclose the use of any A.I tools used for development in the PR.
> Using artificial intelligence for help or research is very welcome.
> Fully vibe-coded solutions will be rejected.

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Add yourself to the CONTRIBUTORS.txt file
4. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
5. Push to the Branch (`git push origin feature/AmazingFeature`)
6. Open a Pull Request
