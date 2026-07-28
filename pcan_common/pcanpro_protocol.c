#include "pcanpro_protocol.h"
#include "pcan_usb.h"
#include "pcan_usbpro_fw.h"
#include "pcanpro_can.h"
#include "pcanpro_led.h"
#include "pcanpro_timestamp.h"
#include "pcanpro_usbd.h"
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CAN_CHANNEL_MAX (2)

#define PCAN_USBPRO_RTR 0x01
#define PCAN_USBPRO_EXT 0x02
#define PCAN_USBPRO_SR 0x80

/* PCAN-USB-PRO status flags */
#define PCAN_USBPRO_BUS_HEAVY 0x01
#define PCAN_USBPRO_BUS_OVERRUN 0x0c

static struct {
  struct {
    /* error handling related */
    uint8_t err;
    uint8_t ecc;
    uint8_t rx_err;
    uint8_t tx_err;
    /* config */
    uint8_t silient;
    uint8_t bus_active;
    uint8_t loopback;
    uint8_t err_mask;

    uint32_t ccbt; /* current baudrate value */
    uint32_t channel_nr;

    uint8_t led_is_busy;
  } can[CAN_CHANNEL_MAX];

  uint32_t device_nr;
  uint32_t time_calibration_mode;
  uint32_t last_time_sync;
  uint32_t last_flush;
} pcan_device = {
    .device_nr = 0xFFFFFFFF,

    .can[0] = {.channel_nr = 0xFFFFFFFF},
    .can[1] = {.channel_nr = 0xFFFFFFFF},
};

#define PCAN_USBFD_TYPE_EXT 2

struct pcan_ufd_fw_info {
  uint16_t size_of;
  uint16_t type;
  uint8_t hw_type;
  uint8_t bl_version[3];
  uint8_t hw_version;
  uint8_t fw_version[3];
  uint64_t dev_id;
  uint32_t ser_no;
  uint32_t flags;
  uint8_t cmd_out_ep;
  uint8_t cmd_in_ep;
  uint16_t data_out_ep;
  uint8_t data_in_ep;
  uint8_t dummy[3];
} __attribute__((packed));

/* Global Info Templates in normal memory */
static struct pcan_usbpro_bootloader_info bi_info
    __attribute__((aligned(4))) = {
        .ctrl_type = BOOTLOADER_INFO_STRUCT_TYPE,
        .version = {0, 0, 0, 0},
        .day = 1,
        .month = 1,
        .year = 26,
        .dummy = 0,
        .serial_num_high = 200030,
        .serial_num_low = 1,
        .hw_type = 0,
        .hw_rev = 0,
};

static struct pcan_ufd_fw_info fw_info __attribute__((aligned(4))) = {
    .size_of = 36,
    .type = PCAN_USBFD_TYPE_EXT,
    .hw_type = 0,
    .bl_version = {1, 0, 0},
    .hw_version = 1,
    .fw_version = {1, 3, 3},
    .dev_id = 0,
    .ser_no = PCAN_SERIAL_NUMBER,
    .flags = 0,
    .cmd_out_ep = 0x03,
    .cmd_in_ep = 0x83,
    .data_out_ep = 0x01 | (0x02 << 8), // EP1 and EP2
    .data_in_ep = 0x81,                
    .dummy = {0, 0, 0},
};

static struct pcan_usbpro_uc_chipid uc_chid_info __attribute__((aligned(4))) = {
    .ctrl_type = uC_CHIPID_STRUCT_TYPE,
    .chip_id = 0,
};

static struct pcan_usbpro_usb_chipid usb_chid_info
    __attribute__((aligned(4))) = {
        .ctrl_type = USB_CHIPID_STRUCT_TYPE,
        .chip_id = 0,
};

static struct pcan_usbpro_device_nr dev_nr_info __attribute__((aligned(4))) = {
    .ctrl_type = DEVICE_NR_STRUCT_TYPE,
    .device_nr = PCAN_DEVICE_NUMBER,
};

static struct pcan_usbpro_cpld_info cpld_info __attribute__((aligned(4))) = {
    .ctrl_type = CPLD_INFO_STRUCT_TYPE,
    .cpld_nr = 0,
};

static struct pcan_usbpro_info_mode info_mode_data
    __attribute__((aligned(4))) = {.ctrl_type = 0};

static struct pcan_usbpro_time_mode time_mode_info
    __attribute__((aligned(4))) = {.ctrl_type = 0};

/* internal structure used to handle messages sent to bulk urb */
struct pcan_usbpro_msg {
  uint8_t *rec_ptr;
  int rec_buffer_size;
  int rec_buffer_len;
  union {
    uint16_t *rec_counter_read;
    uint32_t *rec_counter;
    uint8_t *rec_buffer;
  } u;
};

#define PCAN_USB_DATA_BUFFER_SIZE 1024
static uint8_t resp_buffer[2][PCAN_USB_DATA_BUFFER_SIZE];
static uint8_t temp_resp_buffer[2][PCAN_USB_DATA_BUFFER_SIZE];
static uint8_t drv_load_packet[16];
static struct pcan_usbpro_msg resp[2];

static struct t_m2h_fsm resp_fsm[2] = {
    [0] =
        {
            .state = 0,
            .ep_addr = PCAN_USB_EP_CMDIN,
            .pdbuf = temp_resp_buffer[0],
            .dbsize = PCAN_USB_DATA_BUFFER_SIZE,
        },
    [1] = {
        .state = 0,
        .ep_addr = PCAN_USB_EP_MSGIN_CH1,
        .pdbuf = temp_resp_buffer[1],
        .dbsize = PCAN_USB_DATA_BUFFER_SIZE,
    }};

int pcan_protocol_can_rx_ready(void) {
  return 1;
}

/* low level requests */
#if defined(ESP_PLATFORM)
bool pcan_protocol_device_setup(tusb_control_request_t const * req) {
#else
uint8_t pcan_protocol_device_setup(USBD_HandleTypeDef *pdev,
                                   USBD_SetupReqTypedef *req) {
#endif
  switch (req->bRequest) {
  case USB_VENDOR_REQUEST_INFO:
#if defined(ESP_PLATFORM)
    printf("PCAN: Info Req 0x%04X\n", req->wValue);
#else
    printf("PCAN: Info Req 0x%04X\r\n", req->wValue);
#endif
    switch (req->wValue) {
    case USB_VENDOR_REQUEST_wVALUE_INFO_BOOTLOADER:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, (void *)&bi_info, sizeof(bi_info));
#else
      return USBD_CtlSendData(pdev, (void *)&bi_info, sizeof(bi_info));
#endif
    case USB_VENDOR_REQUEST_wVALUE_INFO_FIRMWARE:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, (void *)&fw_info, sizeof(fw_info));
#else
      return USBD_CtlSendData(pdev, (void *)&fw_info, sizeof(fw_info));
#endif
    case USB_VENDOR_REQUEST_wVALUE_INFO_uC_CHIPID:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, (void *)&uc_chid_info, sizeof(uc_chid_info));
#else
      return USBD_CtlSendData(pdev, (void *)&uc_chid_info, sizeof(uc_chid_info));
#endif
    case USB_VENDOR_REQUEST_wVALUE_INFO_USB_CHIPID:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, (void *)&usb_chid_info, sizeof(usb_chid_info));
#else
      return USBD_CtlSendData(pdev, (void *)&usb_chid_info, sizeof(usb_chid_info));
#endif
    case USB_VENDOR_REQUEST_wVALUE_INFO_DEVICENR:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, (void *)&dev_nr_info, sizeof(dev_nr_info));
#else
      return USBD_CtlSendData(pdev, (void *)&dev_nr_info, sizeof(dev_nr_info));
#endif
    case USB_VENDOR_REQUEST_wVALUE_INFO_CPLD:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, (void *)&cpld_info, sizeof(cpld_info));
#else
      return USBD_CtlSendData(pdev, (void *)&cpld_info, sizeof(cpld_info));
#endif
    case USB_VENDOR_REQUEST_wVALUE_INFO_MODE:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, (void *)&info_mode_data, sizeof(info_mode_data));
#else
      return USBD_CtlSendData(pdev, (void *)&info_mode_data, sizeof(info_mode_data));
#endif
    case USB_VENDOR_REQUEST_wVALUE_INFO_TIMEMODE:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, (void *)&time_mode_info, sizeof(time_mode_info));
#else
      return USBD_CtlSendData(pdev, (void *)&time_mode_info, sizeof(time_mode_info));
#endif
    default:
#if defined(ESP_PLATFORM)
      return false;
#else
      return USBD_FAIL;
#endif
    }
    break;

  case USB_VENDOR_REQUEST_FKT:
    switch (req->wValue) {
    case USB_VENDOR_REQUEST_wVALUE_SETFKT_INTERFACE_DRIVER_LOADED:
#if defined(ESP_PLATFORM)
      return tud_control_xfer(0, req, drv_load_packet, req->wLength);
#else
      USBD_CtlPrepareRx(pdev, drv_load_packet, 16);
      return USBD_OK;
#endif
    default:
#if defined(ESP_PLATFORM)
      return tud_control_status(0, req);
#else
      return USBD_OK; // Silently accept other vendor functions
#endif
    }
    break;
  default:
#if defined(ESP_PLATFORM)
    return false;
#else
    USBD_CtlError(pdev, req);
    return USBD_FAIL;
#endif
  }
}

void pcan_ep0_receive(void) {
  printf("PCAN: Driver Loaded (Type=%d)\r\n", drv_load_packet[0]);
  if (drv_load_packet[0] == 0) {
    pcan_flush_ep(PCAN_USB_EP_MSGIN_CH1);
    pcan_flush_ep(PCAN_USB_EP_CMDIN);
    pcan_led_set_mode(LED_STAT, LED_MODE_BLINK_SLOW, 0xFFFFFFFF);
  }
}

static int pcan_usbpro_sizeof_rec(uint8_t data_type) {
  switch (data_type) {
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_8: return sizeof(struct pcan_usbpro_canmsg_rx);
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_4: return sizeof(struct pcan_usbpro_canmsg_rx) - 4;
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_0:
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_RTR_RX: return sizeof(struct pcan_usbpro_canmsg_rx) - 8;
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_STATUS_ERROR_RX: return sizeof(struct pcan_usbpro_canmsg_status_error_rx);
  case DATA_TYPE_USB2CAN_STRUCT_CALIBRATION_TIMESTAMP_RX: return sizeof(struct pcan_usbpro_calibration_ts_rx);
  case DATA_TYPE_USB2CAN_STRUCT_BUSLAST_RX: return sizeof(struct pcan_usbpro_buslast_rx);
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_8: return sizeof(struct pcan_usbpro_canmsg_tx);
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_4: return sizeof(struct pcan_usbpro_canmsg_tx) - 4;
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_0: return sizeof(struct pcan_usbpro_canmsg_tx) - 8;
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETBAUDRATE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETBAUDRATE: return sizeof(struct pcan_usbpro_baudrate);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETCANBUSACTIVATE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETCANBUSACTIVATE: return sizeof(struct pcan_usbpro_bus_activity);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETSILENTMODE: return sizeof(struct pcan_usbpro_silent_mode);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETDEVICENR:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETDEVICENR: return sizeof(struct pcan_usbpro_dev_nr);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETWARNINGLIMIT: return sizeof(struct pcan_usbpro_warning_limit);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETLOOKUP_EXPLICIT: return sizeof(struct pcan_usbpro_lookup_explicit);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETLOOKUP_GROUP: return sizeof(struct pcan_usbpro_lookup_group);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETFILTERMODE: return sizeof(struct pcan_usbpro_filter_mode);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETRESET_MODE: return sizeof(struct pcan_usbpro_reset_mode);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETERRORFRAME: return sizeof(struct pcan_usbpro_error_frame);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETCANBUS_ERROR_STATUS: return sizeof(struct pcan_usbpro_error_status);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETREGISTER: return sizeof(struct pcan_usbpro_set_register);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETREGISTER: return sizeof(struct pcan_usbpro_get_register);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETGET_CALIBRATION_MSG: return sizeof(struct pcan_usbpro_calibration);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETGET_BUSLAST_MSG: return sizeof(struct pcan_usbpro_buslast);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETSTRING: return sizeof(struct pcan_usbpro_set_string);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETSTRING: return sizeof(struct pcan_usbpro_get_string);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_STRING: return sizeof(struct pcan_usbpro_string);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SAVEEEPROM: return sizeof(struct pcan_usbpro_save_eeprom);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_USB_IN_PACKET_DELAY: return sizeof(struct pcan_usbpro_packet_delay);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_TIMESTAMP_PARAM: return sizeof(struct pcan_usbpro_timestamp_param);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_ERROR_GEN_ID: return sizeof(struct pcan_usbpro_error_gen_id);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_ERROR_GEN_NOW: return sizeof(struct pcan_usbpro_error_gen_now);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SET_SOFTFILER: return sizeof(struct pcan_usbpro_softfiler);
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SET_CANLED: return sizeof(struct pcan_usbpro_set_can_led);
  /* Protocol extensions often sent by modern PCAN drivers */
  // case 0x0C: return 12; // Filter Std
  // case 0x0D: return 12; // Filter Ext
  default:
    printf("PCAN: Unknown Rec ID 0x%02X\r\n", data_type);
    return -1;
  }
}

static uint8_t *pcan_usbpro_msg_init(struct pcan_usbpro_msg *pm,
                                     void *buffer_addr, int buffer_size) {
  if (buffer_size < 4) return NULL;
  pm->u.rec_buffer = (uint8_t *)buffer_addr;
  pm->rec_buffer_size = pm->rec_buffer_len = buffer_size;
  pm->rec_ptr = pm->u.rec_buffer + 4;
  return pm->rec_ptr;
}

static uint8_t *pcan_usbpro_msg_init_empty(struct pcan_usbpro_msg *pm,
                                           void *buffer_addr, int buffer_size) {
  uint8_t *pr = pcan_usbpro_msg_init(pm, buffer_addr, buffer_size);
  if (pr) {
    pm->rec_buffer_len = 4;
    *pm->u.rec_counter = 0;
  }
  return pr;
}

static void pcan_usbpro_msg_reset(struct pcan_usbpro_msg *pm) {
  pm->rec_ptr = pm->u.rec_buffer + 4;
  pm->rec_buffer_len = 4;
  *pm->u.rec_counter = 0;
}

static int pcan_usbpro_msg_add_rec(struct pcan_usbpro_msg *pm, int id, ...) {
  int l, i = 0;
  uint8_t *pc;
  va_list ap;
  va_start(ap, id);
  int rec_size = pcan_usbpro_sizeof_rec(id);
  if (rec_size < 0 || (pm->rec_buffer_size < (rec_size + pm->rec_buffer_len))) {
    va_end(ap);
    return 0;
  }
  pc = pm->rec_ptr + 1;
  switch (id) {
  case DATA_TYPE_USB2CAN_STRUCT_BUSLAST_RX:
    *pc++ = (uint8_t)va_arg(ap, int);            
    *(uint16_t *)pc = (uint16_t)va_arg(ap, int); 
    pc += 2;
    *(uint32_t *)pc = va_arg(ap, uint32_t); 
    pc += 4;
    break;
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_8: i += 4; /* fall through */
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_4: i += 4; /* fall through */
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_0:
    *pc++ = (uint8_t)va_arg(ap, int);       
    *pc++ = (uint8_t)va_arg(ap, int);       
    *pc++ = (uint8_t)va_arg(ap, int);       
    *(uint32_t *)pc = va_arg(ap, uint32_t); 
    pc += 4;
    memcpy(pc, va_arg(ap, int *), i);
    pc += i;
    break;
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_8: i += 4; /* fall through */
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_4: i += 4; /* fall through */
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_0:
  case DATA_TYPE_USB2CAN_STRUCT_CANMSG_RTR_RX:
    *pc++ = (uint8_t)va_arg(ap, int);       
    *pc++ = (uint8_t)va_arg(ap, int);       
    *pc++ = (uint8_t)va_arg(ap, int);       
    *(uint32_t *)pc = va_arg(ap, uint32_t); 
    pc += 4;
    *(uint32_t *)pc = va_arg(ap, uint32_t); 
    pc += 4;
    memcpy(pc, va_arg(ap, int *), i);
    pc += i;
    break;
  case DATA_TYPE_USB2CAN_STRUCT_CALIBRATION_TIMESTAMP_RX:
    pc += 3;                                
    *(uint32_t *)pc = va_arg(ap, uint32_t); 
    pc += 4;
    *(uint32_t *)pc = va_arg(ap, uint32_t); 
    pc += 4;
    break;
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETCANBUSACTIVATE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETCANBUS_ERROR_STATUS:
    *pc++ = (uint8_t)va_arg(ap, int);            
    *(uint16_t *)pc = (uint16_t)va_arg(ap, int); 
    pc += 2;
    break;
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETBAUDRATE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETBAUDRATE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETDEVICENR:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_GETDEVICENR:
    *pc++ = (uint8_t)va_arg(ap, int); 
    pc += 2;                          
    *(uint32_t *)pc = va_arg(ap, uint32_t);
    pc += 4;
    break;
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETCANBUSACTIVATE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETSILENTMODE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETWARNINGLIMIT:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETFILTERMODE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETRESET_MODE:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETERRORFRAME:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_TIMESTAMP_PARAM:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_ERROR_GEN_NOW:
    *pc++ = (uint8_t)va_arg(ap, int); 
    *(uint16_t *)pc = (uint16_t)va_arg(ap, int);
    pc += 2;
    break;
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETLOOKUP_EXPLICIT:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SET_CANLED:
    *pc++ = (uint8_t)va_arg(ap, int);            
    *(uint16_t *)pc = (uint16_t)va_arg(ap, int); 
    pc += 2;
    *(uint32_t *)pc = va_arg(ap, uint32_t); 
    pc += 4;
    break;
  case DATA_TYPE_USB2CAN_STRUCT_FKT_SETGET_CALIBRATION_MSG:
  case DATA_TYPE_USB2CAN_STRUCT_FKT_USB_IN_PACKET_DELAY:
    pc++; 
    *(uint16_t *)pc = (uint16_t)va_arg(ap, int); 
    pc += 2;
    break;
  default: break;
  }
  l = pc - pm->rec_ptr;
  if (l > 0) {
    *pm->u.rec_counter = *pm->u.rec_counter + 1;
    *(pm->rec_ptr) = (uint8_t)id;
    pm->rec_ptr = pc;
    pm->rec_buffer_len += l;
  }
  va_end(ap);
  return l;
}

int pcan_protocol_set_baudrate(uint8_t channel, uint32_t ccbt) {
  uint32_t brp = (ccbt & 0x3fff) + 1;
  uint32_t tseg2 = ((ccbt >> 20) & 0x07) + 1;
  uint32_t tseg1 = ((ccbt >> 16) & 0x0f) + 1;
  uint32_t sjw = ((ccbt >> 14) & 0x03) + 1;
  uint32_t pcan_brp = (24 * brp) / 56;

  pcan_can_set_bitrate_ex(channel, pcan_brp, tseg1, tseg2, sjw);
  pcan_device.can[channel].ccbt = ccbt;
  return 0;
}

int pcan_protocol_rx_frame(uint8_t channel, struct t_can_msg *pmsg) {
  uint8_t rec_type, client = 0, flags = 0;

  if (!pcan_device.can[channel].led_is_busy && !(pmsg->flags & MSG_FLAG_ECHO)) {
    pcan_led_set_mode(channel ? LED_CH1_RX : LED_CH0_RX, LED_MODE_BLINK_FAST, 237);
  }
  if (pmsg->flags & MSG_FLAG_RTR) rec_type = DATA_TYPE_USB2CAN_STRUCT_CANMSG_RTR_RX;
  else if (pmsg->size == 0) rec_type = DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_0;
  else if (pmsg->size <= 4) rec_type = DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_4;
  else rec_type = DATA_TYPE_USB2CAN_STRUCT_CANMSG_RX_8;

  if (pmsg->flags & MSG_FLAG_ECHO) { flags |= 0x04; client = pmsg->dummy; }
  if (pmsg->flags & MSG_FLAG_RTR) flags |= PCAN_USBPRO_RTR;
  if (pmsg->flags & MSG_FLAG_EXT) flags |= PCAN_USBPRO_EXT;

  pcan_usbpro_msg_add_rec(&resp[PCAN_USB_BUFFER_DATA], rec_type, client, flags,
                          ((channel << 4) | pmsg->size), pmsg->timestamp,
                          pmsg->id, pmsg->data);
  return 0;
}

int pcan_protocol_tx_frame_cb(uint8_t channel, struct t_can_msg *pmsg) {
  if (pmsg->flags & MSG_FLAG_ECHO) pcan_protocol_rx_frame(channel, pmsg);
  if (!pcan_device.can[channel].led_is_busy) {
    pcan_led_set_mode(channel ? LED_CH1_TX : LED_CH0_TX, LED_MODE_BLINK_FAST, 237);
  }
  return 0;
}

int pcan_protocol_tx_frame(struct pcan_usbpro_canmsg_tx *pmsg) {
  struct t_can_msg msg = {0};
  uint8_t channel = (pmsg->len >> 4) & 0x0f;
  if (channel >= CAN_CHANNEL_MAX) return -1;
  msg.id = pmsg->id;
  msg.size = pmsg->len & 0x0f;
  if (msg.size > 8) return -1;
  if (pmsg->flags & PCAN_USBPRO_RTR) msg.flags |= MSG_FLAG_RTR;
  if (pmsg->flags & PCAN_USBPRO_EXT) msg.flags |= MSG_FLAG_EXT;
  if (pmsg->client & PCAN_USBPRO_SR) { msg.flags |= MSG_FLAG_ECHO; msg.dummy = pmsg->client; }
  memcpy(msg.data, pmsg->data, msg.size);
  msg.timestamp = pcan_timestamp_us();
  pcan_can_write(channel, &msg);
  return 0;
}

void pcan_protocol_process_data(uint8_t ep, uint8_t *ptr, uint16_t size) {
  struct pcan_usbpro_msg m = {0};


  uint8_t *rec_ptr = pcan_usbpro_msg_init(&m, ptr, size);
  if (!rec_ptr || size < 4) return;

  int buffer_ep = (ep == PCAN_USB_EP_CMDOUT) ? PCAN_USB_BUFFER_CMD : PCAN_USB_BUFFER_DATA;
  uint32_t num_recs = *m.u.rec_counter_read;

  size -= 4; // Account for the 4-byte header

  for (uint32_t r = 0; r < num_recs; r++) {
    union pcan_usbpro_rec *prec = (void *)rec_ptr;
    int rec_size = pcan_usbpro_sizeof_rec(prec->data_type);

    if (rec_size < 0 || size < rec_size) return; 

    switch (prec->data_type) {
    case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_8:
    case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_4:
    case DATA_TYPE_USB2CAN_STRUCT_CANMSG_TX_0:
      pcan_protocol_tx_frame(&prec->canmsg_tx);
      break;
    case DATA_TYPE_USB2CAN_STRUCT_FKT_SETBAUDRATE:
      pcan_protocol_set_baudrate(prec->baudrate.channel, prec->baudrate.CCBT);
      break;
    case DATA_TYPE_USB2CAN_STRUCT_FKT_SETCANBUSACTIVATE:
      pcan_can_set_bus_active(prec->bus_activity.channel ? CAN_BUS_2 : CAN_BUS_1, prec->bus_activity.onoff);
      break;
    case DATA_TYPE_USB2CAN_STRUCT_FKT_GETDEVICENR:
      pcan_usbpro_msg_add_rec(&resp[PCAN_USB_BUFFER_CMD], DATA_TYPE_USB2CAN_STRUCT_FKT_GETDEVICENR, 
                              prec->dev_nr.channel, pcan_device.can[prec->dev_nr.channel].channel_nr);
      break;
    default: break;
    }
    rec_ptr += rec_size;
    size -= rec_size;
  }
}

void pcan_protocol_init(void) {
  pcan_can_init_ex(CAN_BUS_1, 500000);
  pcan_can_init_ex(CAN_BUS_2, 500000);
  pcan_usbpro_msg_init_empty(&resp[PCAN_USB_BUFFER_CMD], &resp_buffer[PCAN_USB_BUFFER_CMD], PCAN_USB_DATA_BUFFER_SIZE);
  pcan_usbpro_msg_init_empty(&resp[PCAN_USB_BUFFER_DATA], &resp_buffer[PCAN_USB_BUFFER_DATA], PCAN_USB_DATA_BUFFER_SIZE);
  pcan_can_install_rx_callback(CAN_BUS_1, pcan_protocol_rx_frame);
  pcan_can_install_rx_callback(CAN_BUS_2, pcan_protocol_rx_frame);
  pcan_can_install_tx_callback(CAN_BUS_1, pcan_protocol_tx_frame_cb);
  pcan_can_install_tx_callback(CAN_BUS_2, pcan_protocol_tx_frame_cb);
}

#if defined(ESP_PLATFORM)
bool pcan_usb_transmit(uint8_t ep, uint8_t *buf, uint16_t size);
#endif

void pcan_protocol_poll(void) {
  pcan_can_poll();
  for (int i = 0; i < 2; i++) {
    if (resp[i].rec_buffer_len > 4) {
#if defined(ESP_PLATFORM)
      uint8_t ep = (i == 0) ? 0x81 : 0x82; // EP_CMDIN or EP_MSGIN_CH1
      if (pcan_usb_transmit(ep, resp[i].u.rec_buffer, resp[i].rec_buffer_len)) {
        pcan_usbpro_msg_reset(&resp[i]);
      }
#else
      if (pcan_flush_data(&resp_fsm[i], resp[i].u.rec_buffer, resp[i].rec_buffer_len)) {
        pcan_usbpro_msg_reset(&resp[i]);
      }
#endif
    }
  }
}
