#ifndef _TRXCON_H
#define _TRXCON_H

#include "trx_if.h"
#include "l1ctl_link.h"

struct app_data_t {
	struct trx_instance *trx;
	struct l1ctl_link *l1l;

	const char *l1l_socket;
	const char *trx_ip;
	uint16_t trx_port;

	int daemonize;
	int quit;
};

#endif /* _TRXCON_H */
