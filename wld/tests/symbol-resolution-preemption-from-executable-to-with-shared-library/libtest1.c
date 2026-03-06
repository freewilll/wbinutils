// Is overridden by f() in main and should never be called
int f() {
    return 2;
}

// A wrapper to check that functions in the shared library call the preempted version of f().
int g() {
    return f();
}
