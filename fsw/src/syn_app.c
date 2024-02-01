/*******************************************************************************
** File: syn_app.c
**
** Purpose:
**   This file contains the source code for the SYN application.
**
*******************************************************************************/

/*
** Include Files
*/
#include <arpa/inet.h>
#include "syn_app.h"
#include "itc_synopsis_bridge.h"

/*
** Global Data
*/
SYN_AppData_t SYN_AppData;

void* memory;
size_t mem_req_bytes;
#define NUMDPS 8

const char dp_file_names[NUMDPS][20]; // = { {"asdp000000000"}, {"asdp000000001"}, {"asdp000000002"}, {"asdp000000003"}, {"asdp000000004"}, {"asdp000000005"}, {"asdp000000006"}};
float dp_file_sizes[NUMDPS]; 
int dp_size = 0;
#define MAX_DL_SIZE 100
#define MIN_DL_SIZE 0

double sigma = 1.34289567767;
float dl_size = 5.0;
int num_files_downlinked = 0;

int get_dp_index(char key[])
{
    for(int i = 0; i<NUMDPS; i++)
    {
        //printf("key: %s\n", key);
        //printf("NAME: %s\n", dp_file_names[i]);
        if(strcmp(dp_file_names[i], key) == 0)
        {
            return i;
        }
    }
    return -1;
}

void dp_insert(char key[], float value)
{
    int index = get_dp_index(key);
    if (index == -1) { // Key not found
        strcpy(dp_file_names[dp_size], key);
        dp_file_sizes[dp_size] = value;
        dp_size++;
    }
    else { // Key found
        dp_file_sizes[index] = value;
    }
}

float get_dp_size(char key[])
{
    int index = get_dp_index(key);
    if(index == -1)
    {
        return -1;
    }
    else
    {
        //printf("RETURNING: %f\n", dp_file_sizes[index]);
        return dp_file_sizes[index];
    }
}

/*
** Application entry point and main process loop
*/
void SYN_AppMain(void)
{
    int32 status = OS_SUCCESS;

    /*
    ** Create the first Performance Log entry
    */
    CFE_ES_PerfLogEntry(SYN_PERF_ID);

    /* 
    ** Perform application initialization
    */
    status = SYN_AppInit();
    if (status != CFE_SUCCESS)
    {
        SYN_AppData.RunStatus = CFE_ES_RunStatus_APP_ERROR;
    }

    /*
    ** Main loop
    */
    while (CFE_ES_RunLoop(&SYN_AppData.RunStatus) == true)
    {
        /*
        ** Performance log exit stamp
        */
        CFE_ES_PerfLogExit(SYN_PERF_ID);

        /* 
        ** Pend on the arrival of the next Software Bus message
        ** Note that this is the standard, but timeouts are available
        */
        status = CFE_SB_ReceiveBuffer((CFE_SB_Buffer_t **)&SYN_AppData.MsgPtr,  SYN_AppData.CmdPipe,  CFE_SB_PEND_FOREVER);
        
        /* 
        ** Begin performance metrics on anything after this line. This will help to determine
        ** where we are spending most of the time during this app execution.
        */
        CFE_ES_PerfLogEntry(SYN_PERF_ID);

        /*
        ** If the CFE_SB_ReceiveBuffer was successful, then continue to process the command packet
        ** If not, then exit the application in error.
        ** Note that a SB read error should not always result in an app quitting.
        */
        if (status == CFE_SUCCESS)
        {
            SYN_ProcessCommandPacket();
        }
        else
        {
            CFE_EVS_SendEvent(SYN_PIPE_ERR_EID, CFE_EVS_EventType_ERROR, "SYN: SB Pipe Read Error = %d", (int) status);
            SYN_AppData.RunStatus = CFE_ES_RunStatus_APP_ERROR;
        }
    }

    /*
    ** Disable component, which cleans up the interface, upon exit
    */
    SYN_Disable();

    /*
    ** Performance log exit stamp
    */
    CFE_ES_PerfLogExit(SYN_PERF_ID);

    /*
    ** Exit the application
    */
    CFE_ES_ExitApp(SYN_AppData.RunStatus);
} 


/* 
** Initialize application
*/
int32 SYN_AppInit(void)
{
    int32 status = OS_SUCCESS;
    
    SYN_AppData.RunStatus = CFE_ES_RunStatus_APP_RUN;

    /*
    ** Register the events
    */ 
    status = CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);    /* as default, no filters are used */
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("SYN: Error registering for event services: 0x%08X\n", (unsigned int) status);
       return status;
    }

    /*
    ** Create the Software Bus command pipe 
    */
    status = CFE_SB_CreatePipe(&SYN_AppData.CmdPipe, SYN_PIPE_DEPTH, "SYN_CMD_PIPE");
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(SYN_PIPE_ERR_EID, CFE_EVS_EventType_ERROR,
            "Error Creating SB Pipe,RC=0x%08X",(unsigned int) status);
       return status;
    }
    
    /*
    ** Subscribe to ground commands
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(SYN_CMD_MID), SYN_AppData.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(SYN_SUB_CMD_ERR_EID, CFE_EVS_EventType_ERROR,
            "Error Subscribing to HK Gnd Cmds, MID=0x%04X, RC=0x%08X",
            SYN_CMD_MID, (unsigned int) status);
        return status;
    }

    /*
    ** Subscribe to housekeeping (hk) message requests
    */
    status = CFE_SB_Subscribe(CFE_SB_ValueToMsgId(SYN_REQ_HK_MID), SYN_AppData.CmdPipe);
    if (status != CFE_SUCCESS)
    {
        CFE_EVS_SendEvent(SYN_SUB_REQ_HK_ERR_EID, CFE_EVS_EventType_ERROR,
            "Error Subscribing to HK Request, MID=0x%04X, RC=0x%08X",
            SYN_REQ_HK_MID, (unsigned int) status);
        return status;
    }

    /*
    ** TODO: Subscribe to any other messages here
    */


    /* 
    ** Initialize the published HK message - this HK message will contain the 
    ** telemetry that has been defined in the SYN_HkTelemetryPkt for this app.
    */
    CFE_MSG_Init(CFE_MSG_PTR(SYN_AppData.HkTelemetryPkt.TlmHeader),
                   CFE_SB_ValueToMsgId(SYN_HK_TLM_MID),
                   SYN_HK_TLM_LNGTH);

    /*
    ** Initialize the device packet message
    ** This packet is specific to your application
    */
    CFE_MSG_Init(CFE_MSG_PTR(SYN_AppData.DevicePkt.TlmHeader),
                   CFE_SB_ValueToMsgId(SYN_DEVICE_TLM_MID),
                   SYN_DEVICE_TLM_LNGTH);

    /*
    ** TODO: Initialize any other messages that this app will publish
    */

    /*
    ** ITC SYNOPSIS TESTING
    */
  

    // Determine memory requirements
    mem_req_bytes = 0;
    mem_req_bytes = itc_app_get_memory_requiremennt();

    
    // Set up Passthrough ASDS
    itc_setup_ptasds();

    // Initalize App
    memory = malloc(mem_req_bytes); 
    itc_app_init(mem_req_bytes, memory);    
    
    // Creating a map for DL sizes (just to speed things up)

    dp_insert("asdp000000000", 1.306);
    dp_insert("asdp000000001", 0.773);
    dp_insert("asdp000000002", 0.538);
    dp_insert("asdp000000003", 1.055);
    dp_insert("asdp000000004", 0.403);
    dp_insert("asdp000000005", 0.635);
    dp_insert("asdp000000006", 1.240);
    dp_insert("asdp000000007", 0.494);

    /* 
    ** Always reset all counters during application initialization 
    */
    SYN_ResetCounters();

    /*
    ** Initialize application data
    ** Note that counters are excluded as they were reset in the previous code block
    */
    SYN_AppData.HkTelemetryPkt.DeviceEnabled = SYN_DEVICE_DISABLED;
    SYN_AppData.HkTelemetryPkt.DeviceHK.DeviceCounter = 0;
    SYN_AppData.HkTelemetryPkt.DeviceHK.DeviceConfig = 0;
    SYN_AppData.HkTelemetryPkt.DeviceHK.DeviceStatus = 0;

    /* 
     ** Send an information event that the app has initialized. 
     ** This is useful for debugging the loading of individual applications.
     */
    status = CFE_EVS_SendEvent(SYN_STARTUP_INF_EID, CFE_EVS_EventType_INFORMATION,
               "SYN App Initialized. Version %d.%d.%d.%d",
                SYN_MAJOR_VERSION,
                SYN_MINOR_VERSION, 
                SYN_REVISION, 
                SYN_MISSION_REV);	
    if (status != CFE_SUCCESS)
    {
        CFE_ES_WriteToSysLog("SYN: Error sending initialization event: 0x%08X\n", (unsigned int) status);
    }
    return status;
} 


/* 
** Process packets received on the SYN command pipe
*/
void SYN_ProcessCommandPacket(void)
{
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_GetMsgId(SYN_AppData.MsgPtr, &MsgId);
    switch (CFE_SB_MsgIdToValue(MsgId))
    {
        /*
        ** Ground Commands with command codes fall under the SYN_CMD_MID (Message ID)
        */
        case SYN_CMD_MID:
            SYN_ProcessGroundCommand();
            break;

        /*
        ** All other messages, other than ground commands, add to this case statement.
        */
        case SYN_REQ_HK_MID:
            SYN_ProcessTelemetryRequest();
            break;

        /*
        ** All other invalid messages that this app doesn't recognize, 
        ** increment the command error counter and log as an error event.  
        */
        default:
            SYN_AppData.HkTelemetryPkt.CommandErrorCount++;
            CFE_EVS_SendEvent(SYN_PROCESS_CMD_ERR_EID,CFE_EVS_EventType_ERROR, "SYN: Invalid command packet, MID = 0x%x", CFE_SB_MsgIdToValue(MsgId));
            break;
    }
    return;
} 


/*
** Process ground commands
** TODO: Add additional commands required by the specific component
*/
void SYN_ProcessGroundCommand(void)
{
    int32 status = OS_SUCCESS;
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t CommandCode = 0;

    /*
    ** MsgId is only needed if the command code is not recognized. See default case
    */
    CFE_MSG_GetMsgId(SYN_AppData.MsgPtr, &MsgId);

    /*
    ** Ground Commands, by definition, have a command code (_CC) associated with them
    ** Pull this command code from the message and then process
    */
    CFE_MSG_GetFcnCode(SYN_AppData.MsgPtr, &CommandCode);
    switch (CommandCode)
    {
        /*
        ** NOOP Command
        */
        case SYN_NOOP_CC:
            /*
            ** First, verify the command length immediately after CC identification 
            ** Note that VerifyCmdLength handles the command and command error counters
            */
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                /* Second, send EVS event on successful receipt ground commands*/
                CFE_EVS_SendEvent(SYN_CMD_NOOP_INF_EID, CFE_EVS_EventType_INFORMATION, "SYN: NOOP command received");
                /* Third, do the desired command action if applicable, in the case of NOOP it is no operation */
            }
            break;

        /*
        ** Reset Counters Command
        */
        case SYN_RESET_COUNTERS_CC:
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                CFE_EVS_SendEvent(SYN_CMD_RESET_INF_EID, CFE_EVS_EventType_INFORMATION, "SYN: RESET counters command received");
                SYN_ResetCounters();
            }
            break;

        /*
        ** Enable Command
        */
        case SYN_ENABLE_CC:
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                CFE_EVS_SendEvent(SYN_CMD_ENABLE_INF_EID, CFE_EVS_EventType_INFORMATION, "SYN: Enable command received");
                SYN_Enable();
            }
            break;

        /*
        ** Disable Command
        */
        case SYN_DISABLE_CC:
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                CFE_EVS_SendEvent(SYN_CMD_DISABLE_INF_EID, CFE_EVS_EventType_INFORMATION, "SYN: Disable command received");
                SYN_Disable();
            }
            break;

        /*
        ** TODO: Edit and add more command codes as appropriate for the application
        ** Set Configuration Command
        ** Note that this is an example of a command that has additional arguments
        */
        case SYN_CONFIG_CC:
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_Config_cmd_t)) == OS_SUCCESS)
            {
                uint32_t config = ntohl(((SYN_Config_cmd_t*) SYN_AppData.MsgPtr)->DeviceCfg); // command is defined as big-endian... need to convert to host representation
                CFE_EVS_SendEvent(SYN_CMD_CONFIG_INF_EID, CFE_EVS_EventType_INFORMATION, "SYN: Configuration command received: %u", config);
                /* Command device to send HK */
                status = SYN_CommandDevice(&SYN_AppData.SynUart, SYN_DEVICE_CFG_CMD, config);
                if (status == OS_SUCCESS)
                {
                    SYN_AppData.HkTelemetryPkt.DeviceCount++;
                }
                else
                {
                    SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
                }
            }
            break;

        case SYN_AUTOMATE_CC:
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                CFE_EVS_SendEvent(SYN_CMD_AUTOMATE_EID, CFE_EVS_EventType_INFORMATION, "SYN: Config AUTOMATED ADD DATA command received");
                /* Command device to send HK */
                //status = SYN_APP_CommandDevice(SYN_APP_AppData.Syn_appUart.handle, SYN_APP_DEVICE_CFG_CMD, ((SYN_APP_Config_cmd_t*) SYN_APP_AppData.MsgPtr)->DeviceCfg);
                
                int syn_app_status;

                int dp_counter = get_dp_counter();
                while (dp_counter < NUMDPS)
                {
                    //printf("DP COUNTER: %d\n", dp_counter);
                    syn_app_status = owls_add_dpmsg();

                    if(syn_app_status == 0) //Success
                    {
                        
                        //printf("COUNTER: %d\n", dp_counter);
                        char sys_cp_cmd[512];
                        snprintf(sys_cp_cmd, sizeof(sys_cp_cmd), "cp -R %s%d %s", "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/assets/asdp00000000", dp_counter, "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/onboard");
                        //printf("COMMAND: %s\n", sys_cp_cmd);
                        system(sys_cp_cmd);
                        dp_counter = get_dp_counter();
                    }
                    else{
                        printf("*! SYN_APP:  Unable to add additional Automated DPMSG!\n");
                        break;
                    }
                    
                    SYN_AppData.HkTelemetryPkt.DeviceCount++;
                }
            }
            else
            {
                SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
            }            
            break;

        case SYN_ADD_DATA_CC:
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                CFE_EVS_SendEvent(SYN_CMD_ADD_DATA_EID, CFE_EVS_EventType_INFORMATION, "SYN: Config ADD DATA command received");
                /* Command device to send HK */
                //status = SYN_APP_CommandDevice(SYN_APP_AppData.Syn_appUart.handle, SYN_APP_DEVICE_CFG_CMD, ((SYN_APP_NoArgs_cmd_t*) SYN_APP_AppData.MsgPtr)->DeviceCfg);
            
                int syn_app_status;

                int dp_counter = get_dp_counter();

                syn_app_status = owls_add_dpmsg();

                if(syn_app_status == 0) //Success
                {
                    //printf("COUNTER: %d\n", dp_counter);
                    char sys_cp_cmd[512];
                    snprintf(sys_cp_cmd, sizeof(sys_cp_cmd), "OS_MV %s%d -> %s", "./data/owls/assets/asdp000000000/test.json", dp_counter, "./data/owls/onboard/");
                    //printf("COMMAND: %s\n", sys_cp_cmd);
                    OS_printf("ADDING DATA: %s\n", sys_cp_cmd);
                    
                    //syn_app_status = OS_mv("./data/owls/assets/asdp000000000/test.json", "./data/owls/onboard/test.json");
                    //OS_printf("STATUS: %d\n", syn_app_status);
                    
                    //system(sys_cp_cmd);
                }
                else
                {
                    OS_printf("*! SYN:  Unable to add additional DPMSG!\n");
                }
                SYN_AppData.HkTelemetryPkt.DeviceCount++;
            }
            else
            {
                SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
            }            
            break; 

        case SYN_CONFIG_DL_CC:
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_Config_cmd_t)) == OS_SUCCESS)
            {
                CFE_EVS_SendEvent(SYN_CMD_CONFIG_DL_EID, CFE_EVS_EventType_INFORMATION, "SYN: Config DL command received");

                /* Command device to send HK */
                uint32_t config = ntohl(((SYN_Config_cmd_t*) SYN_AppData.MsgPtr)->DeviceCfg);
                //status = SYN_CommandDevice(&SYN_AppData.SynUart.handle, SYN_DEVICE_CFG_CMD, config);
                status = SYN_CommandDevice(&SYN_AppData.SynUart, SYN_DEVICE_CFG_CMD, config);
                if (status == OS_SUCCESS)
                {
                    dl_size = ((((SYN_Config_cmd_t*) SYN_AppData.MsgPtr)->DeviceCfg) / 10.0);
                    //printf("CF SIZE: %d\n", (((SYN_Config_cmd_t*) SYN_APP_AppData.MsgPtr)->DeviceCfg));
                    printf("** SYNOPSIS DL SIZE SET: %f\n", dl_size);
                    SYN_AppData.HkTelemetryPkt.DeviceCount++;
                }
                else
                {
                    SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
                }
            }
            break;

        case SYN_CONFIG_ALPHA_CC:
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_Config_cmd_t)) == OS_SUCCESS)
            {
                CFE_EVS_SendEvent(SYN_CMD_CONFIG_ALPHA_EID, CFE_EVS_EventType_INFORMATION, "SYN: Config ALPHA command received");
                
                /* Command device to send HK */
                uint32_t config = ntohl(((SYN_Config_cmd_t*) SYN_AppData.MsgPtr)->DeviceCfg);
                //status = SYN_CommandDevice(&SYN_AppData.SynUart.handle, SYN_DEVICE_CFG_CMD, config);
                status = SYN_CommandDevice(&SYN_AppData.SynUart, SYN_DEVICE_CFG_CMD, config);
                if (status == OS_SUCCESS)
                {
                    sigma = ((((SYN_Config_cmd_t*) SYN_AppData.MsgPtr)->DeviceCfg) / 100.0);
                    //printf("CF SIZE: %d\n", (((SYN_APP_Config_cmd_t*) SYN_APP_AppData.MsgPtr)->DeviceCfg));
                    owls_set_sigma(sigma);
                    printf("** SYNOSIS ALPHA SET: %f\n", sigma);
                    SYN_AppData.HkTelemetryPkt.DeviceCount++;
                }
                else
                {
                    SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
                }
            }
            break;

        case SYN_RESET_CC:
            /*
            ** First, verify the command length immediately after CC identification 
            ** Note that VerifyCmdLength handles the command and command error counters
            */
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                printf("* RESET IN PROGRESS!\n");
                //SYN_APP_ResetCounters();
                dl_size = 5.0;
                num_files_downlinked = 0;
                char sys_rm_cmd[512];
                snprintf(sys_rm_cmd, sizeof(sys_rm_cmd), "rm -rf %s", "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/onboard/*");
                system(sys_rm_cmd);
                snprintf(sys_rm_cmd, sizeof(sys_rm_cmd), "rm -rf %s", "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/downlink/*");
                system(sys_rm_cmd); 

                /* Second, send EVS event on successful receipt ground commands*/
                CFE_EVS_SendEvent(SYN_CMD_RESET_DEMO_EID, CFE_EVS_EventType_INFORMATION, "SYN: MSG command received");
   
                itc_app_deinit(memory); 
                memory = malloc(mem_req_bytes);
               
                reset_dp_counter();

                char reset_db_cmd[512]; 
                snprintf(reset_db_cmd, sizeof(reset_db_cmd), "rm -rf %s", "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/owls_asdpdb_20230815_copy.db");                                        
                system(reset_db_cmd);
                snprintf(reset_db_cmd, sizeof(reset_db_cmd), "cp %s %s", "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/owls_asdpdb_20230815_copy1.db", "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/owls_asdpdb_20230815_copy.db");
                system(reset_db_cmd);

                itc_app_init(mem_req_bytes, memory);
            }
            break;

        case SYN_PRIO_CC:
            /*
            ** First, verify the command length immediately after CC identification 
            ** Note that VerifyCmdLength handles the command and command error counters
            */
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                /* Second, send EVS event on successful receipt ground commands*/
                CFE_EVS_SendEvent(SYN_CMD_PRIO_EID, CFE_EVS_EventType_INFORMATION, "SYN: PRIORITIZE command received");
                /* Third, do the desired command action if applicable, in the case of NOOP it is no operation */
                int syn_app_status;
                syn_app_status = owls_prioritize_data();
                //printf("Owls PRIO Return: %d\n", syn_app_status);
                printf("** PRIORITIZATION COMPLETE\n");
            }
            break;

        case SYN_GET_PDATA_CC:
            /*
            ** First, verify the command length immediately after CC identification 
            ** Note that VerifyCmdLength handles the command and command error counters
            */
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                /* Second, send EVS event on successful receipt ground commands*/
                CFE_EVS_SendEvent(SYN_CMD_GET_PDATA_EID, CFE_EVS_EventType_INFORMATION, "SYN: GET_P_DATA command received");
                /* Third, do the desired command action if applicable, in the case of NOOP it is no operation */
                int syn_app_status;
                float total_dl_size = 0.0;
                //printf("DL SIZE: %f\n", dl_size);
                printf("* DOWNLINKING IN PROGRESS\n");
                int total_dps_added = get_dp_counter();

                for(int i = 0; i < total_dps_added; i++)
                {
                    //printf("CURRENT LOOP (i): %d\n", i);
                    if(total_dl_size > dl_size) break;

                    char* test_string = owls_get_prioritized_data(i);
                    if (test_string != NULL )
                    {
                        //printf("Test String: %s\n", test_string);
                        char* cp_test_string = strdup(test_string);
                        //printf("SYN_APP: Downlink: %s\n", cp_test_string);
                        char* base_name = basename(cp_test_string);
                        //printf("BASENAME: %s\n", base_name);
                        char base_name_noext[256];
                        sscanf(base_name, "%[^.]", base_name_noext);

                        //printf("BASENAME_NOEXT: %s\n", base_name_noext);
                        float test_string_dp_size = get_dp_size(base_name_noext);
                        total_dl_size += test_string_dp_size;
                        //printf("TOTAL DL_SIZE: %f\n", total_dl_size);

                        if(total_dl_size <= dl_size)
                        {
                            // printf("Full String: %s\n", test_string);
                            // printf("Base Name: %s\n", base_name);
                            char destination_name[560];
                            char source_name[560];
                            char mv_command[560];

                            snprintf(destination_name, sizeof(destination_name), "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/downlink/%s", base_name_noext);  
                            snprintf(source_name, sizeof(source_name), "/home/nos3/Desktop/github-nos3/fsw/build/exe/cpu1/data/owls/onboard/%s", base_name_noext);
                            //printf("DESTINATION NAME: %s\n", destination_name);
                            //printf("FILE NAME: %s\n", base_name);
                            //printf("SOURCE NAME: %s\n", source_name);
                            //int32 status = rename(source_name, destination_name);
                            snprintf(mv_command, sizeof(mv_command), "mv %s %s", source_name, destination_name);
                            //printf("COMMAND: %s\n", mv_command);
                            int32 status = system(mv_command);                            
                            //printf("STATUS: %d\n", status);

                            //int32 status = OS_mv(source_name, destination_name);
                            owls_update_downlink_status(test_string);
                            
                            owls_destroy_prioritized_data_string(test_string);
                            
                        }
                        else
                        {
                            total_dl_size -= test_string_dp_size;
                            owls_destroy_prioritized_data_string(test_string);
                        }
                        //free(base_name);
                    }
                    else
                    {
                        printf("*! No Remaining Priority Data to Downlink\n");
                        break;
                    }
                }
                //printf("Re Prioritize Data Products");
                //syn_app_status = owls_prioritize_data();
                //printf("Owls PRIO Return: %d\n", syn_app_status); 
                printf("** DOWNLINKING COMPLETE - Wait for refresh\n");
            }
            break;

        case SYN_DISP_PDATA_CC:
            /*
            ** First, verify the command length immediately after CC identification 
            ** Note that VerifyCmdLength handles the command and command error counters
            */
            if (SYN_VerifyCmdLength(SYN_AppData.MsgPtr, sizeof(SYN_NoArgs_cmd_t)) == OS_SUCCESS)
            {
                /* Second, send EVS event on successful receipt ground commands*/
                CFE_EVS_SendEvent(SYN_CMD_DISP_PDATA_EID, CFE_EVS_EventType_INFORMATION, "SYN: DISPLAY PDATA command received");
                /* Third, do the desired command action if applicable, in the case of NOOP it is no operation */
                int syn_app_status;
                syn_app_status = owls_display_prioritized_data();
                //printf("Owls GET PRIO Return: %d\n", syn_app_status);
            }
            break;

        /*
        ** Invalid Command Codes
        */
        default:
            /* Increment the error counter upon receipt of an invalid command */
            SYN_AppData.HkTelemetryPkt.CommandErrorCount++;
            CFE_EVS_SendEvent(SYN_CMD_ERR_EID, CFE_EVS_EventType_ERROR, 
                "SYN: Invalid command code for packet, MID = 0x%x, cmdCode = 0x%x", CFE_SB_MsgIdToValue(MsgId), CommandCode);
            break;
    }
    return;
} 


/*
** Process Telemetry Request - Triggered in response to a telemetery request
** TODO: Add additional telemetry required by the specific component
*/
void SYN_ProcessTelemetryRequest(void)
{
    int32 status = OS_SUCCESS;
    CFE_SB_MsgId_t MsgId = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t CommandCode = 0;

    /* MsgId is only needed if the command code is not recognized. See default case */
    CFE_MSG_GetMsgId(SYN_AppData.MsgPtr, &MsgId);

    /* Pull this command code from the message and then process */
    CFE_MSG_GetFcnCode(SYN_AppData.MsgPtr, &CommandCode);
    switch (CommandCode)
    {
        case SYN_REQ_HK_TLM:
            SYN_ReportHousekeeping();
            break;

        case SYN_REQ_DATA_TLM:
            SYN_ReportDeviceTelemetry();
            break;

        /*
        ** Invalid Command Codes
        */
        default:
            /* Increment the error counter upon receipt of an invalid command */
            SYN_AppData.HkTelemetryPkt.CommandErrorCount++;
            CFE_EVS_SendEvent(SYN_DEVICE_TLM_ERR_EID, CFE_EVS_EventType_ERROR, 
                "SYN: Invalid command code for packet, MID = 0x%x, cmdCode = 0x%x", CFE_SB_MsgIdToValue(MsgId), CommandCode);
            break;
    }
    return;
}


/* 
** Report Application Housekeeping
*/
void SYN_ReportHousekeeping(void)
{
    int32 status = OS_SUCCESS;

    /* Check that device is enabled */
    if (SYN_AppData.HkTelemetryPkt.DeviceEnabled == SYN_DEVICE_ENABLED)
    {
        status = SYN_RequestHK(&SYN_AppData.SynUart, (SYN_Device_HK_tlm_t*) &SYN_AppData.HkTelemetryPkt.DeviceHK);
        if (status == OS_SUCCESS)
        {
            SYN_AppData.HkTelemetryPkt.DeviceCount++;
        }
        else
        {
            SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(SYN_REQ_HK_ERR_EID, CFE_EVS_EventType_ERROR, 
                    "SYN: Request device HK reported error %d", status);
        }
    }
    /* Intentionally do not report errors if disabled */

    /* Time stamp and publish housekeeping telemetry */
    CFE_SB_TimeStampMsg((CFE_MSG_Message_t *) &SYN_AppData.HkTelemetryPkt);
    CFE_SB_TransmitMsg((CFE_MSG_Message_t *) &SYN_AppData.HkTelemetryPkt, true);
    return;
}


/*
** Collect and Report Device Telemetry
*/
void SYN_ReportDeviceTelemetry(void)
{
    int32 status = OS_SUCCESS;

    /* Check that device is enabled */
    if (SYN_AppData.HkTelemetryPkt.DeviceEnabled == SYN_DEVICE_ENABLED)
    {
        status = SYN_RequestData(&SYN_AppData.SynUart, (SYN_Device_Data_tlm_t*) &SYN_AppData.DevicePkt.Syn);
        if (status == OS_SUCCESS)
        {
            SYN_AppData.HkTelemetryPkt.DeviceCount++;
            /* Time stamp and publish data telemetry */
            CFE_SB_TimeStampMsg((CFE_MSG_Message_t *) &SYN_AppData.DevicePkt);
            CFE_SB_TransmitMsg((CFE_MSG_Message_t *) &SYN_AppData.DevicePkt, true);
        }
        else
        {
            SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(SYN_REQ_DATA_ERR_EID, CFE_EVS_EventType_ERROR, 
                    "SYN: Request device data reported error %d", status);
        }
    }
    /* Intentionally do not report errors if disabled */
    return;
}


/*
** Reset all global counter variables
*/
void SYN_ResetCounters(void)
{
    SYN_AppData.HkTelemetryPkt.CommandErrorCount = 0;
    SYN_AppData.HkTelemetryPkt.CommandCount = 0;
    SYN_AppData.HkTelemetryPkt.DeviceErrorCount = 0;
    SYN_AppData.HkTelemetryPkt.DeviceCount = 0;
    return;
} 


/*
** Enable Component
** TODO: Edit for your specific component implementation
*/
void SYN_Enable(void)
{
    int32 status = OS_SUCCESS;

    /* Check that device is disabled */
    if (SYN_AppData.HkTelemetryPkt.DeviceEnabled == SYN_DEVICE_DISABLED)
    {
        /*
        ** Initialize hardware interface data
        ** TODO: Make specific to your application depending on protocol in use
        ** Note that other components provide examples for the different protocols available
        */ 
        SYN_AppData.SynUart.deviceString = SYN_CFG_STRING;
        SYN_AppData.SynUart.handle = SYN_CFG_HANDLE;
        SYN_AppData.SynUart.isOpen = PORT_CLOSED;
        SYN_AppData.SynUart.baud = SYN_CFG_BAUDRATE_HZ;
        SYN_AppData.SynUart.access_option = uart_access_flag_RDWR;

        /* Open device specific protocols */
        status = uart_init_port(&SYN_AppData.SynUart);
        if (status == OS_SUCCESS)
        {
            SYN_AppData.HkTelemetryPkt.DeviceCount++;
            SYN_AppData.HkTelemetryPkt.DeviceEnabled = SYN_DEVICE_ENABLED;
            CFE_EVS_SendEvent(SYN_ENABLE_INF_EID, CFE_EVS_EventType_INFORMATION, "SYN: Device enabled");
        }
        else
        {
            SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(SYN_UART_INIT_ERR_EID, CFE_EVS_EventType_ERROR, "SYN: UART port initialization error %d", status);
        }
    }
    else
    {
        SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
        CFE_EVS_SendEvent(SYN_ENABLE_ERR_EID, CFE_EVS_EventType_ERROR, "SYN: Device enable failed, already enabled");
    }
    return;
}


/*
** Disable Component
** TODO: Edit for your specific component implementation
*/
void SYN_Disable(void)
{
    int32 status = OS_SUCCESS;

    /* Check that device is enabled */
    if (SYN_AppData.HkTelemetryPkt.DeviceEnabled == SYN_DEVICE_ENABLED)
    {
        /* Open device specific protocols */
        status = uart_close_port(&SYN_AppData.SynUart);
        if (status == OS_SUCCESS)
        {
            SYN_AppData.HkTelemetryPkt.DeviceCount++;
            SYN_AppData.HkTelemetryPkt.DeviceEnabled = SYN_DEVICE_DISABLED;
            CFE_EVS_SendEvent(SYN_DISABLE_INF_EID, CFE_EVS_EventType_INFORMATION, "SYN: Device disabled");
        }
        else
        {
            SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
            CFE_EVS_SendEvent(SYN_UART_CLOSE_ERR_EID, CFE_EVS_EventType_ERROR, "SYN: UART port close error %d", status);
        }
    }
    else
    {
        SYN_AppData.HkTelemetryPkt.DeviceErrorCount++;
        CFE_EVS_SendEvent(SYN_DISABLE_ERR_EID, CFE_EVS_EventType_ERROR, "SYN: Device disable failed, already disabled");
    }
    return;
}


/*
** Verify command packet length matches expected
*/
int32 SYN_VerifyCmdLength(CFE_MSG_Message_t * msg, uint16 expected_length)
{     
    int32 status = OS_SUCCESS;
    CFE_SB_MsgId_t msg_id = CFE_SB_INVALID_MSG_ID;
    CFE_MSG_FcnCode_t cmd_code = 0;
    size_t actual_length = 0;

    CFE_MSG_GetSize(msg, &actual_length);
    if (expected_length == actual_length)
    {
        /* Increment the command counter upon receipt of an invalid command */
        SYN_AppData.HkTelemetryPkt.CommandCount++;
    }
    else
    {
        CFE_MSG_GetMsgId(msg, &msg_id);
        CFE_MSG_GetFcnCode(msg, &cmd_code);

        CFE_EVS_SendEvent(SYN_LEN_ERR_EID, CFE_EVS_EventType_ERROR,
           "Invalid msg length: ID = 0x%X,  CC = %d, Len = %d, Expected = %d",
              CFE_SB_MsgIdToValue(msg_id), cmd_code, actual_length, expected_length);

        status = OS_ERROR;

        /* Increment the command error counter upon receipt of an invalid command */
        SYN_AppData.HkTelemetryPkt.CommandErrorCount++;
    }
    return status;
} 


