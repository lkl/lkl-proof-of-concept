#ifndef _LKL_BENCH_NET_H
#define _LKL_BENCH_NET_H


#include <asm/lkl.h>
#include <asm/env.h>
#include <asm/eth.h>

static int net_sps;
static unsigned short net_iff;
static unsigned long long net_bytes;
static unsigned long long net_packets;

int net_pre_test(void)
{
	int ifindex, err;
	struct ifreq ifr;

	if (!cla.test->net)
		return 0;

	if (!cla.iface) {
		printf("net: no interface specified!\n");
		return -1;
	}

	if (!cla.src.s_addr) {
		char cmd[1024];
		char src[20] = { 0, }, *c;
		struct hostent *hostinfo;

		snprintf(cmd, sizeof(cmd), "ip -4 addr show dev %s | grep inet | head -n1  | tr -s ' ' | cut -f 3 -d ' '",
			 cla.iface);
		FILE *f = popen(cmd, "r");
		if (!f) {
			fprintf(stderr, "net: failed to get IPv4 address on %s\n", cla.iface);
			return -1;
		}

		fscanf(f, "%10s", src);
		c = strchr(src, '/');
		if (!c) {
			fprintf(stderr, "bad IP/netmask: %s\n", src);
			return -1;
		}

		cla.netmask_len = atoi(c+1);
		*c = 0;
		hostinfo = gethostbyname(src);
		if (!hostinfo) {
			fprintf(stderr, "net: no IPv4 address found on %s\n", cla.iface);
			return -1;
		}
		cla.src = *(struct in_addr*)hostinfo->h_addr;
	}

	if (cla.lkl) {
		cla.src.s_addr = htonl(ntohl(cla.src.s_addr) + 1);

		strncpy(ifr.ifr_name, cla.iface, IFNAMSIZ);
		net_sps = socket(PF_INET, SOCK_DGRAM, 0);
		if (net_sps < 0) {
			fprintf(stderr, "failed to create DGRAM INET socket: %s\n", strerror(errno));
			return -1;
		}

		err = ioctl(net_sps, SIOCGIFFLAGS, (long)&ifr);
		if (err) {
			fprintf(stderr, "failed to get flags on %s: %s\n", cla.iface, strerror(errno));
			return -1;
		}
		net_iff = ifr.ifr_flags;
		ifr.ifr_flags |= IFF_PROMISC;
		err = ioctl(net_sps, SIOCSIFFLAGS, (long)&ifr);
		if (err) {
			fprintf(stderr, "failed to set %s in promisc mode\n", cla.iface);
			return -1;
		}

		if ((ifindex=lkl_add_eth(cla.iface, (char*)&cla.mac, 1024)) == 0)
			return -1;

		if ((err=lkl_if_set_ipv4(ifindex, cla.src.s_addr,
					 cla.netmask_len)) < 0) {
			printf("failed to set IP: %s/%d: %s\n",
			       inet_ntoa(cla.src), cla.netmask_len,
			       strerror(-err));
			return -1;
		}

		if ((err=lkl_if_up(ifindex)) < 0) {
			printf("failed to bring interface up: %s\n", strerror(-err));
			return -1;
		}
	}

	return 0;
}

void net_post_test(void)
{
	struct ifreq ifr;

	if (cla.lkl) {
		strncpy(ifr.ifr_name, cla.iface, IFNAMSIZ);
		ifr.ifr_flags = net_iff;
		ioctl(net_sps, SIOCSIFFLAGS, (long)&ifr);
		close(net_sps);
	}
}


int do_connect(int fd, const struct sockaddr *saddr, socklen_t slen)
{
	if (cla.lkl)
		return lkl_sys_connect(fd, (struct sockaddr*)saddr, slen);
	else
		return connect(fd, saddr, slen);
}


int do_bind(int fd, const struct sockaddr *saddr, socklen_t slen)
{
	if (cla.lkl)
		return lkl_sys_bind(fd, (struct sockaddr*)saddr, slen);
	else
		return bind(fd, saddr, slen);
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

void do_close(int fd)
{
	if (cla.lkl)
		lkl_sys_close(fd);
	else
		close(fd);
}

#define get_error(err) (cla.lkl?-err:errno)

static void net_do_test(int type)
{
	char buff[4096];
	struct sockaddr_in saddr = {
		.sin_family = AF_INET,
	};
	int sock, err;

	net_packets = 0;

	if ((sock=do_socket(PF_INET, type, 0)) < 0) {
		printf("can't create socket: %s\n", strerror(get_error(sock)));
		return;
	}


	saddr.sin_port = htons(cla.port);

	if (type == SOCK_STREAM) {
		saddr.sin_addr = cla.dst;
		if ((err=do_connect(sock, (struct sockaddr*)&saddr,
				    sizeof(saddr))) < 0) {
			fprintf(stderr, "can't connect to %s:%u: %s\n",
				inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port),
				strerror(get_error(err)));
			return ;
		}
	} else {
		saddr.sin_addr = cla.src;
		if ((err = do_bind(sock, (struct sockaddr*)&saddr,
				   sizeof(saddr))) < 0) {
			fprintf(stderr, "can't bind to %s:%u: %s\n",
				inet_ntoa(saddr.sin_addr), ntohs(saddr.sin_port),
				strerror(get_error(err)));
		}
	}

	test_mark_start(0);
	while ((err=do_read(sock, buff, sizeof(buff))) > 0) {
		if (type == SOCK_STREAM)
			net_bytes += err;
		else {
			if (++net_packets == 10000)
				break;
		}
	}
	test_mark_stop(0);
	take_sample(test_time(0));

	if (err < 0)
		fprintf(stderr, "net: read eroror: %s\n", strerror(get_error(err)));

	do_close(sock);
}

static void test_net_tcp(void)
{
	int i;

	if (!cla.dst.s_addr) {
		fprintf(stderr, "net: no destination address\n");
		return;
	}


	printf("tcp read... "); fflush(stdout);
	for(i = 0; i < 100; i++)
		net_do_test(SOCK_STREAM);

	printf("read %lld bytes in %f us\n", net_bytes/100, samples_avg());
}

static void test_net_udp(void)
{
	int i;

	printf("udp read... "); fflush(stdout);
	for(i = 0; i < 10; i++)
		net_do_test(SOCK_DGRAM);

	printf("read %lld bytes/packets in %f us\n", net_packets, samples_avg());
}


#endif
