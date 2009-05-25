/*
 * =====================================================================================
 *
 *       Filename:  zdclient.c
 *
 *    Description:  main source file for ZDClient
 *
 *        Version:  0.2
 *        Created:  05/17/2009 05:38:56 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  PT<pentie@gmail.com>
 *        Company:  http://apt-blog.co.cc
 *
 * =====================================================================================
 */

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include <signal.h>
#include <getopt.h>
#include <unistd.h>
#include "md5.h"

//#include <assert.h>

/* ZDClient Version */
#define ZDC_VER "0.7"

/* default snap length (maximum bytes per packet to capture) */
#define SNAP_LEN 1518

/* ethernet headers are always exactly 14 bytes [1] */
#define SIZE_ETHERNET 14

/* Ethernet addresses are 6 bytes */
#define ETHER_ADDR_LEN	6
/* Ethernet header */
struct sniff_ethernet {
    u_char  ether_dhost[ETHER_ADDR_LEN];    /* destination host address */
    u_char  ether_shost[ETHER_ADDR_LEN];    /* source host address */
    u_short ether_type;                     /* IP? ARP? RARP? etc */
};

struct sniff_eap_header {
    u_char eapol_v;
    u_char eapol_t;
    u_short eapol_length;
    u_char eap_t;
    u_char eap_id;
    u_short eap_length;
    u_char eap_op;
    u_char eap_v_length;
    u_char eap_md5_challenge[16];
};

enum EAPType {
    EAPOL_START,
    EAPOL_LOGOFF,
    EAP_REQUEST_IDENTITY,
    EAP_RESPONSE_IDENTITY,
    EAP_REQUEST_IDENTITY_KEEP_ALIVE,
    EAP_RESPONSE_IDENTITY_KEEP_ALIVE,
    EAP_REQUETS_MD5_CHALLENGE,
    EAP_RESPONSE_MD5_CHALLENGE,
    EAP_SUCCESS,
    EAP_FAILURE,
    ERROR
};

enum STATE {
   READY,
   STARTED,
   ID_AUTHED,
   ONLINE
};

void    send_eap_packet(enum EAPType send_type);
void    show_usage();
char*   get_md5_digest(const char* str, size_t len);
void    action_by_eap_type(enum EAPType pType, 
                        const struct sniff_eap_header *header);
void    send_eap_packet(enum EAPType send_type);
void    init_frames();
void    init_info();
void    init_device();
void    init_arguments(int argc, char **argv);
int     set_device_new_ip();
void    fill_password_md5(u_char attach_key[], u_int id);
void program_unique_check(const char* program_name);


static void signal_interrupted (int signo);
static void get_packet(u_char *args, const struct pcap_pkthdr *header, 
                        const u_char *packet);


char        errbuf[PCAP_ERRBUF_SIZE];  /* error buffer */
enum STATE  state;                     /* program state */
pcap_t      *handle = NULL;			   /* packet capture handle */

int         dhcp_on = 0;               /* DHCP 模式标记 */
int         background = 0;            /* 后台运行标记  */     
char        *dev = NULL;               /* 连接的设备名 */
char        *username = NULL;          
char        *password = NULL;

int         username_length;
int         password_length;

char        *user_gateway = NULL;      /* 由用户设定的四个报文参数 */
char        *user_dns = NULL;
char        *user_ip = NULL;
char        *user_mask = NULL;

u_int       local_ip;			       /* 网卡IP，网络序，下同 */
u_int       local_mask;			       /* subnet mask */
u_int       local_gateway = -1;
u_int       local_dns = -1;
u_char      local_mac[ETHER_ADDR_LEN]; /* MAC地址 */

char        *client_ver = NULL;         /* 报文协议版本号 */

u_char      muticast_mac[] =            /* 802.1x的认证服务器多播地址 */
                        {0x01, 0x80, 0xc2, 0x00, 0x00, 0x03};

u_char      eapol_start[18];            /* EAPOL START报文 */
u_char      eapol_logoff[18];           /* EAPOL LogOff报文 */
u_char      *eap_response_ident = NULL; /* EAP RESPON/IDENTITY报文 */
u_char      *eap_response_md5ch = NULL; /* EAP RESPON/MD5 报文 */

u_int       live_count = 0;             /* KEEP ALIVE 报文的计数值 */
pid_t       current_pid = 0;            /* 记录后台进程的pid */

int         use_pseudo_ip = 0;          /* DHCP模式网卡无IP情况下使用伪IP的标志 */
int         exit_flag = 0;

// debug function
void 
print_hex(u_char *array, int count)
{
    int i;
    for(i = 0; i < count; i++){
        printf("%02x ", array[i]);
    }
    printf("\n");
}

void
show_usage()
{
    printf( "\n"
            "ZDClient %s \n"
            "\t  -- Supllicant for DigiChina Authentication.\n"
            "\n"
            "  Usage:\n"
            "\tRun under root privilege, usually by `sudo', with your \n"
            "\taccount info in arguments:\n\n"
            "\t-u, --username           Your username.\n"
            "\t-p, --password           Your password.\n"
            "\n"
            "  Optional Arguments:\n\n"
            "\t-g, --gateway         Specify Gateway server address. \n\n"

            "\t-d, --dns             Specify DNS server address. \n\n"

            "\t--device              Specify which device to use.\n"
            "\t                      Default is usually eth0.\n\n"

            "\t--dhcp                Use DHCP mode if your ISP requests.\n"
            "\t                      You may need to run `dhclient' manualy to\n"
            "\t                      renew your IP address after successful \n"
            "\t                      authentication.\n\n"

            "\t--ip                  With DHCP mode on, program need to send \n"
            "\t--mask                packet to the server with an IP and MASK, use \n"
            "\t                      this arguments to specify them, or program will\n"
            "\t                      use a pseudo one.  Affacts only when both promoted.\n\n"

            "\t-b, --background      Program fork to background after authentication.\n\n"

            "\t--ver                 Specify a client version. \n"
            "\t                      Default is `3.5.04.1013fk'.\n"
            "\t                      Other known versions are:\n"
            "\t                      `3.5.04.1110fk', `3.5.04.0324', \n"
            "\t                      `3.4.2006.1027', `3.4.2006.1229', \n"
            "\t                      `3.4.2006.0220'\n"
            "\t                      NO longer than 13 Bytes allowed.\n\n"

            "\t-l                    Tell the process to Logoff.\n\n"

            "\t-h, --help            Show this help.\n\n"
            "\n"
            "  About ZDClient:\n\n"
            "\tThis program is a C implementation to DigiChina Authentication,\n"
            "\twith a simple goal of replacing a Java `scut_supplicant' by Yaoqi.\n\n"

            "\tZDC Client is a software developed individually, with NO any rela-\n"
            "\tiontship with Digital China company.\n\n\n"
            
            "\tAnother PT work. Blog: http://apt-blog.co.cc\n"
            "\t\t\t\t\t\t\t\t2009.05.22\n",
            ZDC_VER);
}

/* calcuate for md5 digest */
char* 
get_md5_digest(const char* str, size_t len)
{
	md5_state_t state;
	md5_byte_t digest[16];
	md5_init(&state);
	md5_append(&state, (const md5_byte_t *)str, len);
	md5_finish(&state, digest);

    char *result = malloc(16);
    memcpy(result, digest, 16);
    return result;
}

enum EAPType 
get_eap_type(const struct sniff_eap_header *eap_header) 
{
    switch (eap_header->eap_t){
        case 0x01:
            if ( eap_header->eap_op == 0x01 &&
                        eap_header->eap_id == 0x03 )
                return EAP_REQUEST_IDENTITY_KEEP_ALIVE;
            if ( eap_header->eap_op == 0x01)
                return EAP_REQUEST_IDENTITY;
            if ( eap_header->eap_op == 0x04)
                return EAP_REQUETS_MD5_CHALLENGE;

            break;
        case 0x03:
        //    if (eap_header->eap_id == 0x02)
            return EAP_SUCCESS;
            break;
        case 0x04:
            return EAP_FAILURE;
    }
    fprintf (stderr, "&&IMPORTANT: Unknown Package : eap_t:      %02x\n"
                    "                               eap_id: %02x\n"
                    "                               eap_op:     %02x\n", 
                    eap_header->eap_t, eap_header->eap_id,
                    eap_header->eap_op);
    return ERROR;
}

void 
action_by_eap_type(enum EAPType pType, 
                        const struct sniff_eap_header *header) {
//    printf("PackType: %d\n", pType);
    switch(pType){
        case EAP_SUCCESS:
            state = ONLINE;
            fprintf(stdout, "##Protocol: EAP_SUCCESS\n");
            fprintf(stdout, "&&Info: Authorized Access to Network. \n");
            if (background){
                background = 0;         /* 防止以后误触发 */
                pid_t pID = fork();     /* fork至后台，主程序退出 */
                if (pID != 0) {
                    fprintf(stdout, "&&Info: ZDClient Forked background with PID: [%d]\n\n", pID);
                    exit(0);
                }
            }
            current_pid = getpid();     /* 取得当前进程PID */
            break;
        case EAP_FAILURE:
            state = READY;
            fprintf(stdout, "##Protocol: EAP_FAILURE\n");
            if(state == ONLINE){
                fprintf(stdout, "&&Info: SERVER Forced Logoff\n");
            }
            if (state == STARTED){
                fprintf(stdout, "&&Info: Invalid Username or Client info mismatch.\n");
            }
            if (state == ID_AUTHED){
                fprintf(stdout, "&&Info: Invalid Password.\n");
            }
            pcap_breakloop (handle);
            break;
        case EAP_REQUEST_IDENTITY:
            if (state == STARTED){
                fprintf(stdout, "##Protocol: REQUEST EAP-Identity\n");
            }
            send_eap_packet(EAP_RESPONSE_IDENTITY);
            break;
        case EAP_REQUETS_MD5_CHALLENGE:
            state = ID_AUTHED;
            fprintf(stdout, "##Protocol: REQUEST MD5-Challenge(PASSWORD)\n");
            fill_password_md5((u_char*)header->eap_md5_challenge, header->eap_id);
            send_eap_packet(EAP_RESPONSE_MD5_CHALLENGE);
            break;
        case EAP_REQUEST_IDENTITY_KEEP_ALIVE:
            if (state == ONLINE){
                fprintf(stdout, "[%d]##Protocol: REQUEST EAP_REQUEST_IDENTITY_KEEP_ALIVE (%d)\n",
                                            current_pid,live_count++);
            }

            // 使用伪IP模式认证成功后，获取真实IP，并写入RES/IDTY数据块
            if (use_pseudo_ip){

                //若获取成功，关闭伪IP模式标签
                if (set_device_new_ip() == 0) {
                    use_pseudo_ip = 0;
                }
            }

            send_eap_packet(EAP_RESPONSE_IDENTITY_KEEP_ALIVE);
            break;
        default:
            return;
    }
}

void 
send_eap_packet(enum EAPType send_type)
{
    u_char *frame_data;
    int     frame_length = 0;
    switch(send_type){
        case EAPOL_START:
            state = STARTED;
            frame_data= eapol_start;
            frame_length = 14 + 4;
            fprintf(stdout, "##Protocol: SEND EAPOL-Start\n");
            break;
        case EAPOL_LOGOFF:
            state = READY;
            frame_data = eapol_logoff;
            frame_length = 14 + 4;
            fprintf(stdout, "##Protocol: SEND EAPOL-Logoff\n");
            break;
        case EAP_RESPONSE_IDENTITY:
            frame_data = eap_response_ident;
            frame_length = 14 + 9 + username_length + 46;
            if (*(frame_data + 14 + 5) != 0x01){
                *(frame_data + 14 + 5) = 0x01;
            }
            fprintf(stdout, "##Protocol: SEND EAP-Response/Identity\n");
            break;
        case EAP_RESPONSE_MD5_CHALLENGE:
            frame_data = eap_response_md5ch;
            frame_length = 14 + 10 + 16 + username_length + 46;
            fprintf(stdout, "##Protocol: SEND EAP-Response/Md5-Challenge\n");
            break;
        case EAP_RESPONSE_IDENTITY_KEEP_ALIVE:
            frame_data = eap_response_ident;
            frame_length = 14 + 9 + username_length + 46;
            if (*(frame_data + 14 + 5) != 0x03){
                *(frame_data + 14 + 5) = 0x03;
            }
            fprintf(stdout, "[%d]##Protocol: SEND EAP_RESPONSE_IDENTITY_KEEP_ALIVE\n", current_pid);
            break;
        default:
            fprintf(stderr,"&&IMPORTANT: Wrong Send Request Type.%02x\n", send_type);
            return;
    }
    if (pcap_sendpacket(handle, frame_data, frame_length) != 0)
    {
        fprintf(stderr,"&&IMPORTANT: Error Sending the packet: %s\n", pcap_geterr(handle));
        return;
    }
}

/* Callback function for pcap.  */
void
get_packet(u_char *args, const struct pcap_pkthdr *header, 
    const u_char *packet)
{
	/* declare pointers to packet headers */
	const struct sniff_ethernet *ethernet;  /* The ethernet header [1] */
    const struct sniff_eap_header *eap_header;

    ethernet = (struct sniff_ethernet*)(packet);
    eap_header = (struct sniff_eap_header *)(packet + SIZE_ETHERNET);

    enum EAPType p_type = get_eap_type(eap_header);
    action_by_eap_type(p_type, eap_header);
    return;
}

void 
init_frames()
{
    u_char *local_info = malloc(46);
    int data_index;

    /* *  local_info segment used by both RES/Idn and RES/MD5 frame * */
    data_index = 0;
    local_info[data_index++] = dhcp_on;
    memcpy(local_info + data_index, &local_ip, 4);
    data_index += 4;
    memcpy(local_info + data_index, &local_mask, 4);
    data_index += 4;
    memcpy(local_info + data_index, &local_gateway, 4);
    data_index += 4;
    memcpy(local_info + data_index, &local_dns, 4);
    data_index += 4;
    char* username_md5 = get_md5_digest(username, username_length);
    memcpy(local_info + data_index, username_md5, 16);
    data_index += 16;
    free(username_md5);
    strncpy ((char*)local_info + data_index, client_ver, 13);


    /*****  EAPOL Header  *******/
    u_char eapol_header[SIZE_ETHERNET];
    data_index = 0;
    u_short eapol_t = htons (0x888e);
    memcpy (eapol_header + data_index, muticast_mac, 6); /* dst addr. muticast */
    data_index += 6;
    memcpy (eapol_header + data_index, local_mac, 6);    /* src addr. local mac */
    data_index += 6;
    memcpy (eapol_header + data_index, &eapol_t, 2);    /*  frame type, 0x888e*/

    /**** EAPol START ****/
    u_char start_data[4] = {0x01, 0x01, 0x00, 0x00};
    memcpy (eapol_start, eapol_header, 14);
    memcpy (eapol_start + 14, start_data, 4);

    /****EAPol LOGOFF ****/
    u_char logoff_data[4] = {0x01, 0x02, 0x00, 0x00};
    memcpy (eapol_logoff, eapol_header, 14);
    memcpy (eapol_logoff + 14, logoff_data, 4);


    /* EAP RESPONSE IDENTITY */
    u_char eap_resp_iden_head[9] = {0x01, 0x00, 
                                    0x00, 5 + 46 + username_length,  /* eapol_length */
                                    0x02, 0x01, 
                                    0x00, 5 + username_length,       /* eap_length */
                                    0x01};
    
    eap_response_ident = malloc (14 + 9 + username_length + 46);
    memset(eap_response_ident, 0, 14 + 9 + username_length + 46);

    data_index = 0;
    memcpy (eap_response_ident + data_index, eapol_header, 14);
    data_index += 14;
    memcpy (eap_response_ident + data_index, eap_resp_iden_head, 9);
    data_index += 9;
    memcpy (eap_response_ident + data_index, username, username_length);
    data_index += username_length;
    memcpy (eap_response_ident + data_index, local_info, 46);

    /** EAP RESPONSE MD5 Challenge **/
    u_char eap_resp_md5_head[10] = {0x01, 0x00, 
                                   0x00, 6 + 16 + username_length + 46, /* eapol-length */
                                   0x02, 0x02, 
                                   0x00, 6 + 16 + username_length, /* eap-length */
                                   0x04, 0x10};
    eap_response_md5ch = malloc (14 + 4 + 6 + 16 + username_length + 46);

    data_index = 0;
    memcpy (eap_response_md5ch + data_index, eapol_header, 14);
    data_index += 14;
    memcpy (eap_response_md5ch + data_index, eap_resp_md5_head, 10);
    data_index += 26;// 剩余16位在收到REQ/MD5报文后由fill_password_md5填充 
    memcpy (eap_response_md5ch + data_index, username, username_length);
    data_index += username_length;
    memcpy (eap_response_md5ch + data_index, local_info, 46);
}

void 
fill_password_md5(u_char attach_key[], u_int eap_id)
{
    char *psw_key = malloc(1 + password_length + 16);
    char *md5_challenge_key;
    psw_key[0] = eap_id;
    memcpy (psw_key + 1, password, password_length);
    memcpy (psw_key + 1 + password_length, attach_key, 16);

    md5_challenge_key = get_md5_digest(psw_key, 1 + password_length + 16);
    memcpy (eap_response_md5ch + 14 + 10, md5_challenge_key, 16);

    free (psw_key);
    free (md5_challenge_key);
}

void init_info()
{
    if(username == NULL || password == NULL){
        fprintf (stderr,"Error: NO Username or Password promoted.\n"
                        "Try zdclient --help for usage.\n");
        exit(EXIT_FAILURE);
    }
    username_length = strlen(username);
    password_length = strlen(password);
/*
    if (dhcp_on){
        if (user_ip == NULL){
            fprintf (stderr,"&&Info:DHCP Modol On with NO IP specified.\n"
                            "Use default pseudo IP `169.254.216.45'.\n");
            user_ip = "169.254.216.45";
        }
        if (user_mask == NULL) {
            fprintf (stderr,"&&Info:DHCP Modol On with NO MASK specified.\n"
                            "Use default MASK `255.255.0.0' .\n");
            user_mask = "255.255.0.0";
        }
    }
*/
    if (user_ip)
        local_ip = inet_addr (user_ip);
    else 
        local_ip = 0;

    if (user_mask)
        local_mask = inet_addr (user_mask);
    else 
        local_mask = 0;

    if (user_gateway)
        local_gateway = inet_addr (user_gateway);
    else 
        local_gateway = 0;

    if (user_dns)
        local_dns = inet_addr (user_dns);
    else
        local_dns = 0;

    if (local_ip == -1 || local_mask == -1 || local_gateway == -1 || local_dns == -1) {
        fprintf (stderr,"ERROR: One of specified IP, MASK, Gateway and DNS address\n"
                        "in the arguments format error.\n");
        exit(EXIT_FAILURE);
    }

    if(client_ver == NULL)
        client_ver = "3.5.04.1013fk";
    else{
        if (strlen (client_ver) > 13) {
            fprintf (stderr, "Error: Specified client version `%s' longer than 13 Bytes.\n"
                    "Try `zdclient --help' for more information.\n", client_ver);
            exit(EXIT_FAILURE);
        }
    }
}

void init_device()
{
    struct bpf_program fp;			/* compiled filter program (expression) */
    char filter_exp[51];/* filter expression [3] */
//	bpf_u_int32 mask;			/* subnet mask */
//	bpf_u_int32 net;			/* ip */

    if(dev == NULL)
	    dev = pcap_lookupdev(errbuf);

	if (dev == NULL) {
		fprintf(stderr, "Couldn't find default device: %s\n",
			errbuf);
		exit(EXIT_FAILURE);
    }
	
	/* open capture device */
	handle = pcap_open_live(dev, SNAP_LEN, 1, 1000, errbuf);

	if (handle == NULL) {
		fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
		exit(EXIT_FAILURE);
	}

	/* make sure we're capturing on an Ethernet device [2] */
	if (pcap_datalink(handle) != DLT_EN10MB) {
		fprintf(stderr, "%s is not an Ethernet\n", dev);
		exit(EXIT_FAILURE);
	}

    /* get device basic infomation */
    struct ifreq ifr;
    int sock;
    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    strcpy(ifr.ifr_name, dev);

    //获得网卡Mac
    if(ioctl(sock, SIOCGIFHWADDR, &ifr) < 0)
    {
        perror("ioctl");
        exit(EXIT_FAILURE);
    }
    memcpy(local_mac, ifr.ifr_hwaddr.sa_data, ETHER_ADDR_LEN);
    
    //尝试获得网卡IP
    if(ioctl(sock, SIOCGIFADDR, &ifr) < 0)
    {
        //获取不了IP
        if (dhcp_on){ //DHCP模式下
            use_pseudo_ip = 1; //设置标签
            fprintf(stdout, "&&Info: No IP attached to %s, use `169.254.216.45' instead.\n",
                    dev);
        }
        else {
            perror("ioctl");
            exit(EXIT_FAILURE);
        }
    }

    //如果用户同时指定了IP和MASK，则优先使用(已在init_info转换完成)，
    //否则由程序处理
    if (!(local_ip && local_mask)) {

        //获取不了IP，且用户没有定义IP，使用伪IP
        if (use_pseudo_ip) {
            local_ip = inet_addr ("169.254.216.45");
            local_mask = inet_addr ("255.255.255.0");
        }

        //获取到IP，使用网卡的真实IP
        else {
            local_ip = ((struct  sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;

            //获得子网掩码
            if(ioctl(sock, SIOCGIFNETMASK, &ifr) < 0)
            {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }
            local_mask = ((struct sockaddr_in*)&ifr.ifr_netmask)->sin_addr.s_addr;
        }
    }


    /* construct the filter string */
    sprintf(filter_exp, "ether dst %02x:%02x:%02x:%02x:%02x:%02x"
                        " and ether proto 0x888e", 
                        local_mac[0], local_mac[1],
                        local_mac[2], local_mac[3],
                        local_mac[4], local_mac[5]);

	/* compile the filter expression */
	if (pcap_compile(handle, &fp, filter_exp, 0, 0) == -1) {
		fprintf(stderr, "Couldn't parse filter %s: %s\n",
		    filter_exp, pcap_geterr(handle));
		exit(EXIT_FAILURE);
	}

	/* apply the compiled filter */
	if (pcap_setfilter(handle, &fp) == -1) {
		fprintf(stderr, "Couldn't install filter %s: %s\n",
		    filter_exp, pcap_geterr(handle));
		exit(EXIT_FAILURE);
	}
    pcap_freecode(&fp);
}

int set_device_new_ip()
{
    struct ifreq ifr;
    int sock;
    
    strcpy(ifr.ifr_name, dev);
    if((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    if(ioctl(sock, SIOCGIFADDR, &ifr) < 0)
    {
        return -1;
    }
    if(ioctl(sock, SIOCGIFNETMASK, &ifr) < 0)
    {
        return -1;
    }
    local_ip = ((struct  sockaddr_in*)&ifr.ifr_addr)->sin_addr.s_addr;
    local_mask = ((struct sockaddr_in*)&ifr.ifr_netmask)->sin_addr.s_addr;

    size_t data_index = 14 + 9 + username_length + 1;
    memcpy (eap_response_ident + data_index, &local_ip, 4);
    data_index += 4;
    memcpy (eap_response_ident + data_index, &local_mask, 4);
    return 0;
}

static void
signal_interrupted (int signo)
{
    fprintf(stdout,"\n&&Info: USER Interrupted. \n");
    send_eap_packet(EAPOL_LOGOFF);
    pcap_breakloop (handle);
}

void init_arguments(int argc, char **argv)
{
    /* Option struct for progrm run arguments */
    static struct option long_options[] =
        {
        {"help",        no_argument,        0,              'h'},
        {"background",  no_argument,        &background,    1},
        {"dhcp",        no_argument,        &dhcp_on,       1},
        {"device",      required_argument,  0,              2},
        {"ver",         required_argument,  0,              3},
        {"username",    required_argument,  0,              'u'},
        {"password",    required_argument,  0,              'p'},
        {"ip",          required_argument,  0,              4},
        {"mask",        required_argument,  0,              5},
        {"gateway",     required_argument,  0,              'g'},
        {"dns",         required_argument,  0,              'd'},
        {0, 0, 0, 0}
        };

    int c;
    while (1) {

        /* getopt_long stores the option index here. */
        int option_index = 0;
        c = getopt_long (argc, argv, "u:p:g:d:hbl",
                        long_options, &option_index);
        if (c == -1)
            break;
        switch (c) {
            case 0:
               break;
            case 'b':
                background = 1;
                break;
            case 2:
                dev = optarg;
                break;
            case 4:
                user_ip = optarg;
                break;
            case 5:
                user_mask = optarg;
                break;
            case 'u':
                username = optarg;
                break;
            case 'p':
                password = optarg;
                break;
            case 'g':
                user_gateway = optarg;
                break;
            case 'd':
                user_dns = optarg;
                break;
            case 'l':
                exit_flag = 1;
                break;
            case 'h':
                show_usage();
                exit(EXIT_SUCCESS);
                break;
            case '?':
                if (optopt == 'u' || optopt == 'p'|| optopt == 'g'|| optopt == 'd')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                exit(EXIT_FAILURE);
                break;
            default:
                fprintf (stderr,"Unknown option character `\\x%x'.\n", c);
                exit(EXIT_FAILURE);
        }
    }    
}

void program_unique_check(const char* program)
{
    FILE    *fd;
    pid_t   id = 0;
    char    command[50] = {0};
    char    pid_num[20] = {0};
    const char* program_name;

    program_name = strrchr (program, '/');
    if (program_name)
        ++program_name;
    else
        program_name = program;

    strcat (command, "ps -Ao pid,comm|grep ");
    strcat (command, program_name);

    if ( (fd = popen(command, "r")) == NULL ) {
        perror("popen");
        exit(EXIT_FAILURE);
    }

    fgets(pid_num, 20, fd);

    id = atoi(pid_num);

    if (exit_flag){
        if ( getpid() == id ){
            fprintf (stderr, "@@Error: No `%s' Running.\n", program_name);
            exit(EXIT_FAILURE);
        }
        if ( kill (id, SIGINT) == -1 ) {
			perror("kill");
			exit(EXIT_FAILURE);
        }
        fprintf (stdout, "&&Info: Exit Signal Sent.\n");
        exit(EXIT_SUCCESS);
    }
    if ( getpid() != id ){
        fprintf (stderr, "@@Error: There's another `%s' running with PID %d\n",
                program_name, id);
        exit(EXIT_FAILURE);
    }
    pclose(fd);
}

int main(int argc, char **argv)
{
    init_arguments (argc, argv);
    program_unique_check (argv[0]);
    init_info();
    init_device();
    init_frames ();

    signal (SIGINT, signal_interrupted);
    signal (SIGTERM, signal_interrupted);    

    printf("######## ZDClient ver. %s #########\n", ZDC_VER);
    printf("Device:     %s\n", dev);
    printf("MAC:        ");
    print_hex(local_mac, 6);
    printf("IP:         %s\n", inet_ntoa(*(struct in_addr*)&local_ip));
    printf("MASK:       %s\n", inet_ntoa(*(struct in_addr*)&local_mask));
    printf("Gateway:    %s\n", inet_ntoa(*(struct in_addr*)&local_gateway));
    printf("DNS:        %s\n", inet_ntoa(*(struct in_addr*)&local_dns));
    printf("Client ver: %s\n", client_ver);
    printf("####################################\n");

    send_eap_packet (EAPOL_LOGOFF);
    send_eap_packet (EAPOL_START);

	pcap_loop (handle, -1, get_packet, NULL);   /* main loop */
	pcap_close (handle);
    free (eap_response_ident);
    free (eap_response_md5ch);
    return 0;
}

