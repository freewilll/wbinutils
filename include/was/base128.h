#ifndef _BASE128_H
#define _BASE128_H

int encode_sleb128(int value, char *data);
int encode_uleb128(int value, char *data);

#endif
