/*
  Extension of Semtech Semtech-Cycleo Packet Forwarder.
  (C) 2015 Beta Research BV

Description:
	Virtualization of nodes.

License: Revised BSD License, see LICENSE.TXT file include in the project
Maintainer: Ruud Vlaming
*/


#ifndef INC_VIRTUAL_PKT_FWD_H_
#define INC_VIRTUAL_PKT_FWD_H_


/* -------------------------------------------------------------------------- */
/* --- MAC OSX Extensions  -------------------------------------------------- */

#ifdef __MACH__
#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 0
int clock_gettime(int clk_id, struct timespec* t);
#endif

double difftimespec(struct timespec end, struct timespec beginning);

#endif /* INC_VIRTUAL_PKT_FWD_H_ */
