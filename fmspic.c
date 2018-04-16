// Joystick RC transmitter using FMS-PIC "9600 F0" serial protocol.
// 
//  derived from "zhenhua.c" which was derived from
//  "twidjoy.c"
//
//  Copyright (c) 2008 Martin Kebert
//  Copyright (c) 2001 Arndt Schoenewald
//  Copyright (c) 2000-2001 Vojtech Pavlik
//  Copyright (c) 2000 Mark Fletcher
//
// This is based on the zhenhua.c driver, and since nobody's going to
// be using both this driver and the zhenhua driver, it's simpler to
// keep the same serio ID as Zhen Hua driver.  So, use inputattache
// like this:
//
//   inputattach --daemon --baud 9600 --always -zhen /dev/ttyUSB0
//
// Data coming from transmitter is in this order:
//   byte 0 = synchronisation byte == 0xF0 + #channels + 1
//   byte 1 = button data (always seems to be 0x00)
//   byte 2 = channel 1  (scaled 0x00-0xfe)
//   byte 3 = channel 2
//   byte 4 = channel 3
//   byte 5 = channel 4
// ....
// (repeated)
//
// This driver currently does not support the "19200 FF" PIC serial
// protocol that uses FF as a sync byte.
//
// Adapted from the zhen hua linux driver by Grant Edwards
// <grant.b.edwards@gmail.com>
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
// USA

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/serio.h>
#include <linux/init.h>

#define DRIVER_DESC	"Joystick driver for RC xmtr w/ FMS PIC serial protocol"

MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");

#define FMSPIC_MAX_LENGTH 8  // max number of data bytes supported

// fmspic data

struct fmspic 
{
  struct input_dev *dev;
  int idx;
  int len;
  unsigned char data[FMSPIC_MAX_LENGTH];
  char phys[128];
};

// fmspic_process_packet() decodes packet the driver receives from the
// RC transmitter.

static void fmspic_process_packet(struct fmspic *fmspic)
{
  struct input_dev *dev = fmspic->dev;
  unsigned char *data = fmspic->data;

  if (fmspic->len > 1)
    input_report_abs(dev, ABS_Y,  data[1]);
  if (fmspic->len > 2)
    input_report_abs(dev, ABS_X,  data[2]);
  if (fmspic->len > 3)
    input_report_abs(dev, ABS_RY, data[3]);
  if (fmspic->len > 4)
    input_report_abs(dev, ABS_RX, data[4]);
  if (fmspic->len > 5)
    input_report_abs(dev, ABS_Z, data[5]);
  if (fmspic->len > 6)
    input_report_abs(dev, ABS_RZ, data[6]);

  input_sync(dev);
}

// fmspic_interrupt() is called by the low level driver when
// characters are ready for us. We then buffer them for further
// processing, or call the packet processing routine.

static irqreturn_t fmspic_interrupt(struct serio *serio, unsigned char data, unsigned int flags)
{
  struct fmspic *fmspic = serio_get_drvdata(serio);
  
  // FMSPIC frames star with a syncbyte that is 0xF0 + framelength
  // (framelength includes syncbyte).  No other bytes have 0xF as
  // upper nibble.
  
  // snyc byte?
  if ((data & 0xf0) == 0xf0)
    {
      fmspic->idx = 0;	             // start of packet
      fmspic->len = (data & 0x0f)-1; // number of data bytes
      return IRQ_HANDLED;
    }

  // ignore data until we've seen a sync byte
  if (!fmspic->len)
      return IRQ_HANDLED;

  // store data byte
  if (fmspic->idx < FMSPIC_MAX_LENGTH && fmspic->len && fmspic->idx)
    fmspic->data[fmspic->idx] = data;

  ++fmspic->idx;

  // last byte in frame?
  if (fmspic->idx == fmspic->len)
    {
      fmspic_process_packet(fmspic);
      fmspic->idx = 0;
      fmspic->len = 0;
    }

  return IRQ_HANDLED;
}

// fmspic_disconnect() is the opposite of fmspic_connect()

static void fmspic_disconnect(struct serio *serio)
{
  struct fmspic *fmspic = serio_get_drvdata(serio);
  
  serio_close(serio);
  serio_set_drvdata(serio, NULL);
  input_unregister_device(fmspic->dev);
  kfree(fmspic);
}

#define Log() printk("fmspic %s() line %d\n", __FUNCTION__, __LINE__)

// fmspic_connect() is the routine that is called when someone adds a
// new serio device. It looks for the Twiddler, and if found,
// registers it as an input device.

static int fmspic_connect(struct serio *serio, struct serio_driver *drv)
{
  struct fmspic *fmspic;
  struct input_dev *input_dev;
  int err = -ENOMEM;

  Log();

  fmspic = kzalloc(sizeof(struct fmspic), GFP_KERNEL);
  input_dev = input_allocate_device();
  if (!fmspic || !input_dev)
    goto fail1;

  Log();

  fmspic->dev = input_dev;
  snprintf(fmspic->phys, sizeof(fmspic->phys), "%s/input0", serio->phys);

  Log();
  printk("fmspic phys='%s'\n",fmspic->phys);

  input_dev->name = "FMSPIC RC transmitter device";
  input_dev->phys = fmspic->phys;
  input_dev->id.bustype = BUS_RS232;
  input_dev->id.vendor = SERIO_ZHENHUA;
  input_dev->id.product = 0x0001;
  input_dev->id.version = 0x0100;
  input_dev->dev.parent = &serio->dev;

  input_dev->evbit[0] = BIT(EV_ABS);
  input_set_abs_params(input_dev, ABS_Z, 0, 0xf0, 0, 0);
  input_set_abs_params(input_dev, ABS_X, 0, 0xf0, 0, 0);
  input_set_abs_params(input_dev, ABS_Y, 0, 0xf0, 0, 0);
  input_set_abs_params(input_dev, ABS_RX, 0, 0xf0, 0, 0);
  input_set_abs_params(input_dev, ABS_RY, 0, 0xf0, 0, 0);
  input_set_abs_params(input_dev, ABS_RZ, 0, 0xf0, 0, 0);

  serio_set_drvdata(serio, fmspic);
  err = serio_open(serio, drv);

  Log();

  if (err)
    goto fail2;

  Log();

  err = input_register_device(fmspic->dev);
  if (err)
    goto fail3;

  Log();

  return 0;

 fail3:	serio_close(serio);
 fail2:	serio_set_drvdata(serio, NULL);
 fail1:	input_free_device(input_dev);
  kfree(fmspic);
  Log();
  return err;
}

// The serio driver structure.

static struct serio_device_id fmspic_serio_ids[] = 
  {
    {
      .type	= SERIO_RS232,
      .proto	= SERIO_ZHENHUA,
      .id	= SERIO_ANY,
      .extra	= SERIO_ANY,
    },
    { 0 }
  };

MODULE_DEVICE_TABLE(serio, fmspic_serio_ids);

static struct serio_driver fmspic_drv = 
  {
    .driver       =  { .name = "fmspic", },
    .description  = DRIVER_DESC,
    .id_table     = fmspic_serio_ids,
    .interrupt    = fmspic_interrupt,
    .connect      = fmspic_connect,
    .disconnect	  = fmspic_disconnect,
  };

// The functions for inserting/removing us as a module.

static int __init fmspic_init(void)
{
  Log();
  return serio_register_driver(&fmspic_drv);
}

static void __exit fmspic_exit(void)
{
  Log();
  serio_unregister_driver(&fmspic_drv);
}

module_init(fmspic_init);
module_exit(fmspic_exit);
