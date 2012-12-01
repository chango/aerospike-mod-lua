#pragma once

#include "as_util.h"
#include "as_types.h"
#include "as_buffer.h"
#include <inttypes.h>

/******************************************************************************
 * TYPES
 ******************************************************************************/

typedef struct as_serializer_s as_serializer;
typedef struct as_serializer_hooks_s as_serializer_hooks;

struct as_serializer_s {
    const void * source;
    const as_serializer_hooks * hooks;
};

struct as_serializer_hooks_s {
    int (*free)(as_serializer *);
    int (*serialize)(as_serializer *, as_val *, as_buffer *);
    int (*deserialize)(as_serializer *, as_buffer *, as_val **);
};

/******************************************************************************
 * FUNCTIONS
 ******************************************************************************/

as_serializer * as_serializer_new(const void *, const as_serializer_hooks *);

/******************************************************************************
 * INLINE FUNCTIONS
 ******************************************************************************/

inline int as_serializer_init(as_serializer * s, const void * source, const as_serializer_hooks * hooks) {
    s->source = source;
    s->hooks = hooks;
    return 0;
}

inline void * as_serializer_source(as_serializer * s) {
    return (s ? (void *)s->source : NULL);
}

inline int as_serializer_free(as_serializer * s) {
    return as_util_hook(free, 1, s);
}

inline int as_serializer_serialize(as_serializer * s, as_val * v, as_buffer * b) {
    return as_util_hook(serialize, 1, s, v, b);
}

inline int as_serializer_deserialize(as_serializer * s, as_buffer * b, as_val ** v) {
    return as_util_hook(deserialize, 1, s, b, v);
}