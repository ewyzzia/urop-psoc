#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

#include "cyabs_rtos.h"

#include "cy_secure_sockets.h"

#include "cy_wcm.h"
#include "cy_wcm_error.h"

#include <string.h>

#include "udp_server.h"

#include "cy_nw_helper.h"

#include <inttypes.h>
#include "util.h"

/*******************************************************************************
* Macros
********************************************************************************/

#define MAKE_IPV4_ADDRESS(a, b, c, d)           ((((uint32_t) d) << 24) | \
                                                 (((uint32_t) c) << 16) | \
                                                 (((uint32_t) b) << 8) |\
                                                 ((uint32_t) a))

/* Broadcast address. */
#define BROADCAST_ADDRESS                 MAKE_IPV4_ADDRESS(192, 168, 10, 55)
 
/* IP address to bind the socket for any incoming interface. */
#define BIND_ADDRESS                    MAKE_IPV4_ADDRESS(0, 0, 0, 0)
 
/* UDP Port number. Change it to the required port number. */
#define UDP_PORT                          (50009)
 
/* Buffer size to store the incoming broadcast message. */
#define MAX_UDP_RECV_BUFFER_SIZE          (20)

#define IP_ADDR_BUFFER_SIZE                      (20u)

#define WIFI_INTERFACE_TYPE                  CY_WCM_INTERFACE_TYPE_AP

/* SoftAP Credentials: Modify SOFTAP_SSID and SOFTAP_PASSWORD as required */
#define SOFTAP_SSID                          "MY_SOFT_AP"
#define SOFTAP_PASSWORD                      "psoc1234"

/* Security type of the SoftAP. See 'cy_wcm_security_t' structure
    * in "cy_wcm.h" for more details.
    */
#define SOFTAP_SECURITY_TYPE                  CY_WCM_SECURITY_WPA2_AES_PSK

#define SOFTAP_IP_ADDRESS_COUNT               (2u)

#define SOFTAP_IP_ADDRESS                     MAKE_IPV4_ADDRESS(192, 168, 10, 1)
#define SOFTAP_NETMASK                        MAKE_IPV4_ADDRESS(255, 255, 255, 0)
#define SOFTAP_GATEWAY                        MAKE_IPV4_ADDRESS(192, 168, 10, 1)
#define SOFTAP_RADIO_CHANNEL                  (1u)


/*******************************************************************************
* Function Prototypes
********************************************************************************/
static cy_rslt_t create_server_socket(void);

static cy_rslt_t softap_start(void);

/*******************************************************************************
* Global Variables
********************************************************************************/
/* Secure socket variables. */
cy_socket_sockaddr_t broadcast_address, bind_address;
cy_socket_t server_handle, client_handle;

cy_socket_sockaddr_t broadcast_address = {
    .ip_address.ip.v4 = BROADCAST_ADDRESS,
    .ip_address.version = CY_SOCKET_IP_VER_V4,
    .port = UDP_PORT
};


/* Socket address to bind the socket to. */
cy_socket_sockaddr_t bind_address = {
    .ip_address.ip.v4 = BIND_ADDRESS,
    .ip_address.version = CY_SOCKET_IP_VER_V4,
    .port = UDP_PORT
};


/* Size of the peer socket address. */
uint32_t peer_addr_len;

/* Flags to track the LED state. */
bool led_state = CYBSP_LED_STATE_OFF;

/* Flag variable to check if TCP client is connected. */
bool client_connected;

/* Queue handler */
extern cy_queue_t led_command_q;


void udp_server_task(void *motor_data_msg_addr)
{
    ipc_msg_t *motor_data_msg = (ipc_msg_t*) motor_data_msg_addr;
    cy_rslt_t result;

    cy_wcm_config_t wifi_config = { .interface = WIFI_INTERFACE_TYPE };

    /* Variable to store number of bytes sent over TCP socket. */
    uint32_t bytes_sent = 0;

    /* Initialize Wi-Fi connection manager. */
    if (cy_wcm_init(&wifi_config) != CY_RSLT_SUCCESS) {
        PRINT("Wi-Fi Connection Manager initialization failed! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        CY_ASSERT(0);
    }
    PRINT("Wi-Fi Connection Manager initialized.\r\n");

    /* Start the Wi-Fi device as a Soft AP interface. */
    if (softap_start() != CY_RSLT_SUCCESS) {
        PRINT("Failed to Start Soft AP! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        CY_ASSERT(0);
    }

    /* Initialize secure socket library. */
    if (cy_socket_init() != CY_RSLT_SUCCESS) {
        PRINT("Secure Socket initialization failed! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        CY_ASSERT(0);
    }
    PRINT("Secure Socket initialized\n");

    if (create_server_socket() != CY_RSLT_SUCCESS) {
        PRINT("Failed to create socket! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        CY_ASSERT(0);
    }

    uint32_t my_buf[IPC_DATA_LENGTH] = {}; 
    
    while(true) {

        uint32_t broadcast_addr_size = sizeof(broadcast_address);
        vTaskSuspend(NULL);

        memcpy(my_buf, motor_data_msg->data, sizeof(my_buf));
        //PRINT("count: %08x\r\n", my_buf[0]);
        result = cy_socket_sendto(server_handle, my_buf, sizeof(my_buf),
            CY_SOCKET_FLAGS_NONE,
            &broadcast_address, sizeof(cy_socket_sockaddr_t), &bytes_sent);

        if (result != CY_RSLT_SUCCESS) {
            PRINT("Gulp..that wasn't suposed to happen!\r\n");
            PRINT("Error code: %x\r\n", result);
            cy_rtos_delay_milliseconds(1000);
        }


    }
}


static cy_rslt_t softap_start(void)
{
    cy_rslt_t result = CY_RSLT_SUCCESS;
    char ip_addr_str[IP_ADDR_BUFFER_SIZE];

    /* IP variable for network utility functions */
    cy_nw_ip_address_t nw_ip_addr =
    {
        .version = NW_IP_IPV4
    };

    /* Initialize the Wi-Fi device as a Soft AP. */
    cy_wcm_ap_credentials_t softap_credentials = {SOFTAP_SSID, SOFTAP_PASSWORD,
                                                  SOFTAP_SECURITY_TYPE};
    cy_wcm_ip_setting_t softap_ip_info = {
        .ip_address = {.version = CY_WCM_IP_VER_V4, .ip.v4 = SOFTAP_IP_ADDRESS},
        .gateway = {.version = CY_WCM_IP_VER_V4, .ip.v4 = SOFTAP_GATEWAY},
        .netmask = {.version = CY_WCM_IP_VER_V4, .ip.v4 = SOFTAP_NETMASK}};

    cy_wcm_ap_config_t softap_config = {softap_credentials, CY_WCM_WIFI_BAND_2_4GHZ,
        SOFTAP_RADIO_CHANNEL,
        softap_ip_info,
        NULL};

    /* Start the the Wi-Fi device as a Soft AP. */
    result = cy_wcm_start_ap(&softap_config);

    if(result == CY_RSLT_SUCCESS)
    {
        PRINT("Wi-Fi Device configured as Soft AP\n");
        PRINT("Connect TCP client device to the network: SSID: %s Password:%s\n",
                SOFTAP_SSID, SOFTAP_PASSWORD);
        nw_ip_addr.ip.v4 = softap_ip_info.ip_address.ip.v4;
        cy_nw_ntoa(&nw_ip_addr, ip_addr_str);
        PRINT("SofAP IP Address : %s\n\n", ip_addr_str);
    }

    return result;
}

static cy_rslt_t create_server_socket(void)
{
    cy_rslt_t result;

    uint8_t broadcast_enable = 1;

    /* Initialize the Secure Sockets Library. */
    result = cy_socket_init();
 
    result = cy_socket_create(CY_SOCKET_DOMAIN_AF_INET, CY_SOCKET_TYPE_DGRAM,
                              CY_SOCKET_IPPROTO_UDP, &server_handle);
    if(result != CY_RSLT_SUCCESS)
    {
        PRINT("Failed to create socket! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
        return result;
    }

    /*result = cy_socket_setsockopt(server_handle, CY_SOCKET_SOL_SOCKET,
        CY_SOCKET_SO_BROADCAST, (void*)(&broadcast_enable), sizeof(broadcast_enable));*/
    
    result = cy_socket_bind(server_handle, &bind_address, sizeof(bind_address));
    if(result != CY_RSLT_SUCCESS)
    {
        PRINT("Failed to bind to socket! Error code: 0x%08"PRIx32"\n", (uint32_t)result);
    }

    return result;
}