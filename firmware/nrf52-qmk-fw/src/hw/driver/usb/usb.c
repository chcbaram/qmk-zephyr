
#include "usb.h"
#include "usb_hid/usb_hid.h"


#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(usbd);


#define USB_VID           0x2FE3
#define USB_PID           0x0001
#define USBD_MANUFACTURER "BARAM"
#define USBD_PRODUCT      "QMK-NRF52"



/* By default, do not register the USB DFU class DFU mode instance. */
static const char *const blocklist[] = {
  "dfu_dfu",
  NULL,
};


USBD_DEVICE_DEFINE(usbd,
		   DEVICE_DT_GET(DT_NODELABEL(zephyr_udc0)),
		   USB_VID, USB_PID);


USBD_DESC_LANG_DEFINE(usbd_lang);
USBD_DESC_MANUFACTURER_DEFINE(usbd_mfr, USBD_MANUFACTURER);
USBD_DESC_PRODUCT_DEFINE(usbd_product, USBD_PRODUCT);
IF_ENABLED(CONFIG_HWINFO, (USBD_DESC_SERIAL_NUMBER_DEFINE(usbd_sn)));

USBD_DESC_CONFIG_DEFINE(fs_cfg_desc, "FS Configuration");
USBD_DESC_CONFIG_DEFINE(hs_cfg_desc, "HS Configuration");

static const uint8_t attributes = USB_SCD_SELF_POWERED |
                                  USB_SCD_REMOTE_WAKEUP;

/* Full speed configuration */
USBD_CONFIGURATION_DEFINE(usbd_fs_config,
			  attributes,
			  250, &fs_cfg_desc);

/* High speed configuration */
USBD_CONFIGURATION_DEFINE(usbd_hs_config,
			  attributes,
			  250, &hs_cfg_desc);


static void usbd_fix_code_triple(struct usbd_context *uds_ctx,
				   const enum usbd_speed speed)
{
  /* Always use class code information from Interface Descriptors */
  if (IS_ENABLED(CONFIG_USBD_CDC_ACM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_CDC_ECM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_CDC_NCM_CLASS) ||
      IS_ENABLED(CONFIG_USBD_MIDI2_CLASS) ||
      IS_ENABLED(CONFIG_USBD_AUDIO2_CLASS))
  {
    /*
     * Class with multiple interfaces have an Interface
     * Association Descriptor available, use an appropriate triple
     * to indicate it.
     */
    usbd_device_set_code_triple(uds_ctx, speed,
                                USB_BCC_MISCELLANEOUS, 0x02, 0x01);
  }
  else
  {
    usbd_device_set_code_triple(uds_ctx, speed, 0, 0, 0);
  }
}

struct usbd_context *usbd_setup_device(usbd_msg_cb_t msg_cb)
{
  int err;

  err = usbd_add_descriptor(&usbd, &usbd_lang);
  if (err)
  {
    LOG_ERR("Failed to initialize language descriptor (%d)", err);
    return NULL;
  }

  err = usbd_add_descriptor(&usbd, &usbd_mfr);
  if (err)
  {
    LOG_ERR("Failed to initialize manufacturer descriptor (%d)", err);
    return NULL;
  }

  err = usbd_add_descriptor(&usbd, &usbd_product);
  if (err)
  {
    LOG_ERR("Failed to initialize product descriptor (%d)", err);
    return NULL;
  }

  IF_ENABLED(CONFIG_HWINFO, (
                            err = usbd_add_descriptor(&usbd, &usbd_sn);))
  if (err)
  {
    LOG_ERR("Failed to initialize SN descriptor (%d)", err);
    return NULL;
  }

  if (USBD_SUPPORTS_HIGH_SPEED &&
      usbd_caps_speed(&usbd) == USBD_SPEED_HS)
  {
    err = usbd_add_configuration(&usbd, USBD_SPEED_HS,
                                 &usbd_hs_config);
    if (err)
    {
      LOG_ERR("Failed to add High-Speed configuration");
      return NULL;
    }

    err = usbd_register_all_classes(&usbd, USBD_SPEED_HS, 1,
                                    blocklist);
    if (err)
    {
      LOG_ERR("Failed to add register classes");
      return NULL;
    }

    usbd_fix_code_triple(&usbd, USBD_SPEED_HS);
  }

  err = usbd_add_configuration(&usbd, USBD_SPEED_FS,
                               &usbd_fs_config);
  if (err)
  {
    LOG_ERR("Failed to add Full-Speed configuration");
    return NULL;
  }  

  err = usbd_register_all_classes(&usbd, USBD_SPEED_FS, 1, blocklist);
  if (err)
  {
    LOG_ERR("Failed to add register classes");
    return NULL;
  }

  usbd_fix_code_triple(&usbd, USBD_SPEED_FS);
  usbd_self_powered(&usbd, attributes & USB_SCD_SELF_POWERED);

  if (msg_cb != NULL)
  {    
    err = usbd_msg_register_cb(&usbd, msg_cb);
    if (err)
    {
      LOG_ERR("Failed to register message callback");
      return NULL;
    }  
  }

  return &usbd;
}

struct usbd_context *usbd_init_device(usbd_msg_cb_t msg_cb)
{
  int err;

  if (usbd_setup_device(msg_cb) == NULL)
  {
    return NULL;
  }

  err = usbd_init(&usbd);
  if (err)
  {
    LOG_ERR("Failed to initialize device support");
    return NULL;
  }

  return &usbd;
}

static void msg_cb(struct usbd_context *const   usbd_ctx,
                   const struct usbd_msg *const msg)
{
  LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

  if (msg->type == USBD_MSG_CONFIGURATION)
  {
    LOG_INF("\tConfiguration value %d", msg->status);
  }

  if (usbd_can_detect_vbus(usbd_ctx))
  {
    if (msg->type == USBD_MSG_VBUS_READY)
    {
      if (usbd_enable(usbd_ctx))
      {
        LOG_ERR("Failed to enable device support");
      }
    }

    if (msg->type == USBD_MSG_VBUS_REMOVED)
    {
      if (usbd_disable(usbd_ctx))
      {
        LOG_ERR("Failed to disable device support");
      }
    }
  }
}

bool usbInit(void)
{
  struct usbd_context *p_usbd;


  if (!usbHidInit())
  {
    LOG_ERR("usbHidInit()");
    return false;
  }

  p_usbd = usbd_init_device(msg_cb);
  if (p_usbd == NULL)
  {
    LOG_ERR("usbd_init_device()");
    return false;
  }

  if (!usbd_can_detect_vbus(p_usbd))
  {
    int ret;

    ret = usbd_enable(p_usbd);
    if (ret)
    {
      LOG_ERR("usbd_enable()");
      return false;
    }
  }

  return true;
}