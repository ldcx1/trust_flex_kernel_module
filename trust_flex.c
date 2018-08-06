/*
 *  USB Trust Flex Design support
 *
 *  (c) 2018 teemoshido
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* This is an attempt to implement simple/generic touchpad protocol
 http://linuxwacom.sourceforge.net/wiki/index.php/Kernel_Input_Event_Overview
 https://www.kernel.org/doc/Documentation/input/event-codes.txt

based on original drivers by  <weixing@hanwang.com.cn>
http://linux.fjfi.cvut.cz/~taxman/hw/hanvon/
https://github.com/exaroth/pentagram_virtuoso_drivers/blob/master/hanvon.c
further modified and improved by aidyw
https://github.com/aidyw/bosto-2g-linux-kernel-module
*/
#include <linux/jiffies.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/fcntl.h>
#include <linux/unistd.h>
#include <asm/unaligned.h>
#define DRIVER_AUTHOR   "teemoshido <lese.d@yahoo.com>"//"stacksmith <fpgasm@apple2.x10.mx>"
#define DRIVER_DESC     "Trust Flex Design tablet driver"
#define DRIVER_LICENSE  "GPL"

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE(DRIVER_LICENSE);

#define USB_VENDOR_ID_TRUST		  0x2179
#define USB_PRODUCT_FLEX_DESIGN 0x0077
#define PKT_LEN_MAX	10

#define MAX_X 1920
#define MAX_Y 1080

#define MAX_VALUE 0x7F

#define X_FACTOR MAX_X / MAX_VALUE + 1
#define Y_FACTOR MAX_Y / MAX_VALUE + 1

/* match vendor and interface info */

struct strust {
  unsigned char *data;
  dma_addr_t data_dma;
  struct input_dev *dev;
  struct usb_device *usbdev;
  struct urb *irq;
  char name[64];
  char phys[32];
};

#define PRODUCT_NAME "Trust Flex Design"

static const int hw_eventtypes[]= {EV_KEY, EV_ABS};
static const int hw_absevents[] = {ABS_X, ABS_Y, ABS_PRESSURE};
static const int hw_mscevents[] = {};
static const int hw_btnevents[] = { BTN_TOUCH, BTN_TOOL_PEN,
				    BTN_TOOL_RUBBER, BTN_STYLUS2, BTN_STYLUS, BTN_LEFT};

static void trust_flex_parse_packet(struct strust *buffer ){
  unsigned char *data = buffer->data;
  struct input_dev *input_dev = buffer->dev;

  unsigned char data1 = data[1];   
  //printk("trust_packet: %02x:%02x:%03x:%03x:%03x:%02x:%02x:%02x:%02x:%02x Time:%li\n",	  data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9], jiffies);
  //printk("trust_packet: %01x:%02x:%02x:%02x:%02x:%02x:%02x Time:%li\n",	data[1], data[2], data[3], data[4], data[5], data[6], data[7], jiffies);
  // Raport ID : Button Pressed : X : X : Y : Y : Preasure : Preasure Level
  //      0    :        1       : 2 : 3 : 4 : 5 :    6     :       7
  
  input_report_key(input_dev, BTN_TOUCH, (data1 != 0x00 ) || (data[6] != 0));
  input_report_key(input_dev, BTN_TOOL_PEN, 1);//I think it is when is enought close to the pad

  input_report_abs(input_dev, ABS_PRESSURE, data[6]);
  
  input_report_key(input_dev, BTN_LEFT,  data1 & 0x01);
  input_report_key(input_dev, BTN_STYLUS,  data1 & 0x04 || data1 & 0x05);
  input_report_key(input_dev, BTN_STYLUS2, data1 & 0x02 || data1 & 0x03);

  input_report_abs(input_dev, ABS_X, data[3] * X_FACTOR);
  input_report_abs(input_dev, ABS_Y, data[5] * Y_FACTOR);

  input_sync(input_dev);

}

static void trust_flex_irq(struct urb *urb)
{
  struct strust *buffer = urb->context;
  int retval;
  
  switch (urb->status) {
  case 0:
    trust_flex_parse_packet(buffer);
    break;
  case -ECONNRESET:
  case -ENOENT:
  case -EINPROGRESS:
  case -ESHUTDOWN:
  case -ENODEV:
  default:
    //printk("%s - nonzero urb status received: %d", __func__, urb->status);
    break;
  }
  
  retval = usb_submit_urb(urb, GFP_ATOMIC);
  if (retval)
    printk("%s - usb_submit_urb failed with result %d", __func__, retval);
}

static int trust_flex_open(struct input_dev *dev)
{
  struct strust *buffer = input_get_drvdata(dev);
  
  buffer->irq->dev = buffer->usbdev;
  if (usb_submit_urb(buffer->irq, GFP_KERNEL))
    return -EIO;
  return 0;
}

static void trust_flex_close(struct input_dev *dev)
{
  struct strust *buffer = input_get_drvdata(dev);
  usb_kill_urb(buffer->irq);
}


static int trust_flex_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
  struct usb_device *dev = interface_to_usbdev(intf);
  struct usb_endpoint_descriptor *endpoint;
  struct strust *buffer;
  struct input_dev *input_dev;
  int error;
  int i;
  
  buffer = kzalloc(sizeof(struct strust), GFP_KERNEL);
  input_dev = input_allocate_device();
  if (!buffer || !input_dev) {
    error = -ENOMEM;
    goto fail1;
  }
  
  if (!(buffer->data = usb_alloc_coherent(dev,PKT_LEN_MAX, GFP_KERNEL, &buffer->data_dma))){
    error = -ENOMEM;
    goto fail1;
  }
  
  if (!(buffer->irq = usb_alloc_urb(0, GFP_KERNEL))){
      error = -ENOMEM;
    goto fail2;
  }
  
  buffer->usbdev = dev;
  buffer->dev = input_dev;
  
  usb_make_path(dev, buffer->phys, sizeof(buffer->phys));
  strlcat(buffer->phys, "/input0", sizeof(buffer->phys));
  
  strlcpy(buffer->name, PRODUCT_NAME, sizeof(PRODUCT_NAME));
  input_dev->name = buffer->name;
  input_dev->phys = buffer->phys;
  usb_to_input_id(dev, &input_dev->id);
  input_dev->dev.parent = &intf->dev;
  
  input_set_drvdata(input_dev, buffer);
  
  input_dev->open = trust_flex_open;
  input_dev->close = trust_flex_close;
  
  for (i = 0; i < ARRAY_SIZE(hw_eventtypes); ++i)
    __set_bit(hw_eventtypes[i], input_dev->evbit);
  
  for (i = 0; i < ARRAY_SIZE(hw_absevents); ++i)
    __set_bit(hw_absevents[i], input_dev->absbit);
  
  for (i = 0; i < ARRAY_SIZE(hw_btnevents); ++i)
    __set_bit(hw_btnevents[i], input_dev->keybit);
  
  for (i = 0; i < ARRAY_SIZE(hw_mscevents); ++i)
    __set_bit(hw_mscevents[i], input_dev->mscbit);
                                                  //fuzz
  input_set_abs_params(input_dev, ABS_X,0, MAX_X, 0, 0);
  input_abs_set_res(input_dev, ABS_X, MAX_VALUE);

  input_set_abs_params(input_dev, ABS_Y, 0, MAX_Y, 0, 0);
  input_abs_set_res(input_dev, ABS_Y, MAX_VALUE);
  input_set_abs_params(input_dev, ABS_PRESSURE, 0, 0xFF, 0, 0);
  
  __set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
  
  endpoint = &intf->cur_altsetting->endpoint[0].desc;
  usb_fill_int_urb(buffer->irq, dev,
		   usb_rcvintpipe(dev, endpoint->bEndpointAddress),
		   buffer->data, PKT_LEN_MAX,
		   trust_flex_irq, buffer, endpoint->bInterval);
  buffer->irq->transfer_dma = buffer->data_dma;
  buffer->irq->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
  
  if (input_register_device(buffer->dev))
    goto fail3;
  
  usb_set_intfdata(intf, buffer);
  return 0;
  
 fail3:	usb_free_urb(buffer->irq);
 fail2:	usb_free_coherent(dev, PKT_LEN_MAX,
			  buffer->data, buffer->data_dma);
 fail1:	input_free_device(input_dev);
  kfree(buffer);
  return error;
  
}

static void trust_flex_disconnect(struct usb_interface *intf)
{
  struct strust *buffer = usb_get_intfdata(intf);
  
  printk (KERN_INFO "trust_flex: USB interface disconnected.\n");
  input_unregister_device(buffer->dev);
  usb_free_urb(buffer->irq);
  usb_free_coherent(interface_to_usbdev(intf),
		    PKT_LEN_MAX, buffer->data,
		    buffer->data_dma);
  kfree(buffer);
  usb_set_intfdata(intf, NULL);
}

static const struct usb_device_id trust_flex_ids[] = {
  {USB_DEVICE (USB_VENDOR_ID_TRUST, USB_PRODUCT_FLEX_DESIGN)},
  {}
};

MODULE_DEVICE_TABLE(usb, trust_flex_ids);

static struct usb_driver trust_flex_driver = {
  .name		= PRODUCT_NAME,
  .probe	= trust_flex_probe,
  .disconnect	= trust_flex_disconnect,
  .id_table	= trust_flex_ids,
};

static int __init trust_flex_init(void)
{
  printk(KERN_INFO "Trust Flex Design USB Driver module being initialised.\n" );
  return usb_register(&trust_flex_driver);
}

static void __exit trust_flex_exit(void)
{
  usb_deregister(&trust_flex_driver);
}

module_init(trust_flex_init);
module_exit(trust_flex_exit);
