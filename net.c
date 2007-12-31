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
#include <asm/env.h>
#include <asm/byteorder.h>

#define htonl(x) __cpu_to_be32(x)
#define htons(x) __cpu_to_be16(x)

int ifindex;

int init(void)
{
	char mac[]={0x00,0x0E,0x35,0xE5,0xD5,0x0C};//

	ifindex=lkl_add_eth(mac, 21);
	return 0;
}

enum sock_type {
	SOCK_STREAM	= 1,
	SOCK_DGRAM	= 2,
	SOCK_RAW	= 3,
	SOCK_RDM	= 4,
	SOCK_SEQPACKET	= 5,
	SOCK_DCCP	= 6,
	SOCK_PACKET	= 10,
};

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

void if_up(void)
{
	struct ifreq ifr;
	int err, sock = lkl_sys_socket(PF_INET, SOCK_DGRAM, 0);
	printf("%s: sock=%d\n", __FUNCTION__, sock);

	strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
	lkl_sys_ioctl(sock, SIOCGIFFLAGS, (long)&ifr);
	ifr.ifr_flags |= IFF_UP;
	err=lkl_sys_ioctl(sock, SIOCSIFFLAGS, (long)&ifr);
	printf("%s: err=%d\n", __FUNCTION__, err);
	lkl_sys_close(sock);
}

void ip_add(void)
{
	struct {
		struct nlmsghdr 	n;
		struct ifaddrmsg 	ifa;
		char   			buf[256];
	} req;
	int err, sock=lkl_sys_socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	printf("%s: sock=%d\n", __FUNCTION__, sock);

	memset(&req, 0, sizeof(req));
	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE|NLM_F_EXCL;
	req.n.nlmsg_type = RTM_NEWADDR;

	req.ifa.ifa_family = AF_INET;
	req.ifa.ifa_prefixlen = 24;
	req.ifa.ifa_scope = 0;
	req.ifa.ifa_index = ifindex;

	int addr=htonl((192<<24)+(168<<16)+(1<<8)+100);
	addattr_l(&req.n, sizeof(req), IFA_LOCAL, &addr, sizeof(addr));
	int brd=htonl(0x01ffffff);
	addattr_l(&req.n, sizeof(req), IFA_BROADCAST, &brd, sizeof(brd));

	err=lkl_sys_send(sock, &req, sizeof(req), 0);
	printf("%s: err=%d\n", __FUNCTION__, err);

	lkl_sys_close(sock);
}


int addattr32(struct nlmsghdr *n, int maxlen, int type, __u32 data)
{
	int len = RTA_LENGTH(4);
	struct rtattr *rta;
	if (NLMSG_ALIGN(n->nlmsg_len) + len > maxlen) {
		fprintf(stderr,"addattr32: Error! max allowed bound %d exceeded\n",maxlen);
		return -1;
	}
	rta = NLMSG_TAIL(n);
	rta->rta_type = type;
	rta->rta_len = len;
	memcpy(RTA_DATA(rta), &data, 4);
	n->nlmsg_len = NLMSG_ALIGN(n->nlmsg_len) + len;
	return 0;
}


void route_add(void)
{
	struct {
		struct nlmsghdr 	n;
		struct rtmsg 		r;
		char   			buf[1024];
	} req;
	int err, sock=lkl_sys_socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	printf("%s: sock=%d\n", __FUNCTION__, sock);


	memset(&req, 0, sizeof(req));

	req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.n.nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
	req.n.nlmsg_type = RTM_NEWROUTE;
	req.r.rtm_family = AF_INET;
	req.r.rtm_table = RT_TABLE_MAIN;
	req.r.rtm_scope = RT_SCOPE_NOWHERE;

	req.r.rtm_protocol = RTPROT_BOOT;
	req.r.rtm_scope = RT_SCOPE_UNIVERSE;
	req.r.rtm_type = RTN_UNICAST;

	int gateway=htonl((192<<24)+(168<<16)+(1<<8)+1);
	addattr_l(&req.n, sizeof(req), RTA_GATEWAY, &gateway, sizeof(gateway));

	int dst=0;
	req.r.rtm_dst_len = 0;
	addattr_l(&req.n, sizeof(req), RTA_DST, &dst, sizeof(dst));

	addattr32(&req.n, sizeof(req), RTA_OIF, ifindex);

	err=lkl_sys_send(sock, &req, sizeof(req), 0);
	printf("%s: err=%d\n", __FUNCTION__, err);

	lkl_sys_close(sock);
}



int main(void)
{
	lkl_env_init(init, 16*1024*1024);

	if_up();
	ip_add();
	route_add();

	int err, sock=lkl_sys_socket(PF_INET, SOCK_STREAM, 0);
	printf("%s: sock=%d\n", __FUNCTION__, sock);
	
	struct sockaddr_in saddr = {
		.sin_family = AF_INET,
		.sin_port = htons(80),
		.sin_addr.s_addr = htonl((192<<24)+(168<<16)+(1<<8)+7)
//		.sin_addr.s_addr = htonl((141<<24)+(85<<16)+(37<<8)+30)
	};

	err=lkl_sys_connect(sock, (struct sockaddr*)&saddr, sizeof(saddr));
	printf("%s: err=%d\n", __FUNCTION__, err);				   

//	char req[]="GET /~tavi/asdf/asf/incoming/BT/Gli%20indesiderabili%20-%20Scimeca%202003.mpeg HTTP/1.0\r\n\r\n";
	char req[]="GET /~tavi/big HTTP/1.0\r\n\r\n";
	err=lkl_sys_write(sock, req, sizeof(req));
	printf("%s: err=%d\n", __FUNCTION__, err);				   

	char buffer[4096];

	#define SAMPLE_RATE 1
	unsigned long total=0, last_time=time(NULL), last_bytes=0;
	while ((err=lkl_sys_read(sock, buffer, sizeof(buffer))) > 0) {
		total+=err;
		if (time(NULL)-last_time >= SAMPLE_RATE) {
			printf("%ld %ld\n", total, (total-last_bytes)/SAMPLE_RATE);
			last_time=time(NULL);
			last_bytes=total;
		}
	}

	lkl_sys_halt();

        return 0;
}
