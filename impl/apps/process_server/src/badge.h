/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_PROCESS_SERVER_BADGE_H_
#define _REFOS_PROCESS_SERVER_BADGE_H_

#include <refos/refos.h>

/*! @file
    @brief Process Server badge space definitions.
*/

/* ---- BadgeID 3 to 4098 : ASID & Liveness caps ---- */

#define PID_MAX PROCSERV_MAX_PROCESSES
#define PID_INVALID (ASID_MAX_ID + 1)

#define PID_BADGE_BASE 0x3
#define PID_BADGE_END (PID_BADGE_BASE + PID_MAX)
#define PID_LIVENESS_BADGE_BASE PID_BADGE_END
#define PID_LIVENESS_BADGE_END (PID_LIVENESS_BADGE_BASE + PID_MAX)

/* ---- BadgeID 4099 to 12290 : Windows ---- */

#define W_MAX_WINDOWS 8192
#define W_MAX_ASSOCIATED_WINDOWS 2048

#define W_BADGE_BASE PID_LIVENESS_BADGE_END
#define W_BADGE_END (W_BADGE_BASE + W_MAX_WINDOWS)

/* ---- BadgeID 12291 to 20482 : RAM Dataspace Objects ---- */

#define RAM_DATASPACE_MAX_NUM_DATASPACE 8192
#define RAM_DATASPACE_BADGE_BASE W_BADGE_END
#define RAM_DATASPACE_BADGE_END (RAM_DATASPACE_BADGE_BASE + RAM_DATASPACE_MAX_NUM_DATASPACE)

#endif /* _REFOS_PROCESS_SERVER_BADGE_H_ */
