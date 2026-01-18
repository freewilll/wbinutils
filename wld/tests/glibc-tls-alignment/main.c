__thread int data1 = 42;        // This file is linked last, causing the TLS segment size to not be a multiple of the aligment (8)
extern __thread long data2;     // Forces the alignment to be 8

int main() {
    return !(data1 == 42 && data2 == 43);
}
