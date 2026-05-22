# Minimal C Runtime Plan

## Memory Layout
- Program entry: 0x1000
- Data base: 0x2000
- Stack top: 0xF000
- TestDevice MMIO: 0x1FFFF000

## Program Exit Convention
- main() returns 0  -> goodtrap
- main() returns !=0 -> badtrap(return_code)

## Trap Convention
- goodtrap: write 0 to TestDevice::BASE_ADDR
- badtrap(code): write code to TestDevice::BASE_ADDR

## Future Runtime Files
- start.S
- trap.h
- linker.ld