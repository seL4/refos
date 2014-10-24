/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

#ifndef _REFOS_IO_MORECORE_H_
#define _REFOS_IO_MORECORE_H_

struct sl_procinfo_s;

void refosio_setup_morecore_override(char *mmapRegion, unsigned int mmapRegionSize);

void refosio_init_morecore(struct sl_procinfo_s *procInfo);

#endif /* _REFOS_IO_MORECORE_H_ */

