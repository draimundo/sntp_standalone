/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app.h"
#include "tcpip/tcpip.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.

    Application strings and buffers are be defined outside this structure.
 */

APP_DATA appData;
char printBuffer[64];
bool getSNTP = false;
void (*cbackUTC)(uint32_t) = NULL;
// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

/* TODO:  Add any necessary callback functions.
 */

// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************


/* TODO:  Add any necessary local functions.
 */


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize(void) {
    appData.state = APP_TCPIP_WAIT_INIT;
    appData.ntpAddr.Val = 0;
}

/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Tasks(void) {
    SYS_STATUS tcpipStat;
    static IPV4_ADDR dwLastIP[2] = {
        {-1},
        {-1}
    };
    IPV4_ADDR ipAddr;
    TCPIP_NET_HANDLE netH;
    int i, nNets;
    /* Check the application's current state. */
    switch (appData.state) {
        case APP_TCPIP_WAIT_INIT:
            tcpipStat = TCPIP_STACK_Status(sysObj.tcpip);
            if (tcpipStat < 0) { // some error occurred
                appData.state = APP_TCPIP_ERROR;
            } else if (tcpipStat == SYS_STATUS_READY) {
                appData.state = APP_TCPIP_WAIT_FOR_IP;
            }
            break;


        case APP_TCPIP_WAIT_FOR_IP:
            // if the IP address of an interface has changed
            // display the new value on the system console
            nNets = TCPIP_STACK_NumberOfNetworksGet();

            for (i = 0; i < nNets; i++) {
                netH = TCPIP_STACK_IndexToNet(i);
                if (!TCPIP_STACK_NetIsReady(netH)) {
                    return; // interface not ready yet!
                }
                ipAddr.Val = TCPIP_STACK_NetAddress(netH);
                if (dwLastIP[i].Val != ipAddr.Val) {
                    dwLastIP[i].Val = ipAddr.Val;
                }
            }

            TCPIP_DHCP_INFO pDhcpInfo;
            if (!TCPIP_DHCP_InfoGet(TCPIP_STACK_IndexToNet(0), &pDhcpInfo)) {
                return;
            }
            if (pDhcpInfo.ntpServersNo <= 0) {
                return;
            }
            appData.ntpAddr.Val = pDhcpInfo.ntpServers[0].Val;

            // all interfaces ready. Could start transactions!!!
            appData.state = APP_TCPIP_WAITING_FOR_CALL;
            break;

        case APP_TCPIP_WAITING_FOR_CALL:
        {
            if (getSNTP) {
                char ntpAddrStr[20];
                TCPIP_Helper_IPAddressToString(&appData.ntpAddr, ntpAddrStr, 20);
                if (TCPIP_SNTP_ConnectionParamSet(TCPIP_STACK_IndexToNet(0), IP_ADDRESS_TYPE_IPV4, ntpAddrStr) != SNTP_RES_OK) {
                    return; //retry later (busy)
                }
                if (TCPIP_SNTP_ConnectionInitiate() != SNTP_RES_OK) {
                    return; //retry later (connection already in progress)
                }
                uint32_t UTCSeconds, ms;
                TCPIP_SNTP_RESULT res;
                switch (res = TCPIP_SNTP_TimeGet(&UTCSeconds, &ms)) {
                    case SNTP_RES_OK:
                        break;
                    case SNTP_RES_TSTAMP_STALE:
                        //retry later (no recent timestamp)
                    case SNTP_RES_TSTAMP_ERROR:
                        //retry later (no available timestamp)
                        return; 
                    default:
                        break;
                }
                getSNTP = false;
                cbackUTC(UTCSeconds);

            }
        }
            break;

        case APP_TCPIP_ERROR:
            //TODO restart state machine?
            break;
            
        default:
            break;
    }
}

void registerUTCCallback(void (*pCbackUTC)(uint32_t)){
    cbackUTC = pCbackUTC;
}

void getTimeSNTP(){
    getSNTP = true;
}

/*******************************************************************************
 End of File
 */
