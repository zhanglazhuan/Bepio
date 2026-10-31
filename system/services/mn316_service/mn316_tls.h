#ifndef BEPIO_MN316_TLS_H_
#define BEPIO_MN316_TLS_H_

typedef struct {
	const char *username;
	const char *password;
	int uart_baud; /* 0 selects the board default. */
} bepio_mn316_tls_credentials_t;

int bepio_mn316_tls_run(const bepio_mn316_tls_credentials_t *credentials, int seconds);
int bepio_mn316_tls_command(int argc, char **argv);

#endif
