#ifndef ENGINE_SERIALIZE_H
#define ENGINE_SERIALIZE_H

#include "engine/types.h"

struct serializer {
    void (*write_byte)(void *self, uint8_t value);
    void (*write_bytes)(void *self, uint8_t *values, size_t count);
};

struct deserializer {
    void (*read_byte)(void *self, uint8_t value);
    void (*read_bytes)(void *self, uint8_t *values, size_t count);
};

struct serializer* get_stdio_writer(const char *path);
void close_stdio_write(struct serializer *s);

void write_u8(struct serializer *s, uint8_t value);
void write_u16(struct serializer *s, uint16_t value);
void write_u32(struct serializer *s, uint32_t value);
void write_u64(struct serializer *s, uint64_t value);

void write_i8(struct serializer *s, int8_t value);
void write_i16(struct serializer *s, int16_t value);
void write_i32(struct serializer *s, int32_t value);
void write_i64(struct serializer *s, int64_t value);

void write_float(struct serializer *s, float value);
void write_double(struct serializer *s, double value);

void write_u8_array(struct serializer *s, size_t length, uint8_t *values);
void write_u16_array(struct serializer *s, size_t length, uint16_t *values);
void write_u32_array(struct serializer *s, size_t length, uint32_t *values);
void write_u64_array(struct serializer *s, size_t length, uint64_t *values);

void write_i8_array(struct serializer *s, size_t length, int8_t *values);
void write_i16_array(struct serializer *s, size_t length, int16_t *values);
void write_i32_array(struct serializer *s, size_t length, int32_t *values);
void write_i64_array(struct serializer *s, size_t length, int64_t *values);

void write_float_array(struct serializer *s, size_t length, float *values);
void write_double_array(struct serializer *s, size_t length, double *values);

void write_array(struct serializer *s, size_t length, void *array, size_t stride, void (*write_element)(struct serializer*, void*));

#endif