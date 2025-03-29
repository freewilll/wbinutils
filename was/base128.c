// Encode value into a preallocated array data, returning the size
int encode_sleb128(int value, char *data) {
    int pos = 0;
    int more = 1;
    while (more) {
        unsigned char c = value & 0x7f;
        value >>= 7;

        int sign_bit = c & 0x40;
        if ((value == 0 && !sign_bit) || (value == -1 && sign_bit))
            more = 0;

        else
            c |= 0x80;

        data[pos++] = c;
    }

    return pos;
}

// Encode value into a preallocated array data, returning the size
// The value is not a proper unsigned integer, so this will likely fail for
// stupidly huge values.
int encode_uleb128(int value, char *data) {
    int pos = 0;
    while (1) {
        unsigned char c = value & 0x7f;
        value >>= 7;
        if (value) c |= 0x80;
        data[pos++] = c;
        if (!value) break;
    }

    return pos;
}
