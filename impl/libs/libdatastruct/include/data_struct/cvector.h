/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _CVECTOR_H_
#define _CVECTOR_H_

#define CVECTOR_INIT_SIZE 4
#define CVECTOR_RESIZE_CHECK_FREQ 16

#ifndef kmalloc
    #include <stdlib.h>
    #include <stdint.h>
    #define kmalloc malloc
    #define krealloc realloc
    #define kfree free
#endif

typedef void* cvector_item_t;

typedef struct cvector_s {
    cvector_item_t* data;
    size_t size;
    size_t count;
    int32_t nextResize;
} cvector_t;

void cvector_init(cvector_t *v);

int cvector_add(cvector_t *v, cvector_item_t e);

inline size_t cvector_count(cvector_t *v) {
    return v->count;
}

void cvector_set(cvector_t *v, uint32_t index, cvector_item_t e);

cvector_item_t cvector_get(cvector_t *v, int index);

void cvector_delete(cvector_t *v, int index);

void cvector_free(cvector_t *v);

void cvector_reset(cvector_t *v);

#endif /* _CVECTOR_H_ */
