/*
 * ov9740.c - ov9740 sensor driver
 *
 * Copyright (c) 2011, NVIDIA, All Rights Reserved.
 *
 * Contributors:
 *      Abhinav Sinha <absinha@nvidia.com>
 *
 * Leverage OV2710.c
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/**
 * SetMode Sequence for 1280x720. Phase 0. Sensor Dependent.
 * This sequence should put sensor in streaming mode for 1280x720
 * This is usually given by the FAE or the sensor vendor.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <media/ov9740.h>
#include <linux/module.h>

struct ov9740_reg {
    u16 addr;
    u16 val;
};

struct ov9740_info {
    int mode;
    struct i2c_client *i2c_client;
    struct ov9740_platform_data *pdata;
};

#define OV9740_TABLE_WAIT_MS 0
#define OV9740_TABLE_END 1
#define OV9740_MAX_RETRIES 3

#define OV9740_FOR_SKYPE_WEBCAM 1
#define OV9740_LED_CTRL 1
#define OV9740_PHOBOS_ORIENTATION 1

static struct ov9740_reg mode_1280x720[] = {
    /* PLL Control MIPI bit rate/lane = 448MHz, 16-bit mode.
    * Output size: 1280x720 (0, 0) - (2623, 1951),
    * Line Length = 1886, Frame Length = 990.
    */
    {0x0103, 0x01},
#ifdef OV9740_PHOBOS_ORIENTATION  
    {0x0101, 0x02}, //Orientation    //WOLO_BSP --- Flip camera degree from 0 to 180
#else
    {0x0101, 0x01}, //Orientation
#endif
    
    {OV9740_TABLE_WAIT_MS, 5},
    {0x3104, 0x20}, //PLL mode control
    {0x0305, 0x03},
    {0x0307, 0x4c},
    {0x0303, 0x01},
    {0x0301, 0x08},
    {0x3010, 0x01},
#ifndef OV9740_FOR_SKYPE_WEBCAM
    {0x300e, 0x12},
#endif    
    {OV9740_TABLE_WAIT_MS, 5},

    {0x0340, 0x03}, // ;; Timing setting: VTS
    {0x0341, 0x07},
    {0x0342, 0x06},
    {0x0343, 0x62},
    {0x0344, 0x00},
    {0x0345, 0x08},
    {0x0346, 0x00},
    {0x0347, 0x04},
    {0x0348, 0x05},
    {0x0349, 0x0c},
    {0x034a, 0x02},
    {0x034b, 0xd8},
    {0x034c, 0x05},
    {0x034d, 0x00},
    {0x034e, 0x02},
    {0x034f, 0xd0},
    {OV9740_TABLE_WAIT_MS, 5},

    {0x3002, 0x00}, // ;; Output select
    {0x3004, 0x00},
#ifdef OV9740_LED_CTRL
    {0x3005, 0x80}, /* Phobos LED control */
    {0x3009, 0x80}, /* Phobos LED control */
#else
    {0x3005, 0x00}, /* Phobos LED control */
#endif
    {0x3012, 0x70}, // MIPI control
    {0x3013, 0x60}, // MIPI control
    {0x3014, 0x01}, // MIPI control
    {0x301f, 0x03}, // MIPI control

    {0x3026, 0x00}, // ;; Output select
#ifdef OV9740_LED_CTRL
    {0x3027, 0x80}, /* Phobos LED control */
#else
    {0x3027, 0x00}, /* Phobos LED control */
#endif
    {OV9740_TABLE_WAIT_MS, 5},

    {0x3601, 0x40}, // Analog control
    {0x3602, 0x16},
    {0x3603, 0xaa},
    {0x3604, 0x0c},
    {0x3610, 0xa1},
    {0x3612, 0x24},
    {0x3620, 0x66},
    {0x3621, 0xc0},
    {0x3622, 0x9f},
    {0x3630, 0xd2},
    {0x3631, 0x5e},
    {0x3632, 0x27},
    {0x3633, 0x50},

    {0x3703, 0x42}, // Sensor control
    {0x3704, 0x10},
    {0x3705, 0x45},
    {0x3707, 0x11},

    {0x3817, 0x94}, // Timing control
    {0x3819, 0x6e},
    {0x3831, 0x40},
    {0x3833, 0x04},
    {0x3835, 0x04},
    {0x3837, 0x01},

    {0x3503, 0x10}, // AEC control
    {0x3a18, 0x01},
    {0x3a19, 0xb5},
    {0x3a1a, 0x05},
    {0x3a11, 0x90},
    {0x3a1b, 0x4a},
    {0x3a0f, 0x48},
    {0x3a10, 0x44},
    {0x3a1e, 0x42},
    {0x3a1f, 0x22},

    {0x3a08, 0x00}, // Banding filter
    {0x3a09, 0xe8},
    {0x3a0e, 0x03},
    {0x3a14, 0x15},
    {0x3a15, 0xc6},
    {0x3a0a, 0x00},
    {0x3a0b, 0xc0},
    {0x3a0d, 0x04},
    {0x3a02, 0x18},
    {0x3a03, 0x20},

    {0x3c0a, 0x9c}, // 50/60 detection
    {0x3c0b, 0x3f},

    {0x4002, 0x45}, // BLC control
    {0x4005, 0x18},

    {0x4601, 0x16}, // VFIFO control
    {0x460e, 0x82},

    {0x4702, 0x04}, // DVP control
    {0x4704, 0x00},
    {0x4706, 0x08},

    {0x4800, 0x44}, // MIPI control
    {0x4801, 0x0f},
    {0x4803, 0x05},
    {0x4805, 0x10},
    {0x4837, 0x20},

    {0x5000, 0xff}, // ISP control
    {0x5001, 0xff},
    {0x5003, 0xff},
#if 1  //WOLO_BSP --- Flip camera degree from 0 to 180
    {0x5004, 0x12},
#endif
    {0x5180, 0xf0}, // AWB
    {0x5181, 0x00},
    {0x5182, 0x41},
    {0x5183, 0x42},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5184, 0x82},
    {0x5185, 0x6a},
    {0x5186, 0xf5},
#else
    {0x5184, 0x80},
    {0x5185, 0x68},
    {0x5186, 0x93},
#endif
    {0x5187, 0xa8},
    {0x5188, 0x17},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5189, 0x3b},
#else
    {0x5189, 0x45},
#endif    
    {0x518a, 0x27},
    {0x518b, 0x41},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x518c, 0x35},
#else
    {0x518c, 0x2d},
#endif
    {0x518d, 0xf0},
    {0x518e, 0x10},
    {0x518f, 0xff},
    {0x5190, 0x00},
    {0x5191, 0xff},
    {0x5192, 0x00},
    {0x5193, 0xff},
    {0x5194, 0x00},

    {0x529a, 0x02}, // DNS
    {0x529b, 0x08},
    {0x529c, 0x0a},
    {0x529d, 0x10},
    {0x529e, 0x10},
    {0x529f, 0x28},
    {0x52a0, 0x32},
    {0x52a2, 0x00},
    {0x52a3, 0x02},
    {0x52a4, 0x00},
    {0x52a5, 0x04},
    {0x52a6, 0x00},
    {0x52a7, 0x08},
    {0x52a8, 0x00},
    {0x52a9, 0x10},
    {0x52aa, 0x00},
    {0x52ab, 0x38},
    {0x52ac, 0x00},
    {0x52ad, 0x3c},
    {0x52ae, 0x00},
    {0x52af, 0x4c},

#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x530d, 0x06}, // CIP
#else
    {0x530d, 0x0a}, // CIP
#endif

    {0x5380, 0x01}, // CMX
    {0x5381, 0x00},
    {0x5382, 0x00},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5383, 0x17},
#else
    {0x5383, 0x0d},
#endif
    {0x5384, 0x00},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5385, 0x01},
#else
    {0x5385, 0x2f},
#endif    
    {0x5386, 0x00},
    {0x5387, 0x00},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5388, 0x00},
    {0x5389, 0xf2},
#else
    {0x5388, 0x00},
    {0x5389, 0xd3},
#endif    
    {0x538a, 0x00},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x538b, 0x22},
#else
    {0x538b, 0x0f},
#endif    
    {0x538c, 0x00},
    {0x538d, 0x00},
    {0x538e, 0x00},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x538f, 0x17},
#else
    {0x538f, 0x32},
#endif
    {0x5390, 0x00},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5391, 0xa8},
#else
    {0x5391, 0x94},
#endif    
    {0x5392, 0x00},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5393, 0xa0},
#else
    {0x5393, 0xa4},
#endif
    {0x5394, 0x18},

#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5401, 0x2c},
    {0x5403, 0x28},
    {0x5404, 0x06},
    {0x5405, 0xe0},
#endif

#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5480, 0x0a}, // Y Gamma
    {0x5481, 0x19},
    {0x5482, 0x2d},
    {0x5483, 0x4e},
    {0x5484, 0x59},
    {0x5485, 0x67},
#else
    {0x5480, 0x04}, // Y Gamma
    {0x5481, 0x12},
    {0x5482, 0x27},
    {0x5483, 0x49},
    {0x5484, 0x57},
    {0x5485, 0x66},
#endif
    {0x5486, 0x75},
    {0x5487, 0x81},
    {0x5488, 0x8c},
    {0x5489, 0x95},
    {0x548a, 0xa5},
    {0x548b, 0xb2},
    {0x548c, 0xc8},
    {0x548d, 0xd9},
    {0x548e, 0xec},


    {0x5490, 0x01}, // UV Gamma
    {0x5491, 0xc0},
    {0x5492, 0x03},
    {0x5493, 0x00},
    {0x5494, 0x03},
    {0x5495, 0xe0},
    {0x5496, 0x03},
    {0x5497, 0x10},
    {0x5498, 0x02},
    {0x5499, 0xac},
    {0x549a, 0x02},
    {0x549b, 0x75},
    {0x549c, 0x02},
    {0x549d, 0x44},
    {0x549e, 0x02},
    {0x549f, 0x20},
    {0x54a0, 0x02},
    {0x54a1, 0x07},
    {0x54a2, 0x01},
    {0x54a3, 0xec},
    {0x54a4, 0x01},
    {0x54a5, 0xc0},
    {0x54a6, 0x01},
    {0x54a7, 0x9b},
    {0x54a8, 0x01},
    {0x54a9, 0x63},
    {0x54aa, 0x01},
    {0x54ab, 0x2b},
    {0x54ac, 0x01},
    {0x54ad, 0x22},

    {0x5501, 0x1c}, // UV adjust
    {0x5502, 0x00},
    {0x5503, 0x40},
    {0x5504, 0x00},
    {0x5505, 0x80},

#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5800, 0x3f}, // LENC setting
    {0x5801, 0x2c},
    {0x5802, 0x25},
    {0x5803, 0x26},
    {0x5804, 0x2c},
    {0x5805, 0x3f},
    {0x5806, 0x10},
    {0x5807, 0x0d},
    {0x5808, 0x0a},
    {0x5809, 0x0a},
    {0x580a, 0x0d},
    {0x580b, 0x18},
    {0x580c, 0x06},
    {0x580d, 0x03},
#else
    {0x5800, 0x1c}, // LENC setting
    {0x5801, 0x16},
    {0x5802, 0x15},
    {0x5803, 0x16},
    {0x5804, 0x18},
    {0x5805, 0x1a},
    {0x5806, 0x0c},
    {0x5807, 0x0a},
    {0x5808, 0x08},
    {0x5809, 0x08},
    {0x580a, 0x0a},
    {0x580b, 0x0b},
    {0x580c, 0x05},
    {0x580d, 0x02},
#endif
    {0x580e, 0x00},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x580f, 0x01},
    {0x5810, 0x04},
    {0x5811, 0x08},
    {0x5812, 0x05},
    {0x5813, 0x02},
#else
    {0x580f, 0x00},
    {0x5810, 0x02},
    {0x5811, 0x05},
    {0x5812, 0x04},
    {0x5813, 0x01},
#endif
    {0x5814, 0x00},
    {0x5815, 0x00},
    {0x5816, 0x02},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5817, 0x09},
    {0x5818, 0x0d},
    {0x5819, 0x09},
    {0x581a, 0x06},
    {0x581b, 0x07},
    {0x581c, 0x0a},
    {0x581d, 0x11},
    {0x581e, 0x2d},
    {0x581f, 0x1f},
    {0x5820, 0x1a},
    {0x5821, 0x1c},
    {0x5822, 0x21},
    {0x5823, 0x37},
    {0x5824, 0x66},
    {0x5825, 0x8e},
#else
    {0x5817, 0x03},
    {0x5818, 0x0a},
    {0x5819, 0x07},
    {0x581a, 0x05},
    {0x581b, 0x05},
    {0x581c, 0x08},
    {0x581d, 0x0b},
    {0x581e, 0x15},
    {0x581f, 0x14},
    {0x5820, 0x14},
    {0x5821, 0x13},
    {0x5822, 0x17},
    {0x5823, 0x16},
    {0x5824, 0x46},
    {0x5825, 0x4c},
#endif
    {0x5826, 0x6c},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x5827, 0x8a},
    {0x5828, 0x88},
    {0x5829, 0x4e},
#else
    {0x5827, 0x4c},
    {0x5828, 0x80},
    {0x5829, 0x2e},
#endif

#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x582a, 0x68},
    {0x582b, 0x68},
    {0x582c, 0x6a},
    {0x582d, 0x6a},
    {0x582e, 0x48},
    {0x582f, 0x82},
    {0x5830, 0x80},
    {0x5831, 0x82},
    {0x5832, 0x66},
    {0x5833, 0x4e},
    {0x5834, 0x66},
    {0x5835, 0x66},
    {0x5836, 0x66},
    {0x5837, 0x68},
    {0x5838, 0x0a},
    {0x5839, 0x4e},
#else
    {0x582a, 0x48},
    {0x582b, 0x46},
    {0x582c, 0x2a},
    {0x582d, 0x68},
    {0x582e, 0x08},
    {0x582f, 0x26},
    {0x5830, 0x44},
    {0x5831, 0x46},
    {0x5832, 0x62},
    {0x5833, 0x0c},
    {0x5834, 0x28},
    {0x5835, 0x46},
    {0x5836, 0x28},
    {0x5837, 0x88},
    {0x5838, 0x0e},
    {0x5839, 0x0e},
#endif
    {0x583a, 0x2c},
#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x583b, 0x4c},
    {0x583c, 0x88},
    {0x583d, 0x8e},
#else
    {0x583b, 0x2e},
    {0x583c, 0x46},
    {0x583d, 0xca},
#endif
    {0x583e, 0xf0},
    {0x5842, 0x02},
    {0x5843, 0x5e},
    {0x5844, 0x04},
    {0x5845, 0x32},
    {0x5846, 0x03},
    {0x5847, 0x29},
    {0x5848, 0x02},
    {0x5849, 0xcc},

#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x3a00, 0x7c}, // night mode 1/2
    {0x3a02, 0x06},
    {0x3a03, 0x0e},
#endif

#ifdef OV9740_FOR_SKYPE_WEBCAM
    {0x530d, 0x07}, // Sharpness 1-1
    {0x530c, 0x01},
    {0x529a, 0x01},
    {0x529b, 0x04},
    {0x529c, 0x08},
    {0x529d, 0x09},
    {0x529e, 0x09},
#endif

//#ifndef OV9740_FOR_SKYPE_WEBCAM
    {0x5019, 0x02},  // Change YUV order
//#endif
    {0x0100, 0x01},    // start streaming
    {OV9740_TABLE_END, 0x0000}
};


enum {
    OV9740_MODE_1280x720,
};

static struct ov9740_reg *mode_table[] = {
    [OV9740_MODE_1280x720] = mode_1280x720,
};

static int ov9740_read_reg(struct i2c_client *client, u16 addr, u8 *val)
{
    int err;
    struct i2c_msg msg[2];
    unsigned char data[3];

    if (!client->adapter)
        return -ENODEV;

    msg[0].addr = client->addr;
    msg[0].flags = 0;
    msg[0].len = 2;
    msg[0].buf = data;

    /* high byte goes out first */
    data[0] = (u8) (addr >> 8);
    data[1] = (u8) (addr & 0xff);

    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = data + 2;

    err = i2c_transfer(client->adapter, msg, 2);

    if (err != 2)
        return -EINVAL;

    *val = data[2] ;

    return 0;
}

static int ov9740_write_reg(struct i2c_client *client, u16 addr, u8 val)
{
    int err;
    struct i2c_msg msg;
    unsigned char data[3];
    int retry = 0;

    if (!client->adapter)
        return -ENODEV;

    data[0] = (u8) (addr >> 8);
    data[1] = (u8) (addr & 0xff);
    data[2] = (u8) (val & 0xff);

    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = 3;
    msg.buf = data;

    do {
        err = i2c_transfer(client->adapter, &msg, 1);
        if (err == 1)
            return 0;
        retry++;
        pr_err("ov9740: i2c transfer failed, retrying %x %x\n",
               addr, val);
        msleep(3);
    } while (retry <= OV9740_MAX_RETRIES);

    return err;
}

static int ov9740_write_table(struct i2c_client *client,
                  const struct ov9740_reg table[],
                  const struct ov9740_reg override_list[],
                  int num_override_regs)
{
    int err;
    const struct ov9740_reg *next;
    int i;
    u16 val;

    pr_info("%s!\n", __func__);

    for (next = table; next->addr != OV9740_TABLE_END; next++) {
        if (next->addr == OV9740_TABLE_WAIT_MS) {
            msleep(next->val);
            continue;
        }

        val = next->val;

        /* When an override list is passed in, replace the reg */
        /* value to write if the reg is in the list            */
        if (override_list) {
            for (i = 0; i < num_override_regs; i++) {
                if (next->addr == override_list[i].addr) {
                    val = override_list[i].val;
                    break;
                }
            }
        }

        err = ov9740_write_reg(client, next->addr, val);
        //pr_info("%s! ov9740_write_reg: %u\n", __func__, next->addr);

        if (err)
            return err;
    }
    return 0;
}

static int ov9740_set_mode(struct ov9740_info *info, struct ov9740_mode *mode)
{
    int sensor_mode;
    int err;

    pr_info("%s: xres %u yres %u\n", __func__, mode->xres, mode->yres);
    if (mode->xres == 1280 && mode->yres == 720)
        sensor_mode = OV9740_MODE_1280x720;
    else {
        pr_err("%s: invalid resolution supplied to set mode %d %d\n",
               __func__, mode->xres, mode->yres);
        return -EINVAL;
    }

    err = ov9740_write_table(info->i2c_client, mode_table[sensor_mode],
        NULL, 0);
    if (err)
        return err;

    info->mode = sensor_mode;
    return 0;
}

static int ov9740_get_status(struct ov9740_info *info,
        struct ov9740_status *dev_status)
{
    int err;

	pr_info("ov9740: ov9740_get_status\n");

    err = ov9740_read_reg(info->i2c_client, 0x003, (u8 *) &dev_status->data);

	pr_info("ov9740: ov9740_get_status %x, %x\n",err, dev_status->data);

    return err;
}

static long ov9740_ioctl(struct file *file,
             unsigned int cmd, unsigned long arg)
{
    int err;
    struct ov9740_info *info = file->private_data;
    pr_info("ov9740: ov9740_ioctl\n");
    switch (cmd) {
    case OV9740_IOCTL_SET_MODE:
    {
        struct ov9740_mode mode;
    pr_info("ov9740: OV9740_IOCTL_SET_MODE\n");
        if (copy_from_user(&mode,
                   (const void __user *)arg,
                   sizeof(struct ov9740_mode))) {
            return -EFAULT;
        }

        return ov9740_set_mode(info, &mode);
    }
    case OV9740_IOCTL_GET_STATUS:
    {
        struct ov9740_status dev_status;
    pr_info("ov9740: OV9740_IOCTL_GET_STATUS\n");
        if (copy_from_user(&dev_status,
                   (const void __user *)arg,
                   sizeof(struct ov9740_status))) {
            return -EFAULT;
        }

        err = ov9740_get_status(info, &dev_status);
        if (err)
            return err;
        if (copy_to_user((void __user *)arg, &dev_status,
                 sizeof(struct ov9740_status))) {
            return -EFAULT;
        }
        return 0;
    }
// add this case to read register --Kevin 20130326
    case OV9740_IOCTL_READ_REG:
    {
	struct ov9740_reg *read_info;

	read_info = (struct ov9740_reg *)kzalloc(sizeof (struct ov9740_reg), GFP_KERNEL);

	if (copy_from_user(read_info, (const void __user *)arg, 
			sizeof(struct ov9740_reg))){
		return -EFAULT;
	}

	ov9740_read_reg(info->i2c_client, read_info->addr, (u8 *)&(read_info->val)); // read regster
	pr_info("---------------> ov9740: read addr 0x%04x is 0x%02x \n", read_info->addr, read_info->val);

	if (copy_to_user((void __user*)arg, read_info,
			sizeof(struct ov9740_reg))){
		return -EFAULT;
	}
	
	kfree(read_info);
	read_info = NULL;
	
	return 0;
    }


    default:
        return -EINVAL;
    }
    return 0;
}

static struct ov9740_info *info;

static void ov9740_led_ctl(struct ov9740_info *info, u16 addr, u8 val){
    int err;
    err = ov9740_write_reg(info->i2c_client, addr, val);
    if(err)     pr_info("ov9740: ov9740_led_ctl, err: %d\n", err);
}

static int ov9740_open(struct inode *inode, struct file *file)
{
    struct ov9740_status dev_status;
    int err;

    pr_info("ov9740: ov9740_open\n");
	
    file->private_data = info;
    if (info->pdata && info->pdata->power_on)
        info->pdata->power_on();

		{
			int id=0;
			ov9740_read_reg(info->i2c_client, 0x000, (u8 *) &id);
			pr_info("---------------> ov9740: read id 1  %x \n", id);
			ov9740_read_reg(info->i2c_client, 0x001, (u8 *) &id);
			pr_info("---------------> ov9740: read id 2 %x \n", id);			
			}	

    dev_status.data = 0;
    dev_status.status = 0;
    err = ov9740_get_status(info, &dev_status);

    /* Turn on Led */
    ov9740_led_ctl(info, 0x3009, 0x80);
    return err;
}

int ov9740_release(struct inode *inode, struct file *file)
{
    pr_info("ov9740: ov9740_release\n");
	
    /* Turn off Led */
    ov9740_led_ctl(info, 0x3009, 0x00);
	
    if (info->pdata && info->pdata->power_off)
        info->pdata->power_off();
    file->private_data = NULL;
    return 0;
}

static const struct file_operations ov9740_fileops = {
    .owner = THIS_MODULE,
    .open = ov9740_open,
    .unlocked_ioctl = ov9740_ioctl,
    .release = ov9740_release,
};

static struct miscdevice ov9740_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "ov9740",
    .fops = &ov9740_fileops,
};

static int ov9740_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    int err;

    pr_info("ov9740: probing sensor.\n");

    info = kzalloc(sizeof(struct ov9740_info), GFP_KERNEL);
    if (!info) {
        pr_err("ov9740: Unable to allocate memory!\n");
        return -ENOMEM;
    }

    err = misc_register(&ov9740_device);
    if (err) {
        pr_err("ov9740: Unable to register misc device!\n");
        kfree(info);
        return err;
    }

    info->pdata = client->dev.platform_data;
    info->i2c_client = client;

    i2c_set_clientdata(client, info);
    return 0;
}

static int ov9740_remove(struct i2c_client *client)
{
    struct ov9740_info *info;
    info = i2c_get_clientdata(client);
    misc_deregister(&ov9740_device);
    kfree(info);
    return 0;
}

static const struct i2c_device_id ov9740_id[] = {
    { "ov9740", 0 },
    { },
};

MODULE_DEVICE_TABLE(i2c, ov9740_id);

static struct i2c_driver ov9740_i2c_driver = {
    .driver = {
        .name = "ov9740",
        .owner = THIS_MODULE,
    },
    .probe = ov9740_probe,
    .remove = ov9740_remove,
    .id_table = ov9740_id,
};

static int __init ov9740_init(void)
{
    pr_info("ov9740 sensor driver loading\n");
    return i2c_add_driver(&ov9740_i2c_driver);
}

static void __exit ov9740_exit(void)
{
    i2c_del_driver(&ov9740_i2c_driver);
}

module_init(ov9740_init);
module_exit(ov9740_exit);
MODULE_LICENSE("GPL v2");
