/* 
 * OsmocomBB <-> OsmoTRX connection bridge
 * GSM L2 socket (/tmp/osmocom_l2) handlers
 *
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
#include <string.h>
#include <assert.h>

#include <sys/un.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <osmocom/core/talloc.h>
#include <osmocom/core/select.h>
#include <osmocom/core/socket.h>
#include <osmocom/core/write_queue.h>

#include "logging.h"
#include "l1ctl_link.h"

extern void *tall_trx_ctx;

static int l1ctl_link_read_cb(struct osmo_fd *bfd)
{
	struct l1ctl_link *l1l = (struct l1ctl_link *) bfd->data;
	struct msgb *msg;
	uint16_t len;
	int rc;

	msg = msgb_alloc_headroom(L1CTL_LENGTH + L1CTL_HEADROOM,
		L1CTL_HEADROOM, "L1CTL");
	if (!msg) {
		LOGP(DL1C, LOGL_ERROR, "Failed to allocate msg!\n");
		return -ENOMEM;
	}

	rc = read(bfd->fd, &len, sizeof(len));
	if (rc < sizeof(len)) {
		LOGP(DL1C, LOGL_NOTICE, "L1CTL has lost connection\n");
		msgb_free(msg);
		if (rc >= 0)
			rc = -EIO;
		l1ctl_link_close_conn(l1l);
		return rc;
	}

	len = ntohs(len);
	if (len > L1CTL_LENGTH) {
		LOGP(DL1C, LOGL_ERROR, "Length is too big: %u\n", len);
		msgb_free(msg);
		return -EINVAL;
	}

	msg->l1h = msgb_put(msg, len);
	rc = read(bfd->fd, msg->l1h, msgb_l1len(msg));
	if (rc != len) {
		LOGP(DL1C, LOGL_ERROR, "Can not read data: len=%d rc=%d "
		     "errno=%d\n", len, rc, errno);
		msgb_free(msg);
		return rc;
	}

	// FIXME: struct l1ctl_link has no member 'cb'
	// return l1l->cb(l1l->cb_data, msg);
	LOGP(DL1C, LOGL_NOTICE, "RX DATA\n");
	msgb_free(msg);

	return 0;
}

static int l1ctl_link_write_cb(struct osmo_fd *bfd, struct msgb *msg)
{
	int rc;

	if (bfd->fd <= 0)
		return -EINVAL;

	rc = write(bfd->fd, msg->data, msg->len);
	if (rc != msg->len) {
		LOGP(DL1C, LOGL_ERROR, "Failed to write data: rc=%d\n", rc);
		return -1;
	}

	return 0;
}

/* Accept a new connection */
static int l1ctl_link_accept(struct osmo_fd *bfd, unsigned int flags)
{
	struct l1ctl_link *l1l = (struct l1ctl_link *) bfd->data;
	struct osmo_fd *conn_bfd = &l1l->wq.bfd;
	struct sockaddr_un un_addr;
	socklen_t len;
	int cfd;

	len = sizeof(un_addr);
	cfd = accept(bfd->fd, (struct sockaddr *) &un_addr, &len);
	if (cfd < 0) {
		LOGP(DL1C, LOGL_ERROR, "Failed to accept a new connection\n");
		return -1;
	}

	// Check if we already have an active connection
	if (conn_bfd->fd != -1) {
		LOGP(DL1C, LOGL_NOTICE, "A new connection rejected because "
			"we already have another active!\n");
		close(cfd);
		return 0;
	}

	osmo_wqueue_init(&l1l->wq, 100);
	INIT_LLIST_HEAD(&conn_bfd->list);

	l1l->wq.write_cb = l1ctl_link_write_cb;
	l1l->wq.read_cb = l1ctl_link_read_cb;
	conn_bfd->when = BSC_FD_READ;
	conn_bfd->data = l1l;
	conn_bfd->fd = cfd;

	if (osmo_fd_register(conn_bfd) != 0) {
		LOGP(DL1C, LOGL_ERROR, "Failed to register new connection fd\n");
		close(conn_bfd->fd);
		conn_bfd->fd = -1;
		return -1;
	}

	LOGP(DL1C, LOGL_NOTICE, "L1CTL has a new connection\n");

	// TODO: switch the bridge to CONNECTED state
	return 0;
}

int l1ctl_link_close_conn(struct l1ctl_link *l1l)
{
	struct osmo_fd *conn_bfd = &l1l->wq.bfd;

	if (conn_bfd->fd <= 0)
		return -EINVAL;

	// Close connection socket
	osmo_fd_unregister(conn_bfd);
	close(conn_bfd->fd);
	conn_bfd->fd = -1;

	// Clear pending messages
	osmo_wqueue_clear(&l1l->wq);

	// TODO: switch the bridge to IDLE state
	return 0;
}

int l1ctl_link_send(struct l1ctl_link *l1l, struct msgb *msg)
{
	uint16_t *len;

	// TMP: debug print
	printf("Sending: '%s'\n", osmo_hexdump(msg->data, msg->len));

	if (msg->l1h != msg->data)
		LOGP(DL1C, LOGL_NOTICE, "Message L1 header != Message Data\n");

	// Prepend 16-bit length before sending
	len = (uint16_t *) msgb_push(msg, sizeof(*len));
	*len = htons(msg->len - sizeof(*len));

	if (osmo_wqueue_enqueue(&l1l->wq, msg) != 0) {
		LOGP(DL1C, LOGL_ERROR, "Failed to enqueue msg!\n");
		msgb_free(msg);
		return -EIO;
	}

	return 0;
}

int l1ctl_link_init(struct l1ctl_link **l1l, const char *sock_path)
{
	struct l1ctl_link *l1l_new;
	struct osmo_fd *bfd;
	int rc;

	LOGP(DL1C, LOGL_ERROR, "Init L1CTL link (%s)\n", sock_path);

	// Try to allocate memory
	l1l_new = talloc_zero(tall_trx_ctx, struct l1ctl_link);
	if (!l1l_new) {
		fprintf(stderr, "Failed to allocate memory!\n");
		return -ENOMEM;
	}

	// Create a socket and bind handlers
	bfd = &l1l_new->listen_bfd;
	rc = osmo_sock_unix_init_ofd(bfd, SOCK_STREAM, 0, sock_path,
		OSMO_SOCK_F_BIND);
	if (rc < 0) {
		LOGP(DL1C, LOGL_ERROR, "Could not create UNIX socket: %s\n",
			strerror(errno));
		talloc_free(l1l_new);
		return rc;
	}

	bfd->cb = l1ctl_link_accept;
	bfd->when = BSC_FD_READ;
	bfd->data = l1l_new;

	// To be able to accept first connection and
	// drop others, it should be set to -1
	l1l_new->wq.bfd.fd = -1;
	*l1l = l1l_new;

	return 0;
}

void l1ctl_link_shutdown(struct l1ctl_link *l1l)
{
	struct osmo_fd *listen_bfd;

	LOGP(DL1C, LOGL_ERROR, "Shutdown L1CTL link\n");

	if (l1l != NULL) {
		listen_bfd = &l1l->listen_bfd;

		// Check if we have an established connection
		if (l1l->wq.bfd.fd != -1)
			l1ctl_link_close_conn(l1l);

		// Unbind listening socket
		if (listen_bfd->fd != -1) {
			osmo_fd_unregister(listen_bfd);
			close(listen_bfd->fd);
			listen_bfd->fd = -1;
		}

		// Free memory
		talloc_free(l1l);
	}
}
