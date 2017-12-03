/**
 * @file rtcpxr.c  SIP RTCP-XR PUBLISH -- RFC 6035
 *
 * Copyright (C) 2010 Creytiv.com
 */
#include <re.h>
#include <baresip.h>
#include "core.h"


static struct sip_lsnr *lsnr;
static rtcpxr_recv_h *recvh;
static void *recvarg;


static void handle_rtcpxr(struct ua *ua, const struct sip_msg *msg)
{
	static const char ctype_text[] = "application/vq-rtcpxr";
	struct pl ctype_pl = {ctype_text, sizeof(ctype_text)-1};
	(void)ua;

	if (msg_ctype_cmp(&msg->ctyp, "application", "vq-rtcpxr") && recvh) {
		recvh(&msg->from.auri, &ctype_pl, msg->mb, recvarg);
		(void)sip_reply(uag_sip(), msg, 200, "OK");
	}
	else {
		(void)sip_replyf(uag_sip(), msg, 415, "Unsupported Type",
				 "Accept: %s\r\n"
				 "Content-Length: 0\r\n"
				 "\r\n",
				 ctype_text);
	}
}


static bool request_handler(const struct sip_msg *msg, void *arg)
{
	struct ua *ua;

	(void)arg;

	if (pl_strcmp(&msg->met, "PUBLISH"))
		return false;

	ua = uag_find(&msg->uri.user);
	if (!ua) {
		(void)sip_treply(NULL, uag_sip(), msg, 404, "Not Found");
		return true;
	}

	handle_rtcpxr(ua, msg);

	return true;
}


static void resp_handler(int err, const struct sip_msg *msg, void *arg)
{
	struct ua *ua = arg;

	(void)ua;

	if (err) {
		(void)re_fprintf(stderr, " \x1b[31m%m\x1b[;m\n", err);
		return;
	}

	if (msg->scode >= 300) {
		(void)re_fprintf(stderr, " \x1b[31m%u %r\x1b[;m\n",
				 msg->scode, &msg->reason);
	}
}


int rtcpxr_init(rtcpxr_recv_h *h, void *arg)
{
	int err;

	err = sip_listen(&lsnr, uag_sip(), true, request_handler, NULL);
	if (err)
		return err;

	recvh   = h;
	recvarg = arg;

	return 0;
}


void rtcpxr_close(void)
{
	lsnr = mem_deref(lsnr);
}


/**
 * Send SIP instant PUBLISH RTCP-XR to a peer
 *
 * @param ua    User-Agent object
 * @param peer  Peer SIP Address
 * @param msg   Message to send
 *
 * @return 0 if success, otherwise errorcode
 */
int rtcpxr_send(struct ua *ua, const char *peer, const char *msg, ...)
{
	struct sip_addr addr;
	struct pl pl;
	char *uri = NULL;
	int err = 0;

	if (!ua || !peer || !msg)
		return EINVAL;

	pl_set_str(&pl, peer);

	err = sip_addr_decode(&addr, &pl);
	if (err)
		return err;

	err = pl_strdup(&uri, &addr.auri);
	if (err)
		return err;

	err = sip_req_send(ua, "PUBLISH", uri, resp_handler, ua,
			   "Content-Type: application/vq-rtcpxr\r\n"
			   "Content-Length: %zu\r\n"
			   "\r\n%s",
			   str_len(msg), msg);

	mem_deref(uri);

	return err;
}
