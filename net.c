#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>
#include <time.h>
#include <argp.h>
#include <netdb.h>
#include <stdlib.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <asm/env.h>
#include <asm/eth.h>

const char *argp_program_version = "0.1";
const char *argp_program_bug_address = "tavi@cs.pub.ro";
static char doc[] = "";
static char args_doc[] = "URL";
static struct argp_option options[] = {
    {"interface", 'i', "string", 0, "native interface to use"},
    {"mac", 'm', "mac address", 0, "MAC address for the lkl interface"},
    {"address", 'a', "IPv4 address", 0, "IPv4 address for the lkl interface"},
    {"netmask-length", 'n', "int", 0, "IPv4 netmask length for the lkl interface"},
    {"gateway", 'g', "IPv4 address", 0, "IPv4 gateway for lkl"},
    {"lkl", 'l', 0, 0, "Use LKL"},
    {0},
};

typedef struct cl_args_ {
	struct ether_addr *mac;
	const char *iface, *request;
	struct in_addr address, gateway, host;
	unsigned int netmask_len, port, lkl;
} cl_args_t;

cl_args_t cla = {
	.port = 80,
};


static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
	cl_args_t *cla = state->input;
	switch (key) {
	case 'm':
	{
		cla->mac=ether_aton(arg);
		if (!cla->mac) {
			printf("bad MAC address: %s\n", arg);
			return -1;
		}
		break;
	}
	case 'a':
	{
		struct hostent *hostinfo =gethostbyname(arg);
		if (!hostinfo) {
			printf("unknown host %s\n", arg);
			return -1;
		}
		cla->address=*(struct in_addr*)hostinfo->h_addr;
		break;
	}
	case 'g':
	{
		struct hostent *hostinfo =gethostbyname(arg);
		if (!hostinfo) {
			printf("unknown host %s\n", arg);
			return -1;
		}
		cla->gateway=*(struct in_addr*)hostinfo->h_addr;
		break;
	}
	case 'n':
	{
		cla->netmask_len=atoi(arg);
		if (cla->netmask_len <= 0 || cla->netmask_len >=31) {
			printf("bad netmask length %d\n", cla->netmask_len);
			return -1;
		}
		break;
	}

	case 'i':
	{
		if (if_nametoindex(arg) < 0) {
			printf("invalid interface: %s\n", arg);
			return -1;
		}
		cla->iface=arg;
		break;
	}
	case 'l':
	{
		cla->lkl=1;
		break;
	}
	case ARGP_KEY_ARG:
	{
		char *host, *request, *port;
		struct hostent *hostinfo;

		//URL: http://host:port/request 
		if (strncasecmp("http://", arg, sizeof("http://")-1) != 0) {
			printf("bad url: %s\n", arg);
			return -1;
		}

		host=arg+sizeof("http://")-1;
		request=strchr(host, '/');
		port=strchr(host, ':');

		if (port && request && port < request) {
			char tmp[request-port];
			memcpy(tmp, port+1, sizeof(tmp));
			tmp[sizeof(tmp)]=0;
			cla->port=atoi(tmp);
		}

		if (!request)
			cla->request="";
		else
			cla->request=request+1;

		if (port)
			*port=0;
		else if (request)
			*request=0;

		hostinfo = gethostbyname(host);
		if (!hostinfo) {
			printf("unknown host %s\n", host);
			return -1;
		}
		cla->host=*(struct in_addr*)hostinfo->h_addr;
		break;
	}
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}
static struct argp argp = { options, parse_opt, args_doc, doc };


int do_connect(int fd, const struct sockaddr *saddr, socklen_t slen)
{
	if (cla.lkl)
		return lkl_sys_connect(fd, (struct sockaddr*)saddr, slen);
	else
		return connect(fd, saddr, slen);
}

int do_socket(int family, int type, int protocol)
{
	if (cla.lkl)
		return lkl_sys_socket(family, type, protocol);
	else
		return socket(family, type, protocol);
}

ssize_t do_read(int fd, void *buf, size_t len)
{
	if (cla.lkl)
		return lkl_sys_read(fd, buf, len);
	else
		return read(fd, buf, len);
}

ssize_t do_write(int fd, const void *buf, size_t len)
{
	if (cla.lkl)
		return lkl_sys_write(fd, buf, len);
	else
		return write(fd, buf, len);
}

#define get_error(err) (cla.lkl?-err:errno)

static int do_full_write(int fd, const char *buffer, int length)
{
	int n, todo=length;

	while (todo && (n=do_write(fd, buffer, todo)) >=  0) {
		todo-=n;
		buffer+=n;
	}

	if (!todo)
		return length;
	else
		return n;
}

int main(int argc, char **argv)
{
	int err, sock;
	struct sockaddr_in saddr = {
		.sin_family = AF_INET,
	};
	char req[1024], buffer[4096];

	if (argp_parse(&argp, argc, argv, 0, 0, &cla) < 0)
		return -1;

	if (!cla.host.s_addr) {
		printf("no url specified!\n");
		return -1;
	}
		
	if (cla.lkl) {
		int ifindex, sock;
		struct ifreq ifr;


		if (!cla.iface || !cla.mac || !cla.netmask_len ||
		    !cla.address.s_addr || !cla.gateway.s_addr) {
			printf("lkl mode and no interface, mac, address, netmask length or gateway specified!\n");
			return -1;
		}

		strncpy(ifr.ifr_name, cla.iface, IFNAMSIZ);
		sock = socket(PF_INET, SOCK_DGRAM, 0);
		if (sock < 0) {
			fprintf(stderr, "failed to create DGRAM INET socket: %s\n", strerror(errno));
			return -1;
		}

		err = ioctl(sock, SIOCGIFFLAGS, (long)&ifr);
		if (err) {
			fprintf(stderr, "failed to get flags on %s: %s\n", cla.iface, strerror(errno));
			return -1;
		}
		ifr.ifr_flags |= IFF_PROMISC;
		err = ioctl(sock, SIOCSIFFLAGS, (long)&ifr);
		if (err) {
			fprintf(stderr, "failed to set %s in promisc mode\n", cla.iface);
			return -1;
		}

		if (lkl_env_init(256*1024*1024) < 0)
			return -1;
		
		if ((ifindex=lkl_add_eth(cla.iface, (char*)cla.mac, 32)) == 0) 
			return -1;
			
		if ((err=lkl_if_set_ipv4(ifindex, cla.address.s_addr,
					 cla.netmask_len)) < 0) {
			printf("failed to set IP: %s/%d: %s\n",
			       inet_ntoa(cla.address), cla.netmask_len,
					 strerror(-err));
			return -1;
		}

		if ((err=lkl_if_up(ifindex)) < 0) {
			printf("failed to bring interface up: %s\n", strerror(-err));
			return -1;
		}

		if ((err=lkl_set_gateway(cla.gateway.s_addr))) {
			printf("failed to set gateway %s: %s\n",
			       inet_ntoa(cla.gateway), strerror(-err));
			return -1;
		}
	}
	
	
	if ((sock=do_socket(PF_INET, SOCK_STREAM, 0)) < 0) {
		printf("can't create socket: %s\n", strerror(get_error(sock)));
		return -1;
	}
	
	saddr.sin_port = htons(cla.port);
	saddr.sin_addr = cla.host;

	if ((err=do_connect(sock, (struct sockaddr*)&saddr,
			    sizeof(saddr))) < 0) {
		printf("can't connect to %s:%u: %s\n",
		       inet_ntoa(cla.host), ntohs(saddr.sin_port), 
		       strerror(get_error(err)));
		return -1;
	}

	snprintf(req, sizeof(req), "GET /%s HTTP/1.0\r\n\r\n", cla.request);

	if ((err=do_full_write(sock, req, sizeof(req))) < 0) {
		printf("can't write: %s\n", strerror(get_error(err)));
		return -1;
	}

	#define SAMPLE_RATE 1
	unsigned long total=0, last_time=time(NULL), last_bytes=0;
	while ((err=do_read(sock, buffer, sizeof(buffer))) > 0) {
		write(1, buffer, err);
		total+=err;
		if (time(NULL)-last_time >= SAMPLE_RATE) {
			fprintf(stderr, "%ld %ld\n", total, (total-last_bytes)/SAMPLE_RATE);
			last_time=time(NULL);
			last_bytes=total;
		}
	}

	if (cla.lkl)
		lkl_sys_halt();

        return 0;
}
