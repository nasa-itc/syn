/*******************************************************************************
** File: syn_device.h
**
** Purpose:
**   This is the header file for the SYN device.
**
*******************************************************************************/
#ifndef _SYN_DEVICE_H_
#define _SYN_DEVICE_H_

/*
** Required header files.
*/
#include "device_cfg.h"
#include "hwlib.h"
#include "syn_platform_cfg.h"


/*
** Type definitions
** TODO: Make specific to your application
*/
#define SYN_DEVICE_HDR              0xDEAD
#define SYN_DEVICE_HDR_0            0xDE
#define SYN_DEVICE_HDR_1            0xAD

#define SYN_DEVICE_NOOP_CMD         0x00
#define SYN_DEVICE_REQ_HK_CMD       0x01
#define SYN_DEVICE_REQ_DATA_CMD     0x02
#define SYN_DEVICE_CFG_CMD          0x03

#define SYN_DEVICE_TRAILER          0xBEEF
#define SYN_DEVICE_TRAILER_0        0xBE
#define SYN_DEVICE_TRAILER_1        0xEF

#define SYN_DEVICE_HDR_TRL_LEN      4
#define SYN_DEVICE_CMD_SIZE         9

/*
** SYN device housekeeping telemetry definition
*/
typedef struct
{
    uint32_t  DeviceCounter;
    uint32_t  DeviceConfig;
    uint32_t  DeviceStatus;

} __attribute__((packed)) SYN_Device_HK_tlm_t;
#define SYN_DEVICE_HK_LNGTH sizeof ( SYN_Device_HK_tlm_t )
#define SYN_DEVICE_HK_SIZE SYN_DEVICE_HK_LNGTH + SYN_DEVICE_HDR_TRL_LEN


/*
** SYN device data telemetry definition
*/
typedef struct
{
    uint32_t  DeviceCounter;
    uint16_t  DeviceDataX;
    uint16_t  DeviceDataY;
    uint16_t  DeviceDataZ;

} __attribute__((packed)) SYN_Device_Data_tlm_t;
#define SYN_DEVICE_DATA_LNGTH sizeof ( SYN_Device_Data_tlm_t )
#define SYN_DEVICE_DATA_SIZE SYN_DEVICE_DATA_LNGTH + SYN_DEVICE_HDR_TRL_LEN


/*
** Prototypes
*/
int32_t SYN_ReadData(uart_info_t* device, uint8_t* read_data, uint8_t data_length);
int32_t SYN_CommandDevice(uart_info_t* device, uint8_t cmd, uint32_t payload);
int32_t SYN_RequestHK(uart_info_t* device, SYN_Device_HK_tlm_t* data);
int32_t SYN_RequestData(uart_info_t* device, SYN_Device_Data_tlm_t* data);


#endif /* _SYN_DEVICE_H_ */
