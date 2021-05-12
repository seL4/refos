/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _REFOS_IO_MORECORE_H_
#define _REFOS_IO_MORECORE_H_

struct sl_procinfo_s;

void refosio_setup_morecore_override(char *mmapRegion, unsigned int mmapRegionSize);

void refosio_init_morecore(struct sl_procinfo_s *procInfo);

#endif /* _REFOS_IO_MORECORE_H_ */

