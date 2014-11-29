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


//==============================================================================
static void pi64_complete(void* context){
        input_report_key(pi64_dev, BTN_0, buf[0] & 0x1);
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
	kt_periode = ktime_set(1, 0); //seconds,nanoseconds
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
	pi64_dev->evbit[0] = BIT_MASK(EV_KEY);
	pi64_dev->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);

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
	
