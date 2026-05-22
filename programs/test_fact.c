#include "trap.h"

int fact(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * fact(n - 1);
}

int main(void) {
    int x = fact(5);
    check(x == 120);
    return 0;
}