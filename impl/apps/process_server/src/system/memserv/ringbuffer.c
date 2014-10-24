/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#include "ringbuffer.h"
#include "../../common.h"
#include "dataspace.h"
#include <refos/refos.h>
#include <utils/arith.h>

/*! @file
    @brief Ring buffer implementation, operating on the internal RAM dataspace objects. */

/* -------------------------------- Ring Buffer helper functions -------------------------------- */

static int
rb_update_local_start(struct rb_buffer *buf)
{
    assert(buf && buf->dataspace);
    if (buf->mode == RB_WRITEONLY) {
        /* Read in local start. */
        return ram_dspace_read (
                ((char*) &buf->localStart), sizeof(uint32_t), buf->dataspace, 0
        );
    } else if (buf->mode == RB_READONLY) {
        /* Write out local start. */
        return ram_dspace_write (
                ((char*) &buf->localStart), sizeof(uint32_t), buf->dataspace, 0
        );
    }
    assert(!"Invalid ring buffer mode!");
    return EINVALID;
}

static int
rb_update_local_end(struct rb_buffer *buf)
{
    assert(buf && buf->dataspace);
    if (buf->mode == RB_WRITEONLY) {
        /* Write out local end. */
        return ram_dspace_write(
                ((char*) &buf->localEnd), sizeof(uint32_t), buf->dataspace, sizeof(uint32_t)
        );
    } else if (buf->mode == RB_READONLY) {
        /* Read in local end. */
        return ram_dspace_read(
                ((char*) &buf->localEnd), sizeof(uint32_t), buf->dataspace, sizeof(uint32_t)
        );
    }
    assert(!"Invalid ring buffer mode!");
    return EINVALID;
}

static uint32_t
rb_remaining_size(struct rb_buffer *buf)
{
    return buf->localStart > buf->localEnd ?
            ((buf->localStart - buf->localEnd) - 1) :
            ((buf->size - 1) - (buf->localEnd - buf->localStart));
}

/* ------------------------------ Ring Buffer interface functions ------------------------------- */

struct rb_buffer *
rb_create(struct ram_dspace *dataspace, enum rb_mode mode)
{
    assert(dataspace && dataspace->magic == RAM_DATASPACE_MAGIC);

    /* Allocate space for the structure. */
    struct rb_buffer *newRingBuffer = kmalloc(sizeof(struct rb_buffer));
    if (!newRingBuffer) {
        ROS_ERROR("ring buffer creation malloc failed.\n");
        return NULL;
    }

    /* Fill out the ring buffer structure. */
    newRingBuffer->mode = mode;
    newRingBuffer->localStart = 0;
    newRingBuffer->localEnd = 0;
    newRingBuffer->size = ram_dspace_get_size(dataspace) - RINGBUFFER_METADATA_SIZE;
    newRingBuffer->magic = RINGBUFFER_MAGIC;

    /* Reference the dataspace to own a share of it. */
    ram_dspace_ref(dataspace->parentList, dataspace->ID);
    newRingBuffer->dataspace = dataspace;

    return newRingBuffer;
}

void
rb_delete(struct rb_buffer *rb)
{
    assert(rb && rb->magic == RINGBUFFER_MAGIC);
    ram_dspace_unref(rb->dataspace->parentList, rb->dataspace->ID);
    rb->magic = 0;
    kfree(rb);
}

int
rb_write(struct rb_buffer *buf, char *str, size_t len)
{
    int error;
    assert(buf && buf->magic == RINGBUFFER_MAGIC);

    if (buf->mode != RB_WRITEONLY) {
        ROS_WARNING("tried to write to a read-only ring buffer.\n");
        return EACCESSDENIED;
    }
    
    if (len > rb_remaining_size(buf)) {
        /* We have run out of space according to our local copy of start,
           and our local copy may be out of date, so we update it and try again. */
        error = rb_update_local_start(buf);
        if (error) {
            ROS_ERROR("could not read start value from shared buffer.\n");
            return EINVALID;
        }
        if (len > rb_remaining_size(buf)) {
            /* We have just updated our local start, so data is definitely too big. */
            return ENOMEM;
        }
    }
    
    if (buf->localEnd < buf->localStart) {
        /* Non-wrapping case, copy block of data straight in.

                 | ✕ ✕ █ █ █ █ █ █ █ █ █ █ █ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ _ _ _ _ _ _|
                       ◀―――――― Filled ―――――▶ ◀―――――― Dest ―――――▶ ◀― Empty ―▶
        */
        error = ram_dspace_write(str, len, buf->dataspace,
                                      buf->localEnd + RINGBUFFER_METADATA_SIZE);
        if (error) {
            ROS_ERROR("could not write to dataspace.\n");
            return error;
        }
    } else {
        /* Wrapping case - copy the end bit first.

            | ✕ ✕ _ _ _ _ _ _ _ _ _ _ _ _ _ █ █ █ █ █ █ █ █ █ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹|
                                            ◀―――― Filled ―――▶ ◀――― End bit ―――――▶ 
        */
        uint32_t endBytes = MIN(len, buf->size - buf->localEnd);
        error = ram_dspace_write(str, endBytes, buf->dataspace, 
                                      buf->localEnd + RINGBUFFER_METADATA_SIZE);
        if (error) {
            ROS_ERROR("could not write to dataspace.\n");
            return error;
        }
        assert(endBytes <= len);

        /* Copy the wrapped start bit if it exists.

            | ✕ ✕ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ _ _ _ _ █ █ █ █ █ █ █ █ █ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹|
                  ◀―― Start bit ――▶         ◀―――― Filled ―――▶ 
        */
        if (endBytes < len) {
            error = ram_dspace_write(str + endBytes, len - endBytes, buf->dataspace, 
                                          0 + RINGBUFFER_METADATA_SIZE);
            if (error) {
                ROS_ERROR("could not write to dataspace.\n");
                return error;
            }
        }
    }
    
    /* Update end. */
    buf->localEnd = (buf->localEnd + len) % buf->size;
    error = rb_update_local_end(buf);
    if (error) {
        ROS_ERROR("could not write end value to shared buffer.\n");
        return error;
    }
    
    return ESUCCESS;
}

int
rb_read(struct rb_buffer *buf, char *dest, size_t len, size_t *bytesRead)
{
    int error;
    assert(buf && buf->magic == RINGBUFFER_MAGIC);
    
    if (buf->mode != RB_READONLY) {
        ROS_WARNING("tried to read from a write-only ring buffer.\n");
        return EACCESSDENIED;
    }
    
    size_t contentSize = (buf->size - 1) - rb_remaining_size(buf);
    if (len > contentSize) {
        /* We don't have enough data to read according to our local copy of end,
           and our local copy may be out of date, so we update it and try again. */
        error = rb_update_local_end(buf);
        if (error) {
            ROS_ERROR("could not read end value from shared buffer.\n");
            return error;
        }
    }
    
    if (buf->localStart <= buf->localEnd) {
        /* Non-wrapping case, copy block of data straight out.

                 | ✕ ✕ █ █ █ █ █ █ █ █ █ █ █ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ _ _ _ _ _ _|
                       ◀―――――― Filled ―――――▶ ◀―――――― Src ――――――▶ ◀― Empty ―▶
        */
        *bytesRead = MIN(buf->localEnd - buf->localStart, len);
        error = ram_dspace_read(dest, *bytesRead, buf->dataspace,
                                     buf->localStart + RINGBUFFER_METADATA_SIZE);
        if (error) {
            ROS_ERROR("could not read from dataspace.\n");
            return error;
        }
    } else {
        /* Wrapping case - read the end bit out first.

            | ✕ ✕ _ _ _ _ _ _ _ _ _ _ _ _ _ █ █ █ █ █ █ █ █ █ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹|
                                            ◀―――― Filled ―――▶ ◀――― End bit ―――――▶ 
        */
        *bytesRead = MIN(buf->size - buf->localStart, len);
        error = ram_dspace_read(dest, *bytesRead, buf->dataspace,
                                     buf->localStart + RINGBUFFER_METADATA_SIZE);
        if (error) {
            ROS_ERROR("could not read from dataspace.\n");
            return error;
        }
        unsigned int bytesRemain = MIN(len - *bytesRead, buf->localEnd);

        /* Read the wrapped start bit if it exists. 

            | ✕ ✕ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ _ _ _ _ █ █ █ █ █ █ █ █ █ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹ ▹|
                  ◀―― Start bit ――▶         ◀―――― Filled ―――▶ 
        */
        if (bytesRemain > 0) {
            error = ram_dspace_read(dest + *bytesRead, bytesRemain, buf->dataspace,
                                         0 + RINGBUFFER_METADATA_SIZE);
            if (error) {
                ROS_ERROR("could not read from dataspace.\n");
                return error;
            }
            *bytesRead += bytesRemain;
        }
    }
    
    /* Update start. */
    buf->localEnd = (buf->localEnd + len) % buf->size;
    buf->localStart = (buf->localStart + *bytesRead) % buf->size;
    error = rb_update_local_start(buf);
    if (error) {
        ROS_ERROR("could not write start value to shared buffer.\n");
        return error;
    }
    
    return ESUCCESS;
}