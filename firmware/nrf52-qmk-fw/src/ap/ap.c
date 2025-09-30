#include "ap.h"

#include <zephyr/logging/log.h>

#include <sample_usbd.h>

#include <zephyr/input/input.h>
#include <zephyr/device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/bos.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/class/usbd_hid.h>

LOG_MODULE_REGISTER(nrf52_qmk_fw, LOG_LEVEL_DBG);

const struct device *hid_dev = DEVICE_DT_GET_ONE(zephyr_hid_device);
static const uint8_t hid_report_desc[] = HID_KEYBOARD_REPORT_DESC();


bool usbdInit(void);


void apInit(void)
{

  usbdInit();

  systemInit();
}

void apMain(void)
{
  systemMain();
}

struct kb_event {
	uint16_t code;
	int32_t value;
};

K_MSGQ_DEFINE(kb_msgq, sizeof(struct kb_event), 2, 1);

// UDC_STATIC_BUF_DEFINE(report, KB_REPORT_COUNT);
static uint32_t kb_duration;
static bool kb_ready;

static void input_cb(struct input_event *evt, void *user_data)
{
	struct kb_event kb_evt;

	ARG_UNUSED(user_data);

	kb_evt.code = evt->code;
	kb_evt.value = evt->value;
	if (k_msgq_put(&kb_msgq, &kb_evt, K_NO_WAIT) != 0) {
		LOG_ERR("Failed to put new input event");
	}
}

INPUT_CALLBACK_DEFINE(NULL, input_cb, NULL);

static void kb_iface_ready(const struct device *dev, const bool ready)
{
	LOG_INF("HID device %s interface is %s",
		dev->name, ready ? "ready" : "not ready");
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
	if (type != HID_REPORT_TYPE_OUTPUT) {
		LOG_WRN("Unsupported report type");
		return -ENOTSUP;
	}

	// for (unsigned int i = 0; i < ARRAY_SIZE(kb_leds); i++) {
	// 	if (kb_leds[i].port == NULL) {
	// 		continue;
	// 	}

	// 	(void)gpio_pin_set_dt(&kb_leds[i], buf[0] & BIT(i));
	// }
  logPrintf("kb_set_report()\n");
	return 0;
}

/* Idle duration is stored but not used to calculate idle reports. */
static void kb_set_idle(const struct device *dev,
			const uint8_t id, const uint32_t duration)
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

static void kb_output_report(const struct device *dev, const uint16_t len,
			     const uint8_t *const buf)
{
	LOG_HEXDUMP_DBG(buf, len, "o.r.");
	kb_set_report(dev, HID_REPORT_TYPE_OUTPUT, 0U, len, buf);
}

struct hid_device_ops kb_ops = {
	.iface_ready = kb_iface_ready,
	.get_report = kb_get_report,
	.set_report = kb_set_report,
	.set_idle = kb_set_idle,
	.get_idle = kb_get_idle,
	.set_protocol = kb_set_protocol,
	.output_report = kb_output_report,
};

/* doc device msg-cb start */
static void msg_cb(struct usbd_context *const usbd_ctx,
		   const struct usbd_msg *const msg)
{
	LOG_INF("USBD message: %s", usbd_msg_type_string(msg->type));

	if (msg->type == USBD_MSG_CONFIGURATION) {
		LOG_INF("\tConfiguration value %d", msg->status);
	}

	if (usbd_can_detect_vbus(usbd_ctx)) {
		if (msg->type == USBD_MSG_VBUS_READY) {
			if (usbd_enable(usbd_ctx)) {
				LOG_ERR("Failed to enable device support");
			}
		}

		if (msg->type == USBD_MSG_VBUS_REMOVED) {
			if (usbd_disable(usbd_ctx)) {
				LOG_ERR("Failed to disable device support");
			}
		}
	}
}

bool usbdInit(void)
{


  struct usbd_context *sample_usbd;
  int ret;


  sample_usbd = sample_usbd_init_device(msg_cb);
  if (sample_usbd == NULL)
  {
    LOG_ERR("Failed to initialize USB device");
    return -ENODEV;
  }

  if (!usbd_can_detect_vbus(sample_usbd))
  {
    /* doc device enable start */
    ret = usbd_enable(sample_usbd);
    if (ret)
    {
      LOG_ERR("Failed to enable device support");
      return ret;
    }
    /* doc device enable end */
  }

  if (!device_is_ready(hid_dev))
  {
    LOG_ERR("HID Device is not ready");
    return false;
  }  
  ret = hid_device_register(hid_dev,
                            hid_report_desc, sizeof(hid_report_desc),
                            &kb_ops);
  if (ret != 0)
  {
    LOG_ERR("Failed to register HID Device, %d", ret);
    return ret;
  }
    
  LOG_INF("HID keyboard sample is initialized");

  return true;
}

