/* 
 * OsmocomBB <-> OsmoTRecvconnection bridge
 * GSM L1 control interface
 *
 * (C) 2014 by Sylvain Munaut <tnt@246tNt.com>
 * (C) 2016 by Vadim Yanitskiy <axilirator@gmail.com>
 *
 * All Rights Reserved
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <arpa/inet.h>

#include <osmocom/core/msgb.h>
#include <osmocom/core/talloc.h>
#include <osmocom/core/select.h>

#include <osmocom/gsm/gsm_utils.h>

#include "trxcon.h"
#include "logging.h"
#include "l1ctl_proto.h"

extern void *tall_trx_ctx;
extern struct app_data_t app;

/* ------------------------------------------------------------------------ */
/* L1CTL exported API                                                       */
/* ------------------------------------------------------------------------ */

static struct msgb *l1ctl_alloc(uint8_t msg_type)
{
	struct l1ctl_hdr *l1h;
	struct msgb *msg = msgb_alloc_headroom(256, 4, "osmo_l1");

	if (!msg) {
		LOGP(DL1C, LOGL_ERROR, "Failed to allocate memory\n");
		return NULL;
	}

	msg->l1h = msgb_put(msg, sizeof(*l1h));
	l1h = (struct l1ctl_hdr *) msg->l1h;
	l1h->msg_type = msg_type;

	return msg;
}

int l1ctl_tx_pm_conf(uint16_t band_arfcn, int dbm, int last)
{
	struct msgb *msg;
	struct l1ctl_pm_conf *pmc;

	msg = l1ctl_alloc(L1CTL_PM_CONF);
	if (!msg)
		return -ENOMEM;

	LOGP(DL1C, LOGL_DEBUG, "Send PM Conf (%s %d = %d dBm)\n",
		gsm_band_name(gsm_arfcn2band(band_arfcn)),
		band_arfcn &~ ARFCN_FLAG_MASK, dbm);

	pmc = (struct l1ctl_pm_conf *) msgb_put(msg, sizeof(*pmc));
	pmc->band_arfcn = htons(band_arfcn);
	pmc->pm[0] = dbm2rxlev(dbm);
	pmc->pm[1] = 0;

	if (last) {
		struct l1ctl_hdr *l1h = (struct l1ctl_hdr *) msg->l1h;
		l1h->flags |= L1CTL_F_DONE;
	}

	return l1ctl_link_send(app.l1l, msg);
}

int l1ctl_tx_reset_ind(uint8_t type)
{
	struct msgb *msg;
	struct l1ctl_reset *res;

	msg = l1ctl_alloc(L1CTL_RESET_IND);
	if (!msg)
		return -ENOMEM;

	LOGP(DL1C, LOGL_DEBUG, "Send Reset Ind (%u)\n", type);

	res = (struct l1ctl_reset *) msgb_put(msg, sizeof(*res));
	res->type = type;

	return l1ctl_link_send(app.l1l, msg);
}

int l1ctl_tx_reset_conf(uint8_t type)
{
	struct msgb *msg;
	struct l1ctl_reset *res;

	msg = l1ctl_alloc(L1CTL_RESET_CONF);
	if (!msg)
		return -ENOMEM;

	LOGP(DL1C, LOGL_DEBUG, "Send Reset Conf (%u)\n", type);
	res = (struct l1ctl_reset *) msgb_put(msg, sizeof(*res));
	res->type = type;

	return l1ctl_link_send(app.l1l, msg);
}

/* ------------------------------------------------------------------------ */
/* L1CTL receive handling                                                   */
/* ------------------------------------------------------------------------ */

static int l1ctl_rx_fbsb_req(struct msgb *msg)
{
	struct l1ctl_fbsb_req *fbsb;
	uint16_t band_arfcn;
	int rc = 0;

	// Grab message
	fbsb = (struct l1ctl_fbsb_req *) msg->l1h;

	if (msgb_l1len(msg) < sizeof(*fbsb)) {
		LOGP(DL1C, LOGL_ERROR, "MSG too short FBSB Req: %u\n",
			msgb_l1len(msg));
		rc = -EINVAL;
		goto exit;
	}

	band_arfcn = ntohs(fbsb->band_arfcn);

	LOGP(DL1C, LOGL_DEBUG, "Recv FBSB Req (%s %d)\n",
		gsm_band_name(gsm_arfcn2band(band_arfcn)),
		band_arfcn &~ ARFCN_FLAG_MASK);

	// Setup TRX
	rc += trx_if_cmd_txtune(app.trx, band_arfcn);
	rc += trx_if_cmd_rxtune(app.trx, band_arfcn);
	rc += trx_if_cmd_poweron(app.trx);
	rc += trx_if_cmd_sync(app.trx);

exit:
	msgb_free(msg);
	return rc;
}

static int l1ctl_rx_pm_req(struct msgb *msg)
{
	uint16_t arfcn_start, arfcn_stop, arfcn;
	struct l1ctl_pm_req *pmr;
	int rc = 0;

	// Grab message
	pmr = (struct l1ctl_pm_req *) msg->l1h;

	if (msgb_l1len(msg) < sizeof(*pmr)) {
		LOGP(DL1C, LOGL_ERROR, "MSG too short PM Req: %u\n",
			msgb_l1len(msg));
		rc = -EINVAL;
		goto exit;
	}

	arfcn_start = ntohs(pmr->range.band_arfcn_from);
	arfcn_stop  = ntohs(pmr->range.band_arfcn_to);

	LOGP(DL1C, LOGL_DEBUG, "Recv PM Req (%s: %d -> %d)\n",
		gsm_band_name(gsm_arfcn2band(arfcn_start)),
		arfcn_start &~ ARFCN_FLAG_MASK,
		arfcn_stop &~ ARFCN_FLAG_MASK);

	// FIXME: WTF? Why?
	// Send fake responses
	for (arfcn = arfcn_start; arfcn <= arfcn_stop; arfcn++) {
		l1ctl_tx_pm_conf(arfcn, arfcn == 33 ?
			-60 : -120, arfcn == arfcn_stop);
	}

exit:
	msgb_free(msg);
	return rc;
}

static int l1ctl_rx_reset_req(struct msgb *msg)
{
	struct l1ctl_reset *res;
	int rc = 0;

	// Grab message
	res = (struct l1ctl_reset *) msg->l1h;

	if (msgb_l1len(msg) < sizeof(*res)) {
		LOGP(DL1C, LOGL_ERROR, "MSG too short Reset Req: %u\n",
			msgb_l1len(msg));
		rc = -EINVAL;
		goto exit;
	}

	LOGP(DL1C, LOGL_DEBUG, "Recv Reset Req (%u)\n", res->type);

	// Request power off
	rc += trx_if_cmd_poweroff(app.trx);

	// Simply confirm
	if (!rc)
		rc += l1ctl_tx_reset_conf(res->type);

exit:
	msgb_free(msg);
	return rc;
}

static int l1ctl_rx_echo_req(struct msgb *msg)
{
	LOGP(DL1C, LOGL_NOTICE, "Recv Echo Req\n");

	// Nothing to do, just send it back
	// FIXME: Message L1 header != Message Data
	return l1ctl_link_send(app.l1l, msg);
}

int l1ctl_rx_cb(struct msgb *msg)
{
	struct l1ctl_hdr *l1h;
	int rc = 0;

	l1h = (struct l1ctl_hdr *) msg->l1h;
	msg->l1h = l1h->data;

	switch (l1h->msg_type) {
	case L1CTL_FBSB_REQ:
		rc = l1ctl_rx_fbsb_req(msg);
		break;
	case L1CTL_PM_REQ:
		rc = l1ctl_rx_pm_req(msg);
		break;
	case L1CTL_RESET_REQ:
		rc = l1ctl_rx_reset_req(msg);
		break;
	case L1CTL_ECHO_REQ:
		rc = l1ctl_rx_echo_req(msg);
		break;
	default:
		LOGP(DL1C, LOGL_ERROR, "Unknown MSG: %u\n", l1h->msg_type);
		msgb_free(msg);
	}

	return rc;
}
