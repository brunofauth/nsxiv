#include "range.h"


inline IndexRange IndexRange_widen(IndexRange range, const int32_t ammount) {
    return (IndexRange){
        .start = range.start < ammount ? 0 : range.start - ammount,
        .end = INT32_MAX - ammount < range.end ? INT32_MAX : range.end + ammount,
    };
}


inline bool IndexRange_contains(const IndexRange range, const int32_t index) {
    return index >= range.start && index < range.end;
}
