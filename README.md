<div id="top"></div>

<p align="center">
  <a href="https://github.com/beyluta/WinWidgets">
    <img src="https://img.shields.io/badge/Version-2.0.0-green" alt="WinWidgets version" />
  </a>
</p>

<br />
<div align="center">
  <a href="https://github.com/beyluta/WinWidgets">
    <img src="assets/imgs/icon.png" alt="Logo" width="80" height="80">
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

**WinWidgets** makes web-based desktop widgets easy to develop. Using `HTML`, `CSS`, and `JavaScript` create your own widgets on the fly.

This is what makes this project interesting:

- 🧰 Focus on creating your widgets with all the usual web tools to your disposal
- 👆 Develop complex widgets using a high-level programming language (JS)
- 🖱️ Develop your widgets from anywhere then simply drag and drop to easily port it over
- ⌨️ Have better control over your widget's window with built-in front-end tags

## Support

These are the platforms officially supported by WinWidgets. Note that the software may
run on operating systems or distributions not listed here but it isn't guaranteed.

| Platform | Availability | Supported Version |
| -------- | ------------ | ----------------- |
| Linux    | ✅           | Arch              |
| Windows  | ✅           | Windows 11        |
| MacOS    | ❌           | N/A               |

## Building

This is a brief guide for all supported platforms to compile and run the application.

### Windows prerequisites

For Windows 11 you need to install `mingw64` and put in inside the `Path` environment variable.

### Linux prerequisites

For Linux you need the packages `cairo` and `webkitgtk-6.0`. Make sure to get their
corresponding `-dev` packages as well, else you won't be able to compile.

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

1. Fork the Project
2. Create your Feature Branch (`git checkout -b feature/AmazingFeature`)
3. Commit your Changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the Branch (`git push origin feature/AmazingFeature`)
5. Add yourself to the CONTRIBUTORS.txt file
6. Open a Pull Request
