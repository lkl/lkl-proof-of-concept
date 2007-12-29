#define _NO_OLDNAMES
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <malloc.h>


#undef __GLIBC__
#include <linux/stat.h>
#include <asm/unistd.h>
#include <asm/callbacks.h>
#include <linux/if.h>
#include <linux/net.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_addr.h>
#include <asm/eth.h>

int ifindex;

int init(void)
{
	char mac[]={0,1,2,3,4,5};

	ifindex=lkl_add_eth(mac, 16);
	return 0;
}

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

int addattr_l(struct nlmsghdr *n, int maxlen, int type, const void *data,
	      int alen)
{
	int len = RTA_LENGTH(alen);
	struct rtattr *rta;

	if (NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len) > maxlen) {
		fprintf(stderr, "addattr_l ERROR: message exceeded bound of %d\n",maxlen);
		return -1;
	}
	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), data, alen);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + RTA_ALIGN(len);
	return 0;
}


int main(void)
{
	lkl_env_init(init);

enum sock_type {
	SOCK_STREAM	= 1,
	SOCK_DGRAM	= 2,
	SOCK_RAW	= 3,
	SOCK_RDM	= 4,
	SOCK_SEQPACKET	= 5,
	SOCK_DCCP	= 6,
	SOCK_PACKET	= 10,
};


	int err, sock = lkl_sys_socket(PF_INET, SOCK_DGRAM, 0);
	struct ifreq ifr;
	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
	lkl_sys_ioctl(sock, SIOCGIFFLAGS, (long)&ifr);
	ifr.ifr_flags |= IFF_UP;
	err=lkl_sys_ioctl(sock, SIOCSIFFLAGS, (long)&ifr);
	lkl_sys_close(sock);
		


	sock=lkl_sys_socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

	printf("%d\n", sock);

	struct {
		struct nlmsghdr 	n;
		struct ifaddrmsg 	ifa;
		char   			buf[256];
	} req;
	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE|NLM_F_EXCL;
	req.n.nlmsg_type = RTM_NEWADDR;

	req.ifa.ifa_family = AF_INET;
	req.ifa.ifa_prefixlen = 8;
	req.ifa.ifa_scope = 0;
	req.ifa.ifa_index = ifindex;

	int addr=htonl(0x01000001);
	addattr_l(&req.n, sizeof(req), IFA_LOCAL, &addr, sizeof(addr));
	int brd=htonl(0x01ffffff);
	addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &brd, sizeof(brd));

	err=lkl_sys_send(sock, &req, sizeof(req), 0);

	printf("%d %d\n", err, sizeof(req));

	while(1)
		;

	lkl_sys_halt();

        return 0;
}
