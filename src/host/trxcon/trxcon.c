/* 
 * OsmocomBB <-> OsmoTRX connection bridge
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
#include <getopt.h>
#include <stdlib.h>

#include <osmocom/core/logging.h>
#include <osmocom/core/signal.h>

#define COPYRIGHT \
	"Copyright (C) 2016 by Vadim Yanitskiy <axilirator@gmail.com>\n" \
	"License GPLv2+: GNU GPL version 2 or later " \
		"<http://gnu.org/licenses/gpl.html>\n" \
	"This is free software: you are free to change and redistribute it.\n" \
	"There is NO WARRANTY, to the extent permitted by law.\n\n"

static struct {
	int debug_set;
	int daemonize;

	const char *trx_ip;
	const char *bind_socket;
} app_data;

static void print_usage(const char *app)
{
	printf("Usage: %s\n", app);
}

static void print_help()
{
	printf(" Some help...\n");
	printf("  -h --help         this text\n");
	printf("  -i --trx-ip       The IP address of host runing OsmoTRX\n");
	printf("  -s --bind-socket  L2 socket (default /tmp/osmocom_l2)\n");
	printf("  -D --daemonize    Run as daemon\n");
}

static void handle_options(int argc, char **argv)
{
	while (1) {
		int option_index = 0, c;
		static struct option long_options[] = {
			{"help", 0, 0, 'h'},
			{"trx-ip", 1, 0, 'i'},
			{"daemonize", 0, 0, 'D'},
			{"bind-socket", 0, 0, 's'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "i:s:Dh",
				long_options, &option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			print_usage(argv[0]);
			print_help();
			exit(0);
			break;
		case 'i':
			app_data.trx_ip = optarg;
			break;
		case 's':
			app_data.bind_socket = optarg;
			break;
		case 'D':
			app_data.daemonize = 1;
			break;
		default:
			break;
		}
	}
}

int main(int argc, char **argv)
{
	printf("%s", COPYRIGHT);

	// TODO: we need init app_data before calling handle_options()
	handle_options(argc, argv);

	// Currently nothing to do
	print_usage(argv[0]);
	print_help();

	return 0;
}
