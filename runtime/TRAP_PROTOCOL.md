# Trap / Check Protocol

## PASS
Write 0 to 0x1FFFF000

## FAIL
Write non-zero code to 0x1FFFF000

## check(cond)
- if cond is true: continue
- if cond is false: badtrap(1)