#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstdint>
#include <cstring>

#include <unistd.h>

//#include <client.h>
//#include <server.h>

#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>

#include <errno.h>

int main(int argc, char** argv) {
	int is_client = 0, is_server = 0, port_is_set = 0, parsing_errors = 0, help_displayed = 0;

	struct sockaddr_in client_address, server_address;
	struct hostent* host_ptr;

	socklen_t client_size;

	bzero((char*)&client_address, sizeof(client_address));
	bzero((char*)&server_address, sizeof(server_address));

	if(argc <= 0) {
		return EXIT_FAILURE;
	}

	if(argc == 1) {
		fprintf(stderr, "%s: Error: No parameters given\nUse -h or --help for help using this program.\n", argv[0]);
		return EXIT_FAILURE;
	}

	if(argc >= 2) {
		for(int i = 1; i < argc; i++) {
			if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
				fprintf(stderr,
					"Usage: %s [parameters] [port] ([IPv4 Address], if running as client)\nParameters:\n"
					"  -h, --help      Display program usage and parameters\n"
					"  -c, --client    Run in client mode (connects to a server)\n"
					"  -s, --server    Run in server mode (waits for a client)\n",
				argv[0]);

				help_displayed = 1;
			}
		}

		for(int i = 1; i < argc; i++) {
			if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
				continue;
			}

			if(strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--client") == 0) {
				is_client = true;
				continue;
			}

			if(strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
				is_server = true;
				continue;
			}

			if(is_client && !port_is_set) {
				if(i >= (argc-1)) {
					fprintf(stderr, "Error: Must specify a host when running as client (-c/--client)!\n", argv[i]);
					parsing_errors++;
					break;
				}

				char* end;
				long port_number = strtol(argv[i], &end, 10);

				if(argv[i] == end) {
					fprintf(stderr, "Error: \'%s\' is not a valid port number!\n", argv[i]);
					parsing_errors++;
				}


				port_is_set = 1;

				host_ptr = gethostbyname(argv[i+1]);

				if(host_ptr == NULL) {
					fprintf(stderr, "Error: Could not resolve host \'%s\'\n", argv[i+1]);
					parsing_errors++;
				}

				if(parsing_errors != 0) continue;

				server_address.sin_port = htons((uint64_t)port_number);

				break;
			}

			if(is_server && !port_is_set) {
				char* end;
				long port_number = strtol(argv[i], &end, 10);

				if(argv[i] == end) {
					fprintf(stderr, "Error: \'%s\' is not a valid port number!\n", argv[i]);
					parsing_errors++;
				}

				port_is_set = 1;

				if(parsing_errors != 0) continue;

				client_address.sin_port = htons((uint64_t)port_number);

				continue;
			}

			fprintf(stderr, "%s: Error: Unknown Parameter \'%s\'\n", argv[0], argv[i]);

			parsing_errors++;
		}

		if(!(is_client || is_server)) {
			fprintf(stderr, "%s: Error: Must specify client (-c/--client) or server (-s/--server)!\n", argv[0]);
			parsing_errors++;
		}

		if(is_client && is_server) {
			fprintf(stderr, "%s: Error: Cannot specify client (-c/--client) and server (-s/--server)!\n", argv[0]);
			parsing_errors++;
		}

		if(!port_is_set) {
			fprintf(stderr, "%s: Error: Must specify a port!\n", argv[0]);
			parsing_errors++;
		}

		if(parsing_errors != 0) {
			fprintf(stderr, "Use -h or --help for help using this program.\nFound %d errors while parsing arguments.\n", parsing_errors);

			return EXIT_FAILURE;
		}

		if(help_displayed) {
			return EXIT_SUCCESS;
		}
	}

	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	if(fd == 0) {
		perror("Error opening socket");
		return EXIT_FAILURE;
	}

	char buf[1024];

	memset(buf, 0, 1024);

	if(is_client) {
		
		server_address.sin_family = AF_INET;
		//server_address.sin_port = htons(port_number);
		server_address.sin_addr.s_addr = INADDR_ANY;

		bcopy((char*)host_ptr->h_addr, (char*)&server_address.sin_addr, host_ptr->h_length);

		while(true) {
			struct timespec wake_ts;

			clock_gettime(CLOCK_REALTIME, &wake_ts);

			wake_ts.tv_sec = 0;
			wake_ts.tv_nsec = 1000000000l-wake_ts.tv_nsec;

			if(nanosleep(&wake_ts, NULL) == -1) perror("Error: nanosleep() returned -1");

			clock_gettime(CLOCK_REALTIME, &wake_ts);

			snprintf(buf, (size_t)1024, "I am the client. It is currently %lld.%09lld.", (long long int)wake_ts.tv_sec, (long long int)wake_ts.tv_nsec);

			long long int bytes_sent = sendto(fd, buf, strlen(buf), 0, (struct sockaddr*)&server_address, sizeof(server_address));

			//printf("packet sent");

			if(bytes_sent != strlen(buf)) {
				fprintf(stderr, "Error: sendto() returned %lld", bytes_sent);
				perror("");
			}

			memset(buf, 0, 1024);
		}
	}

	if(is_server) {
		client_address.sin_family = AF_INET;
		//client_address.sin_port = htons(port_number);
		client_address.sin_addr.s_addr = INADDR_ANY;

		if(bind(fd, (struct sockaddr*)&client_address, sizeof(client_address)) != 0) {
			perror("Error binding socket");
			return EXIT_FAILURE;
		}

		while(true) {
			memset(buf, 0, 1024);

			socklen_t client_addr_size = sizeof(client_address);

			long long int bytes_read = recvfrom(fd, buf, (size_t)1024, 0, (struct sockaddr*)&client_address, &client_addr_size);

			if(bytes_read < 0) {
				fprintf(stderr, "Error: recv() returned %lld", bytes_read);
				perror("");
			}

			fprintf(stderr, "Recieved packet: \'");

			printf(buf);

			printf("\'\n");

			fflush(stdout);
		}
		/*


		std::size_t from_len = sizeof(struct sockaddr_in);

		while(true) {
			int n = recvfrom(fd, buf, 1024, 0, (struct sockaddr*)&client_address, &client_size);

			if(n < 0) {
				perror("Error recvfrom");
				//return EXIT_FAILURE;
			}

			write(1, "recieved a datagram: ", 21);
			write(1, buf, n);
			n = sendto(fd, "Got your message\n", 17, 0, (struct sockaddr*)&client_address, client_size);

			if(n < 0) {
				perror("Error sendto");
				//return EXIT_FAILURE;
			}
		}
		*/
	}

	return EXIT_SUCCESS;
}
