
#include "usb_hid.h"

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/input/input.h>

#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>

LOG_MODULE_REGISTER(usb_hid);


#define HID_KBD_NODE   DT_NODELABEL(usb_hid_kbd)
#define HID_VIA_NODE   DT_NODELABEL(usb_hid_via)
#define HID_EXK_NODE   DT_NODELABEL(usb_hid_exk)

static const struct device *hid_kbd_dev = DEVICE_DT_GET(HID_KBD_NODE);
static const struct device *hid_via_dev = DEVICE_DT_GET(HID_VIA_NODE);
static const struct device *hid_exk_dev = DEVICE_DT_GET(HID_EXK_NODE);


static const uint8_t hid_report_kbd_desc[] =
{
  HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP),
  HID_USAGE(HID_USAGE_GEN_DESKTOP_KEYBOARD),
  HID_COLLECTION(HID_COLLECTION_APPLICATION),
  HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD), /* HID_USAGE_MINIMUM(Keyboard LeftControl) */
  HID_USAGE_MIN8(0xE0),                         /* HID_USAGE_MAXIMUM(Keyboard Right GUI) */
  HID_USAGE_MAX8(0xE7),
  HID_LOGICAL_MIN8(0),
  HID_LOGICAL_MAX8(1),
  HID_REPORT_SIZE(1),
  HID_REPORT_COUNT(8),                          /* HID_INPUT(Data,Var,Abs) */
  HID_INPUT(0x02),
  HID_REPORT_SIZE(8),
  HID_REPORT_COUNT(1),                          /* HID_INPUT(Cnst,Var,Abs) */
  HID_INPUT(0x03),
  HID_REPORT_SIZE(1),
  HID_REPORT_COUNT(5),
  HID_USAGE_PAGE(HID_USAGE_GEN_LEDS),           /* HID_USAGE_MINIMUM(Num Lock) */
  HID_USAGE_MIN8(1),                            /* HID_USAGE_MAXIMUM(Kana) */
  HID_USAGE_MAX8(5),                            /* HID_OUTPUT(Data,Var,Abs) */
  HID_OUTPUT(0x02),
  HID_REPORT_SIZE(3),
  HID_REPORT_COUNT(1),                          /* HID_OUTPUT(Cnst,Var,Abs) */
  HID_OUTPUT(0x03),
  HID_REPORT_SIZE(8),
  HID_REPORT_COUNT(6),
  HID_LOGICAL_MIN8(0),
  HID_LOGICAL_MAX8(101),
  HID_USAGE_PAGE(HID_USAGE_GEN_DESKTOP_KEYPAD), /* HID_USAGE_MIN8(Reserved) */
  HID_USAGE_MIN8(0),                            /* HID_USAGE_MAX8(Keyboard Application) */
  HID_USAGE_MAX8(101),                          /* HID_INPUT (Data,Ary,Abs) */
  HID_INPUT(0x00),
  HID_END_COLLECTION,
};

static uint8_t hid_report_via_desc[] = 
{
  //
  0x06, 0x60, 0xFF, // Usage Page (Vendor Defined)
  0x09, 0x61,       // Usage (Vendor Defined)
  0xA1, 0x01,       // Collection (Application)
  // Data to host
  0x09, 0x62,       //   Usage (Vendor Defined)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x95, 32,         //   Report Count
  0x75, 0x08,       //   Report Size (8)
  0x81, 0x02,       //   Input (Data, Variable, Absolute)
  // Data from host
  0x09, 0x63,       //   Usage (Vendor Defined)
  0x15, 0x00,       //   Logical Minimum (0)
  0x26, 0xFF, 0x00, //   Logical Maximum (255)
  0x95, 32,         //   Report Count
  0x75, 0x08,       //   Report Size (8)
  0x91, 0x02,       //   Output (Data, Variable, Absolute)
  0xC0              // End Collection
};

static uint8_t hid_report_exk_desc[] =
{
  //
  0x05, 0x01,               // Usage Page (Generic Desktop)
  0x09, 0x80,               // Usage (System Control)
  0xA1, 0x01,               // Collection (Application)
  0x85, 3,                  //   Report ID
  0x19, 0x01,               //   Usage Minimum (Pointer)
  0x2A, 0xB7, 0x00,         //   Usage Maximum (System Display LCD Autoscale)
  0x15, 0x01,               //   Logical Minimum
  0x26, 0xB7, 0x00,         //   Logical Maximum
  0x95, 0x01,               //   Report Count (1)
  0x75, 0x10,               //   Report Size (16)
  0x81, 0x00,               //   Input (Data, Array, Absolute)
  0xC0,                     // End Collection

  0x05, 0x0C,               // Usage Page (Consumer)
  0x09, 0x01,               // Usage (Consumer Control)
  0xA1, 0x01,               // Collection (Application)
  0x85, 4,                  //   Report ID
  0x19, 0x01,               //   Usage Minimum (Consumer Control)
  0x2A, 0xA0, 0x02,         //   Usage Maximum (AC Desktop Show All Applications)
  0x15, 0x01,               //   Logical Minimum
  0x26, 0xA0, 0x02,         //   Logical Maximum
  0x95, 0x01,               //   Report Count (1)
  0x75, 0x10,               //   Report Size (16)
  0x81, 0x00,               //   Input (Data, Array, Absolute)
  0xC0                      // End Collection
};


enum kb_leds_idx
{
  KB_LED_NUMLOCK = 0,
	KB_LED_CAPSLOCK,
	KB_LED_SCROLLLOCK,
	KB_LED_COUNT,
};

enum kb_report_idx {
	KB_MOD_KEY = 0,
	KB_RESERVED,
	KB_KEY_CODE1,
	KB_KEY_CODE2,
	KB_KEY_CODE3,
	KB_KEY_CODE4,
	KB_KEY_CODE5,
	KB_KEY_CODE6,
	KB_REPORT_COUNT,
};

struct kb_event {
	uint16_t code;
	int32_t value;
};

K_MSGQ_DEFINE(kb_msgq, sizeof(struct kb_event), 2, 1);

UDC_STATIC_BUF_DEFINE(report, KB_REPORT_COUNT);
static uint32_t kb_duration;
static bool     kb_ready;


// static void input_cb(struct input_event *evt, void *user_data)
// {
//   struct kb_event kb_evt;

//   ARG_UNUSED(user_data);

//   kb_evt.code  = evt->code;
//   kb_evt.value = evt->value;
//   if (k_msgq_put(&kb_msgq, &kb_evt, K_NO_WAIT) != 0)
//   {
//     LOG_ERR("Failed to put new input event");
//   }
// }

// INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);

static void kb_iface_ready(const struct device *dev, const bool ready)
{
  LOG_INF("HID device %s interface is %s", dev->name, ready ? "ready" : "not ready");
  kb_ready = ready;
}

static int kb_get_report(const struct device *dev,
                         const uint8_t type, const uint8_t id, const uint16_t len,
                         uint8_t *const buf)
{
  LOG_WRN("Get Report not implemented, Type %u ID %u", type, id);
  return 0;
}

static int kb_set_report(const struct device *dev,
                         const uint8_t type, const uint8_t id, const uint16_t len,
                         const uint8_t *const buf)
{
  if (type != HID_REPORT_TYPE_OUTPUT)
  {
    LOG_WRN("Unsupported report type");
    return -ENOTSUP;
  }
  return 0;
}

static void kb_set_idle(const struct device *dev, const uint8_t id, const uint32_t duration)
{
  LOG_INF("Set Idle %u to %u", id, duration);
  kb_duration = duration;
}

static uint32_t kb_get_idle(const struct device *dev, const uint8_t id)
{
  LOG_INF("Get Idle %u to %u", id, kb_duration);
  return kb_duration;
}

static void kb_set_protocol(const struct device *dev, const uint8_t proto)
{
  LOG_INF("Protocol changed to %s",
          proto == 0U ? "Boot Protocol" : "Report Protocol");
}

static void kb_output_report(const struct device *dev, const uint16_t len, const uint8_t *const buf)
{
  logPrintf("kb_output_report\n");
  LOG_HEXDUMP_DBG(buf, len, "o.r.");
  kb_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);  
}

static void via_iface_ready(const struct device *dev, const bool ready)
{
  logPrintf("via_iface_ready()\n");
  LOG_INF("VIA device %s interface is %s", dev->name, ready ? "ready" : "not ready");
  kb_ready = ready;
}

static int via_get_report(const struct device *dev,
                         const uint8_t type, const uint8_t id, const uint16_t len,
                         uint8_t *const buf)
{
  LOG_WRN("VIA Get Report not implemented, Type %u ID %u", type, id);
  logPrintf("via_get_report()\n");
  return 0;
}

static int via_set_report(const struct device *dev,
                         const uint8_t type, const uint8_t id, const uint16_t len,
                         const uint8_t *const buf)
{
  logPrintf("via_set_report()\n");

  if (type != HID_REPORT_TYPE_OUTPUT)
  {
    LOG_WRN("Unsupported report type");
    return -ENOTSUP;
  }

  LOG_WRN("VIA Set Report not implemented, Type %u ID %u", type, id);

  return 0;
}

static void via_output_report(const struct device *dev, const uint16_t len, const uint8_t *const buf)
{
  logPrintf("via_output_report\n");
  via_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);  
}

struct hid_device_ops kbd_ops = 
{
  .iface_ready   = kb_iface_ready,
  .get_report    = kb_get_report,
  .set_report    = kb_set_report,
  .set_idle      = kb_set_idle,
  .get_idle      = kb_get_idle,
  .set_protocol  = kb_set_protocol,
  .output_report = kb_output_report,
};

struct hid_device_ops via_ops = 
{
  .iface_ready   = via_iface_ready,
  .get_report    = via_get_report,
  .set_report    = via_set_report,
  // .set_idle      = kb_set_idle,
  // .get_idle      = kb_get_idle,
  // .set_protocol  = kb_set_protocol,
  .output_report = via_output_report,
};

bool usbHidInit(void)
{  
	int ret;


  if (!device_is_ready(hid_kbd_dev))
  {
    LOG_ERR("USB KBD Device is not ready");
    return -EIO;
  }
  if (!device_is_ready(hid_via_dev))
  {
    LOG_ERR("USB VIA Device is not ready");
    return -EIO;
  }
  if (!device_is_ready(hid_exk_dev))
  {
    LOG_ERR("USB EXK Device is not ready");
    return -EIO;
  }

  ret = hid_device_register(hid_kbd_dev,
                            hid_report_kbd_desc, sizeof(hid_report_kbd_desc),
                            &kbd_ops);
  if (ret != 0)
  {
    LOG_ERR("Failed to register hid_kbd_dev, %d", ret);
    return ret;
  }

  ret = hid_device_register(hid_via_dev,
                            hid_report_via_desc, sizeof(hid_report_via_desc),
                            &via_ops);
  if (ret != 0)
  {
    LOG_ERR("Failed to register hid_kbd_dev, %d", ret);
    return ret;
  }

  ret = hid_device_register(hid_exk_dev,
                            hid_report_exk_desc, sizeof(hid_report_exk_desc),
                            &kbd_ops);
  if (ret != 0)
  {
    LOG_ERR("Failed to register hid_kbd_dev, %d", ret);
    return ret;
  }  
  return true;
}