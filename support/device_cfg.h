#ifndef _SYN_CHECKOUT_DEVICE_CFG_H_
#define _SYN_CHECKOUT_DEVICE_CFG_H_

/*
** SYN Checkout Configuration
*/
#define SYN_CFG
/* Note: NOS3 uart requires matching handle and bus number */
#define SYN_CFG_STRING           "/dev/usart_29"
#define SYN_CFG_HANDLE           29 
#define SYN_CFG_BAUDRATE_HZ      115200
#define SYN_CFG_MS_TIMEOUT       250
#define SYN_CFG_DEBUG

#endif /* _SYN_CHECKOUT_DEVICE_CFG_H_ */
