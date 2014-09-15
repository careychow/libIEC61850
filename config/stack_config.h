/*
 * config.h
 *
 * Contains global defines to configure the stack. You need to recompile the stack to make
 * changes effective (make clean; make)
 *
 */

#ifndef STACK_CONFIG_H_
#define STACK_CONFIG_H_

/* include asserts if set to 1 */
#define DEBUG 0

/* print debugging information with printf if set to 1 */
#define DEBUG_SOCKET 0
#define DEBUG_COTP 0
#define DEBUG_ISO_SERVER 0
#define DEBUG_ISO_CLIENT 0
#define DEBUG_IED_SERVER 0
#define DEBUG_IED_CLIENT 0
#define DEBUG_MMS_CLIENT 0
#define DEBUG_MMS_SERVER 0

/* Maximum MMS PDU SIZE - default is 65000 */
#define CONFIG_MMS_MAXIMUM_PDU_SIZE 65000

/*
 * Enable single threaded mode
 *
 * 1 ==> server runs in single threaded mode (a single thread for the server and all client connections)
 * 0 ==> server runs in multi-threaded mode (one thread for each connection and
 * one server background thread )
 */
#define CONFIG_MMS_SINGLE_THREADED 1

/*
 * Optimize stack for threadless operation - don't use semaphores
 *
 * WARNING: If set to 1 normal single- and multi-threaded server are no longer working!
 */
#define CONFIG_MMS_THREADLESS_STACK 0

/* number of concurrent MMS client connections the server accepts, -1 for no limit */
#define CONFIG_MAXIMUM_TCP_CLIENT_CONNECTIONS 5

/* activate TCP keep alive mechanism. 1 -> activate */
#define CONFIG_ACTIVATE_TCP_KEEPALIVE 1

/* time (in s) between last message and first keepalive message */
#define CONFIG_TCP_KEEPALIVE_IDLE 5

/* time between subsequent keepalive messages if no ack received */
#define CONFIG_TCP_KEEPALIVE_INTERVAL 2

/* number of not missing keepalive responses until socket is considered dead */
#define CONFIG_TCP_KEEPALIVE_CNT 2

/* maximum COTP (ISO 8073) TPDU size - valid range is 1024 - 8192 */
#define CONFIG_COTP_MAX_TPDU_SIZE 8192

/* timeout while reading from TCP stream in ms */
#define CONFIG_TCP_READ_TIMEOUT_MS 1000

/* Ethernet interface ID for GOOSE and SV */
#define CONFIG_ETHERNET_INTERFACE_ID "eth0"
//#define CONFIG_ETHERNET_INTERFACE_ID "vboxnet0"
//#define CONFIG_ETHERNET_INTERFACE_ID "en0"  // OS X uses enX in place of ethX as ethernet NIC names.

/* Set to 1 to include GOOSE support in the build. Otherwise set to 0 */
#define CONFIG_INCLUDE_GOOSE_SUPPORT 1

#ifdef _WIN32

/* GOOSE will be disabled for Windows if ethernet support (winpcap) is not available */
#ifdef EXCLUDE_ETHERNET_WINDOWS
#ifdef CONFIG_INCLUDE_GOOSE_SUPPORT
#undef CONFIG_INCLUDE_GOOSE_SUPPORT
#endif
#define CONFIG_INCLUDE_GOOSE_SUPPORT 0
#define CONFIG_INCUDE_ETHERNET_WINDOWS 0
#else
#define CONFIG_INCLUDE_ETHERNET_WINDOWS 1
#undef CONFIG_ETHERNET_INTERFACE_ID
#define CONFIG_ETHERNET_INTERFACE_ID "0"
#endif
#endif

/* The GOOSE retransmission interval in ms for the stable condition - i.e. no monitored value changed */
#define CONFIG_GOOSE_STABLE_STATE_TRANSMISSION_INTERVAL 5000

/* The GOOSE retransmission interval in ms in the case an event happens. */
#define CONFIG_GOOSE_EVENT_RETRANSMISSION_INTERVAL 500

/* The number of GOOSE retransmissions after an event */
#define CONFIG_GOOSE_EVENT_RETRANSMISSION_COUNT 2

/* The default value for the priority field of the 802.1Q header (allowed range 0-7) */
#define CONFIG_GOOSE_DEFAULT_PRIORITY 4

/* The default value for the VLAN ID field of the 802.1Q header - the allowed range is 2-4096 or 0 if VLAN/priority is not used */
#define CONFIG_GOOSE_DEFAULT_VLAN_ID 0

/* Configure the 16 bit APPID field in the GOOSE header */
#define CONFIG_GOOSE_DEFAULT_APPID 0x1000

/* Default destination MAC address for GOOSE */
#define CONFIG_GOOSE_DEFAULT_DST_ADDRESS {0x01, 0x0c, 0xcd, 0x01, 0x00, 0x01}

/* include support for IEC 61850 control services */
#define CONFIG_IEC61850_CONTROL_SERVICE 1

/* The default select timeout in ms. This will apply only if no sboTimeout attribute exists for a control object. Set to 0 for no timeout. */
#define CONFIG_CONTROL_DEFAULT_SBO_TIMEOUT 15000

/* include support for IEC 61850 reporting services */
#define CONFIG_IEC61850_REPORT_SERVICE 1

/* The default buffer size of buffered RCBs in bytes */
#define CONFIG_REPORTING_DEFAULT_REPORT_BUFFER_SIZE 65536

/* default results for MMS identify service */
#define CONFIG_DEFAULT_MMS_VENDOR_NAME "libiec61850.com"
#define CONFIG_DEFAULT_MMS_MODEL_NAME "LIBIEC61850"
#define CONFIG_DEFAULT_MMS_REVISION "0.8.0"

/* MMS virtual file store base path - where file services are looking for files */
#define CONFIG_VIRTUAL_FILESTORE_BASEPATH "./vmd-filestore/"

/* Maximum number of open file per MMS connection (for MMS file read service) */
#define CONFIG_MMS_MAX_NUMBER_OF_OPEN_FILES_PER_CONNECTION 5

/* Definition of supported services */
#define MMS_DEFAULT_PROFILE 1

#if (MMS_DEFAULT_PROFILE == 1)
#define MMS_READ_SERVICE 1
#define MMS_WRITE_SERVICE 1
#define MMS_GET_NAME_LIST 1
#define MMS_GET_VARIABLE_ACCESS_ATTRIBUTES 1
#define MMS_DATA_SET_SERVICE 1
#define MMS_DYNAMIC_DATA_SETS 1
#define MMS_GET_DATA_SET_ATTRIBUTES 1
#define MMS_STATUS_SERVICE 1
#define MMS_IDENTIFY_SERVICE 1
#define MMS_FILE_SERVICE 1
#endif /* MMS_DEFAULT_PROFILE */

#if (MMS_WRITE_SERVICE != 1)
#undef CONFIG_IEC61850_CONTROL_SERVICE
#define CONFIG_IEC61850_CONTROL_SERVICE 0
#endif

/* support flatted named variable name space required by IEC 61850-8-1 MMS mapping */
#define CONFIG_MMS_SUPPORT_FLATTED_NAME_SPACE 1

/* VMD scope named variables are not used by IEC 61850 */
#define CONFIG_MMS_SUPPORT_VMD_SCOPE_NAMED_VARIABLES 0

#endif /* STACK_CONFIG_H_ */
