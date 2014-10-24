/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_SYNC_H_
#define _REFOS_SYNC_H_

/*! @file
    @brief Basic kernel synchronisation library.

    Basic mutex functionality using a kernel lock. Based on Anna Lyons' sync library. Implemented
    using a kernel async endpoint. Slow!
*/

typedef struct sync_mutex_* sync_mutex_t;

/*! @brief Create a mutex object.
    @return The created mutex object. (Gives ownership. Must call sync_destroy_mutex on given obj)
*/
sync_mutex_t sync_create_mutex();

/*! @brief Destroy a mutex object.
    @param mutex The mutex object to destroy. (Takes ownership)
*/
void sync_destroy_mutex(sync_mutex_t mutex);

/*! @brief Lock a mutex. May block current program.
    @param mutex The mutex to lock. (No ownership)
*/
void sync_acquire(sync_mutex_t mutex);

/*! @brief Release lock on a mutex.
    @param mutex The mutex to release. (No ownership)
*/
void sync_release(sync_mutex_t mutex);

/*! @brief Poll on a mutex.
    @param mutex The mutex to poll. (No ownership)
    @return True if mutex was acquired, false otherwise.
*/
int sync_try_acquire(sync_mutex_t mutex);

#endif /* _REFOS_SYNC_H_ */
