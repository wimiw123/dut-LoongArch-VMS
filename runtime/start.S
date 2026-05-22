    .section .text.start
    .globl _start
    .extern main
    .extern goodtrap
    .extern badtrap

_start:
    # r3 = sp
    lu12i.w    $r3, 0x0000f      # sp = 0x0000F000

    # 调用 main()
    bl         main

    # main 返回值在 r4
    addi.w     $r12, $r0, 0
    beq        $r4, $r12, pass_path

fail_path:
    bl         badtrap

pass_path:
    bl         goodtrap

hang:
    b          hang