#include "trap.h"

int add(int a, int b) {
    return a + b;
}

int main(void) {
    int x = add(19, 23);
    check(x == 42);
    return 0;
}