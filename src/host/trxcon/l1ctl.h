#ifndef _L1CTL_H
#define _L1CTL_H

int l1ctl_tx_pm_conf(uint16_t band_arfcn, int dbm, int last);
int l1ctl_tx_reset_conf(uint8_t type);
int l1ctl_tx_reset_ind(uint8_t type);
int l1ctl_rx_cb(struct msgb *msg);

#endif /* _L1CTL_H */
