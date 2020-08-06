#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netlink/msg.h>
#include <netlink/attr.h>
#include "genl_header.h"

#ifndef GRADING
#define MAX7219_CS_PIN 12 //12
#define HCSR04_TRIGGER_PIN 5 //2
#define HCSR04_ECHO_PIN 4 //10
#endif

static char* genl_test_mcgrp_names[GENL_TEST_MCGRP_MAX] = {
	GENL_TEST_MCGRP0_NAME,
	GENL_TEST_MCGRP1_NAME,
	GENL_TEST_MCGRP2_NAME,
};

static unsigned int mcgroups;
int pattern ;
int trig_pin,echo_pin,cs_pin,m_request,isInitial;
struct nl_sock* nlsock = NULL;
int count=0;

static int send_msg_to_kernel(struct nl_sock *sock)
{
	struct nl_msg* msg;
	int family_id, err = 0;
	
	//printf("Inside send_msg_to_kernel function\n");
	family_id = genl_ctrl_resolve(sock, GENL_TEST_FAMILY_NAME);
	if(family_id < 0){
		printf("snd_msg function: Unable to resolve family name!\n");
		exit(EXIT_FAILURE);
	}

	msg = nlmsg_alloc();
	if (!msg) {
		fprintf(stderr, "failed to allocate netlink message\n");
		exit(EXIT_FAILURE);
	}

	if(!genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, 
		NLM_F_REQUEST, GENL_TEST_C_MSG, 0)) {
		fprintf(stderr, "failed to put nl hdr!\n");
		err = -ENOMEM;
		goto out;
	}

	//adding HCSR trigger pin value
	err = nla_put_u32(msg, GENL_TRIGGER_PIN_ATTR, trig_pin);
	if (err) {
		fprintf(stderr, "failed to add trigger pin value!\n");
		goto out;
	}

	//adding HCSR echo pin value
	err = nla_put_u32(msg, GENL_ECHO_PIN_ATTR, echo_pin);
	if (err) {
		fprintf(stderr, "failed to add echo pin value!\n");
		goto out;
	}

	//adding LED CS pin value 
	err = nla_put_u32(msg, GENL_CS_PIN_ATTR, cs_pin);
	if (err) {
		fprintf(stderr, "failed to add CS pin value!\n");
		goto out;
	}

	//adding Measurement request 
	err = nla_put_u32(msg, GENL_REQUEST_ATTR, m_request);
	if (err) {
		fprintf(stderr, "failed to add measurement request value!\n");
		goto out;
	}

	//adding pattern request. Toggling between pattern 0 and 1 alternatively when distance is received.
	if(count%2==0) 
		pattern=0; 
	else
		pattern=1;
	
	printf("Sending pattern %d\n",pattern);
	err = nla_put_u32(msg, GENL_PATTERN_ATTR, pattern);
	if (err) {
		fprintf(stderr, "failed to add pattern value!\n");
		goto out;
	}

	//adding initialreq request. isInitial will be 1 for the initial request. From nex time this value will be 0.
	err = nla_put_u32(msg, GENL_INITIAL_ATTR, isInitial);
	if (err) {
		fprintf(stderr, "failed to add pattern value!\n");
		goto out;
	}

	err = nl_send_auto(sock, msg);
	if (err < 0) {
		fprintf(stderr, "failed to send nl message!\n");
	}

out:
	nlmsg_free(msg);
	return err;
}


static int skip_seq_check(struct nl_msg *msg, void *arg)
{
	printf("Skip seq check\n");
	return NL_OK;
}



static int print_rx_msg(struct nl_msg *msg, void* arg)
{
	struct nlattr *attr[GENL_TEST_ATTR_MAX+1];
	//printf("Inside receive function\n");

	genlmsg_parse(nlmsg_hdr(msg), 0, attr, 
			GENL_TEST_ATTR_MAX, genl_test_policy);

	if (!attr[GENL_DISTANCE_ATTR]) {
		fprintf(stdout, "Kernel sent empty message!!\n");
		return NL_OK;
	}

	isInitial=0;
	fprintf(stdout, "HCSR Distance from kernel: %d , Sending new pattern to kernel..\n", nla_get_u32(attr[GENL_DISTANCE_ATTR]));
	count++;
	//printf("Count=%d\n",count);
	send_msg_to_kernel(nlsock);
	return NL_OK;
}

static void prep_nl_sock(struct nl_sock** nlsock)
{
	int family_id, grp_id;
	unsigned int bit = 0;
	
	//printf("Inside prep_nl_sockkkk\n");
	*nlsock = nl_socket_alloc();
	if(!*nlsock) {
		fprintf(stderr, "Unable to alloc nl socket!\n");
		exit(EXIT_FAILURE);
	}

	/* disable seq checks on multicast sockets */
	nl_socket_disable_seq_check(*nlsock);
	nl_socket_disable_auto_ack(*nlsock);

	/* connect to genl */
	if (genl_connect(*nlsock)) {
		fprintf(stderr, "Unable to connect to genl!\n");
		goto exit_err;
	}

	/* resolve the generic nl family id*/
	family_id = genl_ctrl_resolve(*nlsock, GENL_TEST_FAMILY_NAME);
	if(family_id < 0){
		printf("prep_nl_sock: Unable to resolve family name!\n");
		goto exit_err;
	}

	//printf("MCgroups value: %d\n",mcgroups );

	if (!mcgroups)
		return;

	while (bit < sizeof(unsigned int)) {
		//printf("Bit value: %d\n",bit);
		//printf("Inside loop, resolving group name for :%u\n",(1 << bit));
		if (!(mcgroups & (1 << bit)))
			goto next;

		grp_id = genl_ctrl_resolve_grp(*nlsock, GENL_TEST_FAMILY_NAME,
				genl_test_mcgrp_names[bit]);

		if (grp_id < 0)	{
			fprintf(stderr, "Unable to resolve group name for %u!\n",
				(1 << bit));
            goto exit_err;
		}
		if (nl_socket_add_membership(*nlsock, grp_id)) {
			fprintf(stderr, "Unable to join group %u!\n", 
				(1 << bit));
            goto exit_err;
		}
next:
		bit++;
	}

    return;

exit_err:
    nl_socket_free(*nlsock); // this call closes the socket as well
    exit(EXIT_FAILURE);
}


int main(int argc, char** argv)
{
	
	struct nl_cb *cb = NULL;
	int ret;
	int grp=1;


	mcgroups |= 1 << (grp);
	//printf("Main: mcgroups: %d",mcgroups);

	prep_nl_sock(&nlsock);

	trig_pin=HCSR04_TRIGGER_PIN;
	echo_pin=HCSR04_ECHO_PIN;
	cs_pin=MAX7219_CS_PIN;

	m_request=1;
	pattern=0;
	isInitial=1;

	printf("Sending initial pattern to kernel..\n");
	ret = send_msg_to_kernel(nlsock);


	/* prep the cb */
	cb = nl_cb_alloc(NL_CB_DEFAULT);
	nl_cb_set(cb, NL_CB_SEQ_CHECK, NL_CB_CUSTOM, skip_seq_check, NULL);
	nl_cb_set(cb, NL_CB_VALID, NL_CB_CUSTOM, print_rx_msg, NULL);
	do {
		ret = nl_recvmsgs(nlsock, cb);
	} while (!ret);
	
	nl_cb_put(cb);
    nl_socket_free(nlsock);
	return 0;

}