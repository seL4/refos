/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/*! @file
    @brief Ring buffer implementation, operating on the internal RAM dataspace objects.

    A simple <b>MCRingBuffer</b> implementation which operates on the internal RAM dataspaces. This
    is designed for communication with the clients who share to us our own dataspace. Since reading
    from a page entry is computationally expensive as it requires mapping the frame, copying the
    data out and then unmapping the frame, the MCRingBuffer algorithm is used here to minimise the
    frequency of read / writes to the shared control variables.
    
    Reference:

    > P.P.C. Lee, T. Bu,  and G.P. Chandranmenon,  A lock-free, cache-efficient shared ring buffer
    > for multi-core architectures.  ;In Proceedings of ANCS. 2009, 78-79. <br>
    > http://www.cse.cuhk.edu.hk/~pclee/www/pubs/ancs09poster.pdf
*/

#ifndef _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_RING_BUFFER_H_
#define _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_RING_BUFFER_H_

#include "../../common.h"
#include "dataspace.h"

#define RINGBUFFER_MAGIC 0x660149F7
#define RINGBUFFER_METADATA_SIZE (sizeof(uint32_t) * 2)

/*! @brief Ring buffer modes - read only or write only. */
enum rb_mode {
    RB_READONLY,
    RB_WRITEONLY
};

/*! @brief Ring buffer structure.

    A structure containing information regarding to a single ring buffer. Does NOT take ownership of
    the associated dataspace; merely provides a wrapping to use ringbuffers on top of a struct
    ram_dataspace. Ringbuffers are uni-directional; either read-only or write-only, as described in
    rb_modes_t.

    The structure also shares ownership of the underlying dataspace object, using a shared
    reference. This ensures that the ram_dspace structure pointed here is always valid, but also
    means this rb_buffer must be deleted for the linked ram_dspace to be truly deleted, as this
    object "hangs onto" its dataspace.
 */
struct rb_buffer {
    enum rb_mode mode;
    uint32_t magic;

    uint32_t localStart;
    uint32_t localEnd;
    uint32_t size;

    struct ram_dspace *dataspace; /* Shared ownership. */
};

/*! @brief Creates a ring buffer structure operating on the given dataspace.
    
    Creates a ring buffer structure operating on the given dataspace. This assumes a shared
    ownership of the given dataspace, and keeps a strong reference to it.

    @param dataspace The dataspace to wrap a ring buffer interface on. (Shared ownership)
    @param mode Either RB_READONLY or RB_WRITEONLY.
    @return An empty ring buffer on success (Passes ownership), NULL otherwise. 
 */
struct rb_buffer *rb_create(struct ram_dspace *dataspace, enum rb_mode mode);

/*! @brief Deletes given ring buffer structure.
    @param rb The ring buffer to delete. (Takes ownership)
 */
void rb_delete(struct rb_buffer *rb);

/*! @brief Writes data to ring buffer. (See code comments for more info).
    @param buf Destination ring buffer to write to. (No ownership)
    @param str Source buffer to read from in bytes. (No ownership)
    @param len Size of source buffer containing data.
    @return ESUCCESS if success, ENOMEM if ring buffer is full, refos_error otherwise.
 */
int rb_write(struct rb_buffer *buf, char *str, size_t len);

/*! @brief Reads data from ring buffer. (See code comments for more info).
    @param buf Source ring buffer to read from. (No ownership)
    @param dest Destination buffer to read to. (No ownership)
    @param len Maximum length of data to read in bytes.
    @param bytesRead How many bytes were read into the destination buffer. (No ownership)
    @return ESUCCESS if success, refos_error otherwise.
 */
int rb_read(struct rb_buffer *buf, char *dest, size_t len, size_t *bytesRead);

#endif /* _REFOS_PROCESS_SERVER_SYSTEM_MEMSERV_RING_BUFFER_H_ */