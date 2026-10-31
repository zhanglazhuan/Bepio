#ifndef BEPIO_MN316_H_
#define BEPIO_MN316_H_

/* Startup-only probe; call before any other consumer owns the modem UART.
 * Returns 0 on OK, -ETIMEDOUT on silence, -EIO on modem ERROR.
 */
int mn316_probe(void);

#endif
