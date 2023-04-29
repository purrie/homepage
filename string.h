#ifndef STRING_H_
#define STRING_H_

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifndef STRING_CAPACITY
#define STRING_CAPACITY 256
#endif

typedef union {
    u_int8_t ascii;
    u_int32_t unicode;
    u_int8_t bytes[4];
} Char;

typedef struct {
    Char buffer[STRING_CAPACITY];
} String;

void char_cat(char* str, Char c);
char is_unicode(char c);
Char get_character_at(char* str, size_t index);
u_int8_t char_length(Char c);
size_t string_length(String* str);
String cstring_to_string(char* str);
String read_line(FILE* file);
void write_string(FILE* file, String* str);

#endif // STRING_H_
