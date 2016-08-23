#ifndef _TRXCON_H
#define _TRXCON_H

#include "trx_if.h"
#include "l1ctl_link.h"
#include "l1ctl.h"

enum app_state_t {
	APP_STATE_IDLE,      // There is no L1CTL connection, wait for a new one
	APP_STATE_MANAGED    // We have an active L1CTL connection
};

enum app_event_t {
	APP_EVENT_L1C_CONNECT,
	APP_EVENT_L1C_DISCONNECT,
	APP_EVENT_TRX_DISCONNECT,
	APP_EVENT_TRX_RESP_ERROR
};

struct app_data_t {
	struct trx_instance *trx;
	struct l1ctl_link *l1l;

	const char *debug_mask;
	const char *l1l_socket;
	const char *trx_ip;
	uint16_t trx_port;

	// Current application state
	enum app_state_t state;

	int daemonize;
	int quit;
};

void app_handle_event(enum app_event_t event);

#endif /* _TRXCON_H */
