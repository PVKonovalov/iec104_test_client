/*
 *  Copyright 2026 Pavel Konovalov
 *
 *  This file is part of cppflags
 *
 *  cppflags is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  cppflags is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with cppflags. If not, see <http://www.gnu.org/licenses/>.
 *
 *  See LICENSE file for the complete license text.
 */

#pragma once

#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace cppflags {
    class ParseError : public std::runtime_error {
    public:
        explicit ParseError(const std::string &msg) : std::runtime_error(msg) {
        }
    };

    class FlagSet {
    public:
        // Set a preamble line printed before the flags list in the usage output.
        void SetPreamble(const std::string &preamble) {
            preamble_ = preamble;
        }

        // Register an int flag with a default value and description.
        void Int(const std::string &name, int *target, int defaultValue, const std::string &description) {
            *target = defaultValue;
            intEntries_.push_back({name, description, defaultValue, target});
        }

        // Register a boolean flag. Presence of --name sets *target = true; absence leaves it false.
        void Bool(const std::string &name, bool *target, const std::string &description) {
            *target = false;
            boolEntries_.push_back({name, description, target});
        }

        // Register a string flag with a default value and description.
        void String(const std::string &name, std::string *target, const std::string &defaultValue,
                    const std::string &description) {
            *target = defaultValue;
            stringEntries_.push_back({name, description, defaultValue, target});
        }

        // Parse argc/argv, skipping argv[0] (program name).
        // Returns the index of the first non-flag argument, or argc if none.
        int Parse(int argc, const char *const*argv) {
            int i = 1;
            while (i < argc) {
                std::string arg = argv[i];

                if (arg == "--help" || arg == "-h") {
                    printHelp(argv[0]);
                    std::exit(0);
                }

                if (arg.size() < 3 || arg[0] != '-' || arg[1] != '-') {
                    break; // first non-flag argument
                }

                std::string name = arg.substr(2);

                // Check bool entries first (no value argument consumed)
                auto bit = std::find_if(boolEntries_.begin(), boolEntries_.end(),
                                        [&](const BoolEntry &e) { return e.name == name; });
                if (bit != boolEntries_.end()) {
                    *bit->target = true;
                    ++i;
                    continue;
                }

                // Check string entries
                auto sit = std::find_if(stringEntries_.begin(), stringEntries_.end(),
                                        [&](const StringEntry &e) { return e.name == name; });
                if (sit != stringEntries_.end()) {
                    if (i + 1 >= argc) {
                        throw ParseError("flag --" + name + " requires a value");
                    }
                    *sit->target = argv[++i];
                    ++i;
                    continue;
                }

                auto it = std::find_if(intEntries_.begin(), intEntries_.end(),
                                       [&](const IntEntry &e) { return e.name == name; });

                if (it == intEntries_.end()) {
                    throw ParseError("unknown flag: --" + name);
                }

                if (i + 1 >= argc) {
                    throw ParseError("flag --" + name + " requires a value");
                }

                ++i;
                std::string value = argv[i];

                auto badValue = [&]() {
                    std::string msg;
                    msg.reserve(16 + name.size() + value.size());
                    msg.append("flag --").append(name).append(": invalid integer value: ").append(value);
                    return ParseError(msg);
                };
                try {
                    size_t pos;
                    const int parsed = std::stoi(value, &pos);
                    if (pos != value.size()) throw badValue();
                    *it->target = parsed;
                } catch (const ParseError &) {
                    throw;
                } catch (...) {
                    throw badValue();
                }

                ++i;
            }
            return i;
        }

        void printUsage(const char *prog) const {
            printHelp(prog);
        }

    private:
        struct IntEntry {
            std::string name;
            std::string description;
            int defaultValue;
            int *target;
        };

        struct BoolEntry {
            std::string name;
            std::string description;
            bool *target;
        };

        struct StringEntry {
            std::string name;
            std::string description;
            std::string defaultValue;
            std::string *target;
        };

        void printHelp(const char *prog) const {
            if (!preamble_.empty())
                std::cout << preamble_ << "\n\n";
            std::cout << "Usage: " << prog << " [flags]\n\nFlags:\n";
            for (const auto &e: stringEntries_) {
                std::cout << "  --" << e.name << " <string>"
                        << "  (default: \"" << e.defaultValue << "\")\n"
                        << "      " << e.description << "\n";
            }
            for (const auto &e: intEntries_) {
                std::cout << "  --" << e.name << " <int>"
                        << "  (default: " << e.defaultValue << ")\n"
                        << "      " << e.description << "\n";
            }
            for (const auto &e: boolEntries_) {
                std::cout << "  --" << e.name << "\n"
                        << "      " << e.description << "\n";
            }
            std::cout << "  --help\n      Show this help message\n";
        }

        std::string preamble_;
        std::vector<IntEntry> intEntries_;
        std::vector<BoolEntry> boolEntries_;
        std::vector<StringEntry> stringEntries_;
    };
} // namespace cppflags
