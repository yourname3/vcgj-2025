#include "engine/serialize/serialize.h"

#include <string.h>
#include <stdio.h>
#include "engine/alloc.h"

// Serialize everything as little-endian, so that in theory the compiler could
// optimize stuff.

void
write_u8(struct serializer *s, uint8_t value) {
    s->write_byte(s, value);
}

void
write_u16(struct serializer *s, uint16_t value) {
    uint8_t bytes[2];
    bytes[0] = (uint8_t)((value >> 0) & 0xFF);
    bytes[1] = (uint8_t)((value >> 8) & 0xFF);
    s->write_bytes(s, bytes, 2);
}

void
write_u32(struct serializer *s, uint32_t value) {
    uint8_t bytes[4];
    bytes[0] = (uint8_t)((value >>  0) & 0xFF);
    bytes[1] = (uint8_t)((value >>  8) & 0xFF);
    bytes[2] = (uint8_t)((value >> 16) & 0xFF);
    bytes[3] = (uint8_t)((value >> 24) & 0xFF);
    s->write_bytes(s, bytes, 4);
}

void
write_u64(struct serializer *s, uint64_t value) {
    uint8_t bytes[8];
    bytes[0] = (uint8_t)((value >>  0) & 0xFF);
    bytes[1] = (uint8_t)((value >>  8) & 0xFF);
    bytes[2] = (uint8_t)((value >> 16) & 0xFF);
    bytes[3] = (uint8_t)((value >> 24) & 0xFF);
    bytes[4] = (uint8_t)((value >> 32) & 0xFF);
    bytes[5] = (uint8_t)((value >> 40) & 0xFF);
    bytes[6] = (uint8_t)((value >> 48) & 0xFF);
    bytes[7] = (uint8_t)((value >> 56) & 0xFF);
    s->write_bytes(s, bytes, 8);
}

void
write_i8(struct serializer *s, int8_t value) {
    uint8_t byte;
    memcpy(&byte, &value, sizeof(byte));
    write_u8(s, byte);
}

void
write_i16(struct serializer *s, int16_t value) {
    uint16_t uvalue;
    memcpy(&uvalue, &value, sizeof(uvalue));
    write_u16(s, uvalue);
}

void
write_i32(struct serializer *s, int32_t value) {
    uint32_t uvalue;
    memcpy(&uvalue, &value, sizeof(uvalue));
    write_u32(s, uvalue);
}

void
write_i64(struct serializer *s, int64_t value) {
    uint64_t uvalue;
    memcpy(&uvalue, &value, sizeof(uvalue));
    write_u64(s, uvalue);
}

void
write_float(struct serializer *s, float value) {
    uint32_t uvalue;
    memcpy(&uvalue, &value, sizeof(uvalue));
    write_u32(s, uvalue);
}

void
write_double(struct serializer *s, double value) {
    uint64_t uvalue;
    memcpy(&uvalue, &value, sizeof(uvalue));
    write_u64(s, uvalue);
}

#define IMPL_WRITE_ARRAY(sname, datatype) \
void \
write_ ## sname ## _array (struct serializer *s, size_t length, datatype *values) { \
    write_u64(s, (uint64_t)length); \
    for(size_t i = 0; i < length; ++i) { write_ ## sname (s, values[i]); } \
}

IMPL_WRITE_ARRAY(u8, uint8_t)
IMPL_WRITE_ARRAY(u16, uint16_t)
IMPL_WRITE_ARRAY(u32, uint32_t)
IMPL_WRITE_ARRAY(u64, uint64_t)

IMPL_WRITE_ARRAY(i8, int8_t)
IMPL_WRITE_ARRAY(i16, int16_t)
IMPL_WRITE_ARRAY(i32, int32_t)
IMPL_WRITE_ARRAY(i64, int64_t)

IMPL_WRITE_ARRAY(float, float)
IMPL_WRITE_ARRAY(double, double)

void
write_array(struct serializer *s, size_t length, void *array, size_t stride, void (*write_element)(struct serializer*, void*)) {
    write_u64(s, (uint64_t)length);
    char *ptr = array;
    for(size_t i = 0; i < length; ++i) {
        char *elemstart = ptr + stride * i;
        write_element(s, elemstart);
    }
}

struct stdio_writer {
    struct serializer serial;
    FILE *file;
};

void
stdio_write_byte(void *self, uint8_t byte) {
    struct stdio_writer *w = self;
    fwrite(&byte, 1, 1, w->file);
}

void
stdio_write_bytes(void *self, uint8_t *bytes, size_t count) {
    struct stdio_writer *w = self;
    fwrite(bytes, 1, count, w->file);
}

struct serializer*
get_stdio_writer(const char *path) {
    struct stdio_writer *writer = eng_zalloc(sizeof(*writer));

    writer->file = fopen(path, "w");
    if(!writer->file) {
        eng_free(writer, sizeof(*writer));
        return NULL;
    }

    writer->serial.write_byte = stdio_write_byte;
    writer->serial.write_bytes = stdio_write_bytes;
    
    return &writer->serial;
}

void
close_stdio_write(struct serializer *s) {
    if(!s) return;

    struct stdio_writer *writer = (struct stdio_writer*)s;

    fclose(writer->file);
    writer->file = NULL;

    eng_free(writer, sizeof(*writer));
}