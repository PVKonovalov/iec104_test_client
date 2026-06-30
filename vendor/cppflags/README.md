# cppflags

A minimal header-only C++ command-line flag parsing library.

## Features

- Parses `--name value` integer flags and presence-only boolean flags (`--name`)
- Configurable default values and descriptions
- Built-in `--help` / `-h` support with usage output
- Throws `cppflags::ParseError` on unknown flags or missing values
- Single header, no dependencies beyond the C++ standard library

## Usage

Copy `include/cppflags/cppflags.h` into your project or add this directory as a CMake subdirectory.

### CMake

```cmake
add_subdirectory(vendor/cppflags)
target_link_libraries(your_target PRIVATE cppflags)
```

### Example

```cpp
#include "cppflags/cppflags.h"

int main(int argc, char *argv[]) {
    int port = 0;
    int timeout = 30;
    bool verbose = false;

    cppflags::FlagSet flags;
    flags.Int("port",    &port,    502,  "TCP port to connect to");
    flags.Int("timeout", &timeout, 30,   "Connection timeout in seconds");
    flags.Bool("verbose", &verbose,      "Enable verbose output");

    try {
        flags.Parse(argc, argv);
    } catch (const cppflags::ParseError &e) {
        std::cerr << "Error: " << e.what() << "\n";
        flags.printUsage(argv[0]);
        return 1;
    }

    // port, timeout, verbose are now set from argv or left at their defaults
}
```

Invoke with:

```
./myapp --port 1024 --timeout 60 --verbose
```

### Flag types

| Method | Syntax | Description |
|---|---|---|
| `Int(name, &target, default, description)` | `--name value` | Integer flag; requires a value argument |
| `Bool(name, &target, description)` | `--name` | Boolean flag; presence sets target to `true` |

## License

Copyright 2026 Pavel Konovalov. Licensed under the GNU General Public License v3.
