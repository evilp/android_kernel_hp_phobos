
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mach/hardware.h>
#include <linux/mfd/palmas.h>

#define MODULE_VERS "1.0"
#define MODULE_NAME "wolo_diag"

#define WOLO_IO_LEN 20

struct wolo_diag_data_t {
	char name[WOLO_IO_LEN + 1];
	char value[WOLO_IO_LEN + 1];
};


static struct proc_dir_entry *wolo_diag_dir, *wolo_diag_file;


struct wolo_diag_data_t wolo_diag_data;

#define writel(b, addr) (void)((*(volatile unsigned int *)(addr)) = (b))
#define readl(addr) 	({ unsigned int __v = (*(volatile unsigned int *)(addr)); __v; })

static int proc_read_wolo_diag(char *page, char **start,
			    off_t off, int count, 
			    int *eof, void *data)
{
	int len;
//	int earphone_insert=0, dock_det=0;
//	unsigned int reg;

//	earphone_insert = 0;	// not insert

//	len = sprintf(page, "earphone=%d\ndock_det=%d\n", earphone_insert, dock_det);
	len = sprintf(page, "no data\n");

	return len;
}

#define LED_OFF	0
#define LED_BLINK	1

extern struct palmas *palmas_dev;
void LED1_test(int mode)
{
	unsigned int save;
	unsigned int  addr;
	int slave;	

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PU_PD_OD_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD1); 
	regmap_read(palmas_dev->regmap[slave], addr, &save);
	save &= ~0x78;	// mask bit 6:3
	save |= PALMAS_PRIMARY_SECONDARY_PAD1_LED2;
	regmap_write(palmas_dev->regmap[slave], addr, save);			
	slave = PALMAS_BASE_TO_SLAVE(PALMAS_LED_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_LED_BASE, PALMAS_LED_PERIOD_CTRL);
	if(mode == 0)
		{	// off
		regmap_write(palmas_dev->regmap[slave], addr, 0x18);
		addr = PALMAS_BASE_TO_REG(PALMAS_LED_BASE, PALMAS_LED_CTRL);
		regmap_write(palmas_dev->regmap[slave], addr, 0x0C);	
		}
	else	if(mode == 1)
		{	// blink
		regmap_write(palmas_dev->regmap[slave], addr, 0x10);
		addr = PALMAS_BASE_TO_REG(PALMAS_LED_BASE, PALMAS_LED_CTRL);
		regmap_write(palmas_dev->regmap[slave], addr, 0x04);	
		}

}

void LED2_test(int mode)
{
	unsigned int save;
	unsigned int  addr;
	int slave;	

	slave = PALMAS_BASE_TO_SLAVE(PALMAS_PU_PD_OD_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD1); 
	regmap_read(palmas_dev->regmap[slave], addr, &save);
	save &= ~0x78;	// mask bit 6:3
	save |= 0x10;
	regmap_write(palmas_dev->regmap[slave], addr, save);			
	slave = PALMAS_BASE_TO_SLAVE(PALMAS_LED_BASE);
	addr = PALMAS_BASE_TO_REG(PALMAS_LED_BASE, PALMAS_LED_PERIOD_CTRL);
	if(mode == 0)
		{	// off
		regmap_write(palmas_dev->regmap[slave], addr, 0x03);
		addr = PALMAS_BASE_TO_REG(PALMAS_LED_BASE, PALMAS_LED_CTRL);
		regmap_write(palmas_dev->regmap[slave], addr, 0x03);	
		}
	else	if(mode == 1)
		{	// blink
		regmap_write(palmas_dev->regmap[slave], addr, 0x02);
		addr = PALMAS_BASE_TO_REG(PALMAS_LED_BASE, PALMAS_LED_CTRL);
		regmap_write(palmas_dev->regmap[slave], addr, 0x01);	
		}
}

static int proc_write_wolo_diag(struct file *file,
			     const char *buffer,
			     unsigned long count, 
			     void *data)
{
	int len;
	unsigned int value; //, val2;
//	unsigned int reg;

	if(count > WOLO_IO_LEN)
		len = WOLO_IO_LEN;
	else
		len = count;

	if(sscanf(buffer, "LED1=%d", &value) == 1)
		{
		if (!palmas_dev)
			goto end_ret;
		 if(value == 0)
		 	{
		 	LED1_test(LED_OFF);
		 	}
		 else if(value == 1)
		 	LED1_test(LED_BLINK);
		}		
	if(sscanf(buffer, "LED2=%d", &value) == 1)
		{
		if (!palmas_dev)
			goto end_ret;
		 if(value == 0)
		 	{
		 	LED2_test(LED_OFF);
		 	}
		 else if(value == 1)
		 	LED2_test(LED_BLINK);
		}		
	else if(sscanf(buffer, "USB=%d", &value) == 1)
		{
#define TEGRA_USB_USB2D_PORT_TEST_PACKET		0x4
#define USB_PORTSC		0x174
#define   USB_PORTSC_PTC(x)	(((x) & 0xf) << 16)
		unsigned int temp;
		if(value == 1)
			{	// get the addr from tegra_ehci_probe()
			temp = readl((const volatile void  *)(0xFE600000+USB_PORTSC));
			printk("@@@before %x\n", temp);
			writel(temp|USB_PORTSC_PTC(TEGRA_USB_USB2D_PORT_TEST_PACKET), \
				(const volatile void  *)(0xFE600000+USB_PORTSC));	// usb1 test pattern
			temp = readl((const volatile void  *)(0xFE600000+USB_PORTSC));
			printk("@@@after %x\n", temp);
			}
		else if(value == 3)
			{
			temp = readl((const volatile void  *)(0xFE608000+USB_PORTSC));
			printk("@@@before %x\n", temp);
			writel(temp|USB_PORTSC_PTC(TEGRA_USB_USB2D_PORT_TEST_PACKET), \
				(const volatile void  *)(0xFE608000+USB_PORTSC));	// usb1 test pattern
			temp = readl((const volatile void  *)(0xFE608000+USB_PORTSC));
			printk("@@@after %x\n", temp);
			}
		}
	
end_ret:
	return len;
}


static int __init init_proc_wolo_diag(void)
{
	int rv = 0;

	/* create directory */
	wolo_diag_dir = proc_mkdir(MODULE_NAME, NULL);
	if(wolo_diag_dir == NULL) {
		rv = -ENOMEM;
		goto out;
	}
	
	
	wolo_diag_file = create_proc_entry("wolo_diag", 0644, wolo_diag_dir);
	if(wolo_diag_file == NULL) {
		rv = -ENOMEM;
		goto no_wolo_io;
	}

	strcpy(wolo_diag_data.name, "wolo_diag");
	strcpy(wolo_diag_data.value, "wolo_diag");
	wolo_diag_file->data = &wolo_diag_data;
	wolo_diag_file->read_proc = proc_read_wolo_diag;
	wolo_diag_file->write_proc = proc_write_wolo_diag;
		
	/* everything OK */
	printk(KERN_INFO "/proc/%s %s initialised \n",
	       MODULE_NAME, MODULE_VERS);

	return 0;

no_wolo_io:
	remove_proc_entry("wolo_diag", wolo_diag_dir);
out:
	return rv;
}


static void __exit cleanup_proc_wolo_diag(void)
{
	remove_proc_entry("wolo_diag", wolo_diag_dir);
	remove_proc_entry(MODULE_NAME, NULL);

	printk(KERN_INFO "/proc/%s %s removed\n",
	       MODULE_NAME, MODULE_VERS);
}


module_init(init_proc_wolo_diag);
module_exit(cleanup_proc_wolo_diag);

MODULE_AUTHOR("James");
MODULE_DESCRIPTION("wolo_diag");
MODULE_LICENSE("GPL");
