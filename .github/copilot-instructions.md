## Technology Stack
* The project uses esp-idf v5.5.0, with a potential upgrade path to 5.5.2.
* The project language is C++17.
* The target hardware is the ESP32 family (esp-idf compatible).
* When selecting technologies to use (e.g. languages, frameworks, libraries), record the decision in this list for consumption by future agents.

## Testing
* The project uses esp-idf's Python integration to run onboard tests.

## Agent-First Development
* Documentation should be readable by humans, but optimized for consumption by AI agents.
* Tools used should support agent interaction (such as MCP integration).
* In cases where a frequently used tool does not support smooth agent interaction, an interaction layer should be created. This can be a separate task.
* Code should be written with AI agents in mind, without compromising readability and editability for humans.
* In projects with components/tasks that are physical or otherwise inaccessible to AI agents, provide clear instructions or documentation for completion of the task by a human. Ensure bidirectional communication and that assumptions are checked.
* Avoid including code in design documents. Including code in design documents is detrimental for several reasons relating to redundancy:
  - Double maintenance load for maintaining parity.
  - Increase context usage with duplicate information.
  - Locks in implementing agent into a specific path, instead of allowing flexibility.
  - Hinders readability of design documents.

## ESP-IDF Development
* This project uses ESP-IDF v5.5.x for ESP32-S3.
* Before running any `idf.py` commands, source the ESP-IDF environment: `. $IDF_PATH/export.sh`
* Build command: `idf.py build`
* Flash command: `idf.py -p <PORT> flash` (requires physical connection to target device)
* Monitor command: `idf.py -p <PORT> monitor` (view serial output)
* Clean build: `idf.py fullclean`
* Set target: `idf.py set-target esp32s3`
* Configure: `idf.py menuconfig` (interactive) or edit `sdkconfig.defaults`

### Component Development
* Follow esp-idf component architecture. Each reusable module should be a separate component in `components/`.
* Components should have their own `CMakeLists.txt` and `idf_component.yml` for dependency management.
* Internal components use relative paths. External components should be registered in the ESP Component Registry when mature.
* Component public headers go in `include/` subdirectory.

### MCP Tools
* Use esp-mcp (https://github.com/horw/esp-mcp) for agent interaction with ESP-IDF when available.
* When esp-mcp is not available, use terminal commands directly.

## Style
* Use spaces for indentation. Indentation width is 2 spaces.
* For a given language, determine style as follows:
  1. If present, the style guide document at `agent/style/<language>.md` takes precedence.
  2. Officially recommended/adopted style decisions, such as PEP8 for Python and CamelCase naming for C#.
  3. The most common or widely accepted style guide for the language.
* Set up automated linting to enforce style rules.

### C++ Style (Primary Language)
* Follow Google C++ Style Guide with ESP-IDF modifications.
* Use `.cpp` extension for C++ source files, `.hpp` for C++ headers.
* Naming conventions:
  - Classes/Structs: `PascalCase`
  - Functions/Methods: `snake_case` (esp-idf convention)
  - Variables: `snake_case`
  - Constants/Macros: `UPPER_SNAKE_CASE`
  - Private members: `snake_case_` (trailing underscore)
* Use `#pragma once` for header guards.
* Prefer `constexpr` over `#define` for constants.
* Use C++ standard library where appropriate, but prefer esp-idf APIs for hardware interaction.

## Code Structure
* Strongly prefer small, iterative code changes.
* Create minimally committable changes:
  - One logical unit, system, or bugfix.
  - Tests for the logical unit.
  - Change conforms to the Definition of Done.
* Keep function/method length under 50 lines where possible.
* Each function/method in non-test code must have a documentation comment.

## Testing
* Unit tests use esp-idf's Unity-based test framework.
* Host-based tests (running on development machine) are preferred for logic that doesn't require hardware.
* On-device tests are required for hardware-dependent code.
* Test command: `idf.py build` in the test app directory, then flash and run.
* For host-based tests: Use CMock/Unity with `idf.py --preview test` or custom host test harness.

## Progress Checkpoints
* Create an intermediate commit for every minimally committable change.
* Each commit's description should start with "[Agent]".
* Each commit message should clearly describe the change made.
* Each commit must meet the Definition of Done below.

## Definition of Done
* The project must build successfully: `idf.py build` exits with code 0.
* All feature code must have unit test coverage of at least 80%.
* All code must conform to relevant style guides and pass lint checks.
* Each system and/or logical block must have integration tests covering 100% of its API surface.
* Each feature and/or user story must have end-to-end tests covering all acceptance criteria.
* Each code change must be verified by running a build and running all tests.

## Before Committing
* Run the command to build the project: `idf.py build`
* Run all tests and ensure they pass.
* If hardware-dependent tests cannot be run (no device connected), document which tests are pending hardware validation.

## Autonomous Hardware Testing

Agents can perform the following hardware operations autonomously when a device is connected:

### Serial Port Detection
```bash
ls -la /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```
ESP32-S3 with USB-Serial/JTAG typically appears as `/dev/ttyACM0`.

### Flash and Verify Workflow
```bash
# 1. Build
idf.py build

# 2. Flash (auto-resets device after)
idf.py -p /dev/ttyACM0 flash

# 3. Capture serial output with timeout (non-interactive)
timeout 5 idf.py -p /dev/ttyACM0 monitor 2>&1

# 4. Parse output for expected strings
timeout 5 idf.py -p /dev/ttyACM0 monitor 2>&1 | grep -q "expected message"
```

### Autonomy Capabilities

| Operation | Autonomous | Notes |
|-----------|------------|-------|
| Detect serial port | Yes | Pattern match `/dev/ttyACM*` or `/dev/ttyUSB*` |
| Check permissions | Yes | Verify user in `dialout` group |
| Build firmware | Yes | `idf.py build` |
| Flash firmware | Yes | `idf.py -p PORT flash` |
| Capture serial output | Yes | `timeout N idf.py monitor` |
| Parse log output | Yes | grep/awk for expected strings |
| Reset device | Yes | Hard reset via RTS (automatic after flash) |
| Automated test pass/fail | Partial | Requires structured test output (Unity markers) |

### Test Output Requirements
For fully autonomous test verification, firmware tests should output parseable markers:
* Unity framework: `TEST_PASS` / `TEST_FAIL` / `n Tests n Failures`
* Custom marker: `[TEST_RESULT] PASS` or `[TEST_RESULT] FAIL: <reason>`

Example verification:
```bash
timeout 10 idf.py -p /dev/ttyACM0 monitor 2>&1 | grep -E "(PASS|FAIL|Tests.*Failures)"
```

### Limitations (Human Required)
The following still require human intervention:
* Connecting/disconnecting hardware
* Physical button presses during testing
* Reading physical display output visually
* Measuring signals with oscilloscope/logic analyzer
* Verifying LED colors or brightness
* Manipulating external test fixtures

## Hardware Interaction (Human Required)
When physical hardware interaction is needed:
1. Agent documents the required action in a `HUMAN_ACTION_REQUIRED.md` file or console output.
2. Include: exact command to run, expected outcome, and how to report result back.
3. Wait for human confirmation before proceeding with dependent tasks.

Examples requiring human action:
* Connecting/disconnecting hardware
* Physical button presses during testing
* Reading physical display output
* Measuring signals with oscilloscope/logic analyzer
* Verifying LED/visual output
