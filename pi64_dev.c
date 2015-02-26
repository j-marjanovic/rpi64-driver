/*
 * SPI protocol driver fo Pi64 project
 *
 * Copyright (C) 2014 Jan Marjanovic <jan@marjanovic.pro>
 *
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/hrtimer.h>


#define MOD_NAME	"pi64_dev"

static struct input_dev *pi64_dev;
static struct spi_device *pi64_spi_dev;
static struct hrtimer htimer;
static ktime_t kt_periode;

char buf[16];
struct spi_message msg;
struct spi_transfer xfer;

#define N64_ABS_MAX	127

#define MASK_BTN_A	(0x80)
#define MASK_BTN_B	(0x40)
#define MASK_BTN_Z	(0x20)
#define MASK_BTN_START	(0x10)
#define MASK_BTN_TR	(0x1000)
#define MASK_BTN_TL	(0x2000)
#define MASK_DPAD_UP	(0x8)
#define MASK_DPAD_DOWN	(0x4)
#define MASK_DPAD_LEFT	(0x2)
#define MASK_DPAD_RIGHT	(0x1)
#define MASK_KEY_UP	(0x800)
#define MASK_KEY_DOWN	(0x400)
#define MASK_KEY_LEFT	(0x200)
#define MASK_KEY_RIGHT	(0x100)

#define ABS_TO_INT(x)	(x < 128 ? x : x - 255)

//==============================================================================
static void pi64_complete(void* context){

	uint32_t ctrl0 = *(uint32_t*)&buf[0];

	input_report_key(pi64_dev, BTN_A,          ctrl0 & MASK_BTN_A );
	input_report_key(pi64_dev, BTN_B,          ctrl0 & MASK_BTN_B );
	input_report_key(pi64_dev, BTN_Z,          ctrl0 & MASK_BTN_Z );
	input_report_key(pi64_dev, BTN_START,      ctrl0 & MASK_BTN_START );
	input_report_key(pi64_dev, BTN_TR,         ctrl0 & MASK_BTN_TR );
	input_report_key(pi64_dev, BTN_TL,         ctrl0 & MASK_BTN_TL );
	input_report_key(pi64_dev, BTN_DPAD_UP,    ctrl0 & MASK_DPAD_UP );
	input_report_key(pi64_dev, BTN_DPAD_DOWN,  ctrl0 & MASK_DPAD_DOWN );
	input_report_key(pi64_dev, BTN_DPAD_LEFT,  ctrl0 & MASK_DPAD_LEFT );
	input_report_key(pi64_dev, BTN_DPAD_RIGHT, ctrl0 & MASK_DPAD_RIGHT );
	input_report_key(pi64_dev, KEY_UP,         ctrl0 & MASK_KEY_UP );
	input_report_key(pi64_dev, KEY_DOWN,       ctrl0 & MASK_KEY_DOWN );
	input_report_key(pi64_dev, KEY_LEFT,       ctrl0 & MASK_KEY_LEFT );
	input_report_key(pi64_dev, KEY_RIGHT,      ctrl0 & MASK_KEY_RIGHT );

	input_report_abs(pi64_dev, ABS_X, ABS_TO_INT(buf[3]));
	input_report_abs(pi64_dev, ABS_Y, ABS_TO_INT(buf[2]));
		
        input_sync(pi64_dev);
}

//=============================================================================
static int read_from_spi(void){
	int rc = 0;

        memset(&xfer, 0, sizeof(struct spi_transfer));
        xfer.rx_buf = (void*)buf;
        xfer.len = 4;

        spi_message_init_with_transfers(&msg, &xfer, 1);
        msg.complete = pi64_complete;
        rc = spi_async(pi64_spi_dev, &msg);
	return rc;
}

//==============================================================================
static void timer_cleanup(void){
	hrtimer_cancel(& htimer);
}

//==============================================================================
static enum hrtimer_restart timer_function(struct hrtimer * timer){
	int rc = 0;
	
	rc = read_from_spi();
	if(rc) {
		printk(KERN_DEBUG "read_from_spi() failed: %d\n", rc);
		return HRTIMER_NORESTART;
	}

	hrtimer_forward_now(timer, kt_periode);
	return HRTIMER_RESTART;
}

//==============================================================================
static void timer_init(void){
	kt_periode = ktime_set(0, 20*1000*1000); //seconds,nanoseconds
	hrtimer_init (& htimer, CLOCK_REALTIME, HRTIMER_MODE_REL);
	htimer.function = timer_function;
	hrtimer_start(& htimer, kt_periode, HRTIMER_MODE_REL);
}


//=============================================================================
static int pi64_probe(struct spi_device *spi){
	int rc = 0;

	pi64_spi_dev = spi;

	pi64_dev = input_allocate_device();
	if(!pi64_dev) {
		printk(KERN_ERR MOD_NAME ": Could not allocate memory\n");
		return -ENOMEM;
	}

	pi64_dev->name  = MOD_NAME;
	pi64_dev->evbit[0] = BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	pi64_dev->keybit[BIT_WORD(BTN_A)]          |= BIT_MASK(BTN_A);
	pi64_dev->keybit[BIT_WORD(BTN_B)]          |= BIT_MASK(BTN_B);
	pi64_dev->keybit[BIT_WORD(BTN_Z)]          |= BIT_MASK(BTN_Z);
	pi64_dev->keybit[BIT_WORD(BTN_START)]      |= BIT_MASK(BTN_START);
	pi64_dev->keybit[BIT_WORD(BTN_TR)]         |= BIT_MASK(BTN_TR);
	pi64_dev->keybit[BIT_WORD(BTN_TL)]         |= BIT_MASK(BTN_TL);
	pi64_dev->keybit[BIT_WORD(BTN_DPAD_UP)]    |= BIT_MASK(BTN_DPAD_UP);
	pi64_dev->keybit[BIT_WORD(BTN_DPAD_DOWN)]  |= BIT_MASK(BTN_DPAD_DOWN);
	pi64_dev->keybit[BIT_WORD(BTN_DPAD_LEFT)]  |= BIT_MASK(BTN_DPAD_LEFT);
	pi64_dev->keybit[BIT_WORD(BTN_DPAD_RIGHT)] |= BIT_MASK(BTN_DPAD_RIGHT);
	pi64_dev->keybit[BIT_WORD(KEY_UP)]         |= BIT_MASK(KEY_UP);
	pi64_dev->keybit[BIT_WORD(KEY_DOWN)]       |= BIT_MASK(KEY_DOWN);
	pi64_dev->keybit[BIT_WORD(KEY_LEFT)]       |= BIT_MASK(KEY_LEFT);
	pi64_dev->keybit[BIT_WORD(KEY_RIGHT)]      |= BIT_MASK(KEY_RIGHT);

	input_set_abs_params(pi64_dev, ABS_X, -N64_ABS_MAX, N64_ABS_MAX, 0, 0);
	input_set_abs_params(pi64_dev, ABS_Y, -N64_ABS_MAX, N64_ABS_MAX, 0, 0);

	rc = input_register_device(pi64_dev);
	if (rc) {
		printk(KERN_ERR MOD_NAME ": Could not register input dev\n");
	}

	timer_init();

	return 0;
}

//=============================================================================
static int pi64_remove(struct spi_device *spi){
	
	timer_cleanup();
	input_unregister_device(pi64_dev);
	input_free_device(pi64_dev);

	return 0;
}

static struct spi_driver pi64_driver = {
	.driver = {
		.name		= MOD_NAME,
		.owner		= THIS_MODULE,
	},
	.probe		= pi64_probe,
	.remove		= pi64_remove,
};

//==============================================================================
int __init pi64_init(void)
{
	int rc;
	printk(KERN_DEBUG "============================================\n");
	printk(KERN_DEBUG "       Pi64 driver by Jan Marjanovic        \n");
	printk(KERN_DEBUG "\n");
	printk(KERN_DEBUG "  built: %s %s\n", __DATE__, __TIME__);
	printk(KERN_DEBUG "============================================\n");
	
	rc = spi_register_driver(&pi64_driver);
	return 0;
}

//==============================================================================
void  __exit pi64_cleanup(void)
{
	spi_unregister_driver(&pi64_driver);
	printk(KERN_DEBUG MOD_NAME ": exit\n");
}  


module_init(pi64_init);
module_exit(pi64_cleanup);


MODULE_AUTHOR("Jan Marjanovic <jan@marjanovic.pro>");
MODULE_DESCRIPTION("SPI protocol driver fo Pi64 project");
MODULE_LICENSE("GPL");
	
