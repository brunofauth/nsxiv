#pragma once

#include <stdint.h>
#include <stdbool.h>


typedef struct {
    int32_t start;
    int32_t end;
} IndexRange;

IndexRange IndexRange_widen(IndexRange range, const int32_t ammount);
bool IndexRange_contains(const IndexRange range, const int32_t index);

