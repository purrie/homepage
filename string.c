#include "string.h"

void char_cat(char* str, Char c) {
    size_t len = 0;
    while (str[len] != '\0')
        len++;
    for(int i = 0; i < 4; i++) {
        if (c.bytes[i] == '\0')
            break;
        str[len] = c.bytes[i];
        len++;
    }
    str[len] = '\0';
}

char is_unicode(char c) {
    if (c & 0b1 << 7) {
        if (c & 0b1 << 6) {
            if (c & 0b1 << 5) {
                if (c & 0b1 << 4) {
                    return 4;
                }
                return 3;
            }
            return 2;
        }
        return 1;
    }
    return 0;
}

u_int8_t char_length(Char c) {
    u_int8_t len = is_unicode(c.ascii);
    assert(len != 1);
    return len > 0 ? len : 1;
}

Char get_character_at(char* str, size_t index) {
    size_t len = strlen(str);
    Char o;
    o.unicode = 0;
    if (len <= index) {
        return o;
    }

    size_t target = 0;
    size_t i = 0;
    while(target != index) {
        char uni = is_unicode(str[i]);
        if (uni) {
            for (int skip = 1; skip < uni; skip++)
                i++;
        }
        i ++;
        target ++;
    }

    o.ascii = str[i];
    char uni = is_unicode(o.ascii);
    for(int skip = 1; skip < uni; skip++)
        o.bytes[skip] = str[i + skip];

    return o;
}

size_t string_length(String* str) {
    size_t len = 0;
    while (str->buffer[len].ascii != '\0')
        len++;
    return len;
}

String cstring_to_string(char* str) {
    size_t in_index = 0;
    size_t ou_index = 0;
    String s = {0};
    while (str[in_index] != '\0') {
        char unicode = is_unicode(str[in_index]);
        s.buffer[ou_index].ascii = str[in_index];
        if (unicode) {
            for (int u = 1; u < unicode; u++) {
                s.buffer[ou_index].bytes[u] = str[++in_index];
            }
        }
        in_index++;
        ou_index++;
    }

    return s;
}

String read_line(FILE* file) {
    char out[STRING_CAPACITY] = {0};
    if (fgets(out, STRING_CAPACITY - 1, file) == NULL) {
        String s = {0};
        return s;
    }
    return cstring_to_string(out);
}

void write_string(FILE* file, String* str) {
    size_t i = 0;
    while (str->buffer[i].ascii != '\0') {
        size_t len = char_length(str->buffer[i]);
        size_t siz = sizeof(u_int8_t);
        size_t wri = fwrite(str->buffer->bytes, siz, len, file);
        if ((len * siz) != wri) {
            fclose(file);
            fprintf(stderr, "Failed to write to the file");
            exit(-1);
        }
    }
}
