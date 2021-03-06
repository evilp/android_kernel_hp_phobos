/* Copyright (C) 2012 Invensense, Inc.
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#include <linux/mpu.h>


#define AKM_NAME			"akm89xx"
#define AKM_HW_DELAY_POR_MS		(50)
#define AKM_HW_DELAY_TSM_MS		(10)	/* Time Single Measurement */
#define AKM_HW_DELAY_US			(100)
#define AKM_HW_DELAY_ROM_ACCESS_US	(200)
#define AKM_POLL_DELAY_MS_DFLT		(200)
#define AKM_MPU_RETRY_COUNT		(20)
#define AKM_MPU_RETRY_DELAY_MS		(20)
#define AKM_ERR_CNT_MAX			(20)

#define AKM_INPUT_RESOLUTION		(1)
#define AKM_INPUT_DIVISOR		(1)
#define AKM_INPUT_DELAY_MS_MIN		(AKM_HW_DELAY_TSM_MS)
#define AKM_INPUT_POWER_UA		(10000)
#define AKM_INPUT_RANGE			(4912)

#define AKM_REG_WIA			(0x00)
#define AKM_WIA_ID			(0x48)
#define AKM_REG_INFO			(0x01)
#define AKM_REG_ST1			(0x02)
#define AKM_ST1_DRDY			(0x01)
#define AKM_ST1_DOR			(0x02)
#define AKM_REG_HXL			(0x03)
#define AKM_REG_HXH			(0x04)
#define AKM_REG_HYL			(0x05)
#define AKM_REG_HYH			(0x06)
#define AKM_REG_HZL			(0x07)
#define AKM_REG_HZH			(0x08)
#define AKM_REG_ST2			(0x09)
#define AKM_ST2_DERR			(0x04)
#define AKM_ST2_HOFL			(0x08)
#define AKM_ST2_BITM			(0x10)
#define AKM_REG_CNTL1			(0x0A)
#define AKM_CNTL1_MODE_MASK		(0x0F)
#define AKM_CNTL1_MODE_POWERDOWN	(0x00)
#define AKM_CNTL1_MODE_SINGLE		(0x01)
#define AKM_CNTL1_MODE_CONT1		(0x02)
#define AKM_CNTL1_MODE_CONT2		(0x06)
#define AKM_CNTL1_MODE_SELFTEST		(0x08)
#define AKM_CNTL1_MODE_ROM_ACCESS	(0x0F)
#define AKM_CNTL1_OUTPUT_BIT16		(0x10)
#define AKM_REG_CNTL2			(0x0B)
#define AKM_CNTL2_SRST			(0x01)
#define AKM_REG_ASTC			(0x0C)
#define AKM_REG_TS1			(0x0D)
#define AKM_REG_TS2			(0x0E)
#define AKM_REG_I2CDIS			(0x0F)
#define AKM_REG_ASAX			(0x10)
#define AKM_REG_ASAY			(0x11)
#define AKM_REG_ASAZ			(0x12)

#define AKM8963_SCALE14			(19661)
#define AKM8963_SCALE16			(4915)
#define AKM8972_SCALE			(19661)
#define AKM8975_SCALE			(9830)
#define AKM8975_RANGE_X_HI		(100)
#define AKM8975_RANGE_X_LO		(-100)
#define AKM8975_RANGE_Y_HI		(100)
#define AKM8975_RANGE_Y_LO		(-100)
#define AKM8975_RANGE_Z_HI		(-300)
#define AKM8975_RANGE_Z_LO		(-1000)
#define AKM8972_RANGE_X_HI		(50)
#define AKM8972_RANGE_X_LO		(-50)
#define AKM8972_RANGE_Y_HI		(50)
#define AKM8972_RANGE_Y_LO		(-50)
#define AKM8972_RANGE_Z_HI		(-100)
#define AKM8972_RANGE_Z_LO		(-500)
#define AKM8963_RANGE_X_HI		(200)
#define AKM8963_RANGE_X_LO		(-200)
#define AKM8963_RANGE_Y_HI		(200)
#define AKM8963_RANGE_Y_LO		(-200)
#define AKM8963_RANGE_Z_HI		(-800)
#define AKM8963_RANGE_Z_LO		(-3200)
#define AXIS_X				(0)
#define AXIS_Y				(1)
#define AXIS_Z				(2)
#define WR				(0)
#define RD				(1)


/* regulator names in order of powering on */
static char *akm_vregs[] = {
	"vdd",
	"vid",
};


struct akm_asa {
	u8 asa[3];			/* axis sensitivity adjustment */
	s16 range_lo[3];
	s16 range_hi[3];
};

struct akm_inf {
	struct i2c_client *i2c;
	struct mutex mutex_data;
	struct input_dev *idev;
	struct workqueue_struct *wq;
	struct delayed_work dw;
	struct regulator_bulk_data vreg[ARRAY_SIZE(akm_vregs)];
	struct mpu_platform_data pdata;
	struct akm_asa asa;		/* data for calibration */
	unsigned int compass_id;	/* compass id */
	unsigned long poll_delay_us;	/* requested sampling delay (us) */
	unsigned int resolution;	/* report when new data outside this */
	bool use_mpu;			/* if device behind MPU */
	bool initd;			/* set if initialized */
	bool enable;			/* enable status */
	bool port_en[2];		/* enable status of MPU write port */
	int port_id[2];			/* MPU port ID */
	u8 data_out;			/* write value to trigger a sample */
	u8 range_index;			/* max_range index */
	short xyz[3];			/* sample data */
	bool cycle;			/* cycle MPU en/dis for high speed */
	unsigned long cycle_delay_us;	/* cycle off time (us) */
	s64 cycle_ts;			/* cycle MPU disable timestamp */
};


static int akm_i2c_rd(struct akm_inf *inf, u8 reg, u16 len, u8 *val)
{
	struct i2c_msg msg[2];

	msg[0].addr = inf->i2c->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;
	msg[1].addr = inf->i2c->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = len;
	msg[1].buf = val;
	if (i2c_transfer(inf->i2c->adapter, msg, 2) != 2)
		return -EIO;

	return 0;
}

static int akm_i2c_wr(struct akm_inf *inf, u8 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[2];

	buf[0] = reg;
	buf[1] = val;
	msg.addr = inf->i2c->addr;
	msg.flags = 0;
	msg.len = 2;
	msg.buf = buf;
	if (i2c_transfer(inf->i2c->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int akm_vreg_dis(struct akm_inf *inf, int i)
{
	int err = 0;

	if (inf->vreg[i].ret && (inf->vreg[i].consumer != NULL)) {
		err = regulator_disable(inf->vreg[i].consumer);
		if (err)
			dev_err(&inf->i2c->dev, "%s %s ERR\n",
				__func__, inf->vreg[i].supply);
		else
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
	}
	inf->vreg[i].ret = 0;
	return err;
}

static int akm_vreg_dis_all(struct akm_inf *inf)
{
	unsigned int i;
	int err = 0;

	for (i = ARRAY_SIZE(akm_vregs); i > 0; i--)
		err |= akm_vreg_dis(inf, (i - 1));
	return err;
}

static int akm_vreg_en(struct akm_inf *inf, int i)
{
	int err = 0;

	if ((!inf->vreg[i].ret) && (inf->vreg[i].consumer != NULL)) {
		err = regulator_enable(inf->vreg[i].consumer);
		if (!err) {
			inf->vreg[i].ret = 1;
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
			err = 1; /* flag regulator state change */
		} else {
			dev_err(&inf->i2c->dev, "%s %s ERR\n",
				__func__, inf->vreg[i].supply);
		}
	}
	return err;
}

static int akm_vreg_en_all(struct akm_inf *inf)
{
	int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(akm_vregs); i++)
		err |= akm_vreg_en(inf, i);
	return err;
}

static void akm_vreg_exit(struct akm_inf *inf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(akm_vregs); i++) {
		if (inf->vreg[i].consumer != NULL) {
			devm_regulator_put(inf->vreg[i].consumer);
			inf->vreg[i].consumer = NULL;
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
		}
	}
}

static int akm_vreg_init(struct akm_inf *inf)
{
	unsigned int i;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(akm_vregs); i++) {
		inf->vreg[i].supply = akm_vregs[i];
		inf->vreg[i].ret = 0;
		inf->vreg[i].consumer = devm_regulator_get(&inf->i2c->dev,
							  inf->vreg[i].supply);
		if (IS_ERR(inf->vreg[i].consumer)) {
			err |= PTR_ERR(inf->vreg[i].consumer);
			dev_err(&inf->i2c->dev, "%s err %d for %s\n",
				__func__, err, inf->vreg[i].supply);
			inf->vreg[i].consumer = NULL;
		} else {
			dev_dbg(&inf->i2c->dev, "%s %s\n",
				__func__, inf->vreg[i].supply);
		}
	}
	return err;
}

static int akm_pm(struct akm_inf *inf, bool enable)
{
	int err;

	if (enable) {
		err = akm_vreg_en_all(inf);
		if (err)
			mdelay(AKM_HW_DELAY_POR_MS);
	} else {
		err = akm_vreg_dis_all(inf);
	}
	if (err > 0)
		err = 0;
	if (err)
		dev_err(&inf->i2c->dev, "%s enable=%x ERR=%d\n",
			__func__, enable, err);
	else
		dev_dbg(&inf->i2c->dev, "%s enable=%x\n",
			__func__, enable);
	return err;
}

static int akm_port_free(struct akm_inf *inf, int port)
{
	int err = 0;

	if ((inf->use_mpu) && (inf->port_id[port] >= 0)) {
		err = nvi_mpu_port_free(inf->port_id[port]);
		if (!err)
			inf->port_id[port] = -1;
	}
	return err;
}

static int akm_ports_free(struct akm_inf *inf)
{
	int err;

	err = akm_port_free(inf, WR);
	err |= akm_port_free(inf, RD);
	return err;
}

static void akm_pm_exit(struct akm_inf *inf)
{
	akm_ports_free(inf);
	akm_pm(inf, false);
	akm_vreg_exit(inf);
}

static int akm_pm_init(struct akm_inf *inf)
{
	int err;

	inf->initd = false;
	inf->enable = false;
	inf->port_en[WR] = false;
	inf->port_en[RD] = false;
	inf->port_id[WR] = -1;
	inf->port_id[RD] = -1;
	inf->poll_delay_us = (AKM_POLL_DELAY_MS_DFLT * 1000);
	inf->cycle_delay_us = (AKM_INPUT_DELAY_MS_MIN * 1000);
	akm_vreg_init(inf);
	err = akm_pm(inf, true);
	return err;
}

static int akm_nvi_mpu_bypass_request(struct akm_inf *inf)
{
	int i;
	int err = 0;

	if (inf->use_mpu) {
		for (i = 0; i < AKM_MPU_RETRY_COUNT; i++) {
			err = nvi_mpu_bypass_request(true);
			if ((!err) || (err == -EPERM))
				break;

			mdelay(AKM_MPU_RETRY_DELAY_MS);
		}
		if (err == -EPERM)
			err = 0;
	}
	return err;
}

static int akm_nvi_mpu_bypass_release(struct akm_inf *inf)
{
	int err = 0;

	if (inf->use_mpu)
		err = nvi_mpu_bypass_release();
	return err;
}

static int akm_wr(struct akm_inf *inf, u8 reg, u8 val)
{
	int err = 0;

	err = akm_nvi_mpu_bypass_request(inf);
	if (!err) {
		err = akm_i2c_wr(inf, AKM_REG_CNTL1, AKM_CNTL1_MODE_POWERDOWN);
		udelay(AKM_HW_DELAY_US);
		err |= akm_i2c_wr(inf, reg, val);
		akm_nvi_mpu_bypass_release(inf);
	}
	return err;
}

static int akm_port_enable(struct akm_inf *inf, int port, bool enable)
{
	int err = 0;

	if (enable != inf->port_en[port]) {
		err = nvi_mpu_enable(inf->port_id[port], enable, false);
		if (!err)
			inf->port_en[port] = enable;
	}
	return err;
}

static int akm_ports_enable(struct akm_inf *inf, bool enable)
{
	int err;

	err = akm_port_enable(inf, RD, enable);
	err |= akm_port_enable(inf, WR, enable);
	return err;
}

static int akm_mode_wr(struct akm_inf *inf, bool reset, u8 range, u8 mode)
{
	u8 mode_old;
	u8 mode_new;
	u8 val;
	int i;
	int err = 0;

	mode_new = mode;
	if (range)
		mode |= AKM_CNTL1_OUTPUT_BIT16;
	if ((mode == inf->data_out) && (!reset))
		return err;

	mode_old = inf->data_out & AKM_CNTL1_MODE_MASK;
	if (inf->use_mpu)
		err = akm_ports_enable(inf, false);
	else
		cancel_delayed_work_sync(&inf->dw);
	if (err)
		return err;

	if (reset) {
		if (inf->compass_id == COMPASS_ID_AK8963) {
			err = akm_nvi_mpu_bypass_request(inf);
			if (!err) {
				err = akm_wr(inf, AKM_REG_CNTL2,
					     AKM_CNTL2_SRST);
				for (i = 0; i < AKM_HW_DELAY_POR_MS; i++) {
					mdelay(1);
					err = akm_i2c_rd(inf, AKM_REG_CNTL2,
							 1, &val);
					if (err)
						continue;

					if (!(val & AKM_CNTL2_SRST))
						break;
				}
				akm_nvi_mpu_bypass_release(inf);
			}
		}
	}
	inf->range_index = range;
	inf->data_out = mode;
	if (inf->use_mpu) {
		if ((mode_old > AKM_CNTL1_MODE_SINGLE) ||
					    (mode_new > AKM_CNTL1_MODE_SINGLE))
			err = akm_wr(inf, AKM_REG_CNTL1, mode);
		if (mode_new <= AKM_CNTL1_MODE_SINGLE) {
			err |= nvi_mpu_data_out(inf->port_id[WR], mode);
			if (mode_new)
				err |= akm_ports_enable(inf, true);
		} else {
			err |= akm_port_enable(inf, RD, true);
		}
	} else {
		err = akm_wr(inf, AKM_REG_CNTL1, mode);
		if (mode_new)
			queue_delayed_work(inf->wq, &inf->dw,
					 usecs_to_jiffies(inf->poll_delay_us));
	}
	return err;
}

static int akm_delay(struct akm_inf *inf, unsigned long delay_us)
{
	u8 mode;
	int err = 0;

	if (inf->use_mpu)
		err |= nvi_mpu_delay_us(inf->port_id[RD], delay_us);
	if (!err) {
		if (inf->compass_id == COMPASS_ID_AK8963) {
			if (delay_us == (AKM_INPUT_DELAY_MS_MIN * 1000))
				mode = AKM_CNTL1_MODE_CONT2;
			else
				mode = AKM_CNTL1_MODE_SINGLE;
			err = akm_mode_wr(inf, false, inf->range_index, mode);
		} else {
			if ((delay_us == (AKM_INPUT_DELAY_MS_MIN * 1000)) &&
							   inf->cycle_delay_us)
				inf->cycle = true;
			else
				inf->cycle = false;
		}
	}
	return err;
}

static int akm_init_hw(struct akm_inf *inf)
{
	u8 val[8];
	int err;
	int err_t;

	err_t = akm_nvi_mpu_bypass_request(inf);
	if (!err_t) {
		err_t = akm_wr(inf, AKM_REG_CNTL1, AKM_CNTL1_MODE_ROM_ACCESS);
		udelay(AKM_HW_DELAY_ROM_ACCESS_US);
		err_t |= akm_i2c_rd(inf, AKM_REG_ASAX, 3, inf->asa.asa);
		/* we can autodetect AK8963 with BITM */
		inf->compass_id = 0;
		err = akm_wr(inf, AKM_REG_CNTL1, (AKM_CNTL1_MODE_SINGLE |
						  AKM_CNTL1_OUTPUT_BIT16));
		if (!err) {
			mdelay(AKM_HW_DELAY_TSM_MS);
			err = akm_i2c_rd(inf, AKM_REG_ST2, 1, val);
			if ((!err) && (val[0] & AKM_CNTL1_OUTPUT_BIT16))
				inf->compass_id = COMPASS_ID_AK8963;
		}
		akm_nvi_mpu_bypass_release(inf);
		if (!inf->compass_id)
			inf->compass_id = inf->pdata.sec_slave_id;
		if (inf->compass_id == COMPASS_ID_AK8963) {
			inf->asa.range_lo[AXIS_X] = AKM8963_RANGE_X_LO;
			inf->asa.range_hi[AXIS_X] = AKM8963_RANGE_X_HI;
			inf->asa.range_lo[AXIS_Y] = AKM8963_RANGE_Y_LO;
			inf->asa.range_hi[AXIS_Y] = AKM8963_RANGE_Y_HI;
			inf->asa.range_lo[AXIS_Z] = AKM8963_RANGE_Z_LO;
			inf->asa.range_hi[AXIS_Z] = AKM8963_RANGE_Z_HI;
			inf->range_index = 1;
		} else if (inf->compass_id == COMPASS_ID_AK8972) {
			inf->asa.range_lo[AXIS_X] = AKM8972_RANGE_X_LO;
			inf->asa.range_hi[AXIS_X] = AKM8972_RANGE_X_HI;
			inf->asa.range_lo[AXIS_Y] = AKM8972_RANGE_Y_LO;
			inf->asa.range_hi[AXIS_Y] = AKM8972_RANGE_Y_HI;
			inf->asa.range_lo[AXIS_Z] = AKM8972_RANGE_Z_LO;
			inf->asa.range_hi[AXIS_Z] = AKM8972_RANGE_Z_HI;
			inf->range_index = 0;
		} else { /* default COMPASS_ID_AK8975 */
			inf->compass_id = COMPASS_ID_AK8975;
			inf->asa.range_lo[AXIS_X] = AKM8975_RANGE_X_LO;
			inf->asa.range_hi[AXIS_X] = AKM8975_RANGE_X_HI;
			inf->asa.range_lo[AXIS_Y] = AKM8975_RANGE_Y_LO;
			inf->asa.range_hi[AXIS_Y] = AKM8975_RANGE_Y_HI;
			inf->asa.range_lo[AXIS_Z] = AKM8975_RANGE_Z_LO;
			inf->asa.range_hi[AXIS_Z] = AKM8975_RANGE_Z_HI;
			inf->range_index = 0;
		}
	}
	if (!err_t)
		inf->initd = true;
	else
		dev_err(&inf->i2c->dev, "%s ERR %d", __func__, err_t);
	return err_t;
}

static void akm_calc(struct akm_inf *inf, u8 *data)
{
	short x;
	short y;
	short z;

	/* data[1] = AKM_REG_HXL
	 * data[2] = AKM_REG_HXH
	 * data[3] = AKM_REG_HYL
	 * data[4] = AKM_REG_HYH
	 * data[5] = AKM_REG_HZL
	 * data[6] = AKM_REG_HZH
	 */
	x = (short)((data[2] << 8) | data[1]);
	y = (short)((data[4] << 8) | data[3]);
	z = (short)((data[6] << 8) | data[5]);
	x = ((x * (inf->asa.asa[AXIS_X] + 128)) >> 8);
	y = ((y * (inf->asa.asa[AXIS_Y] + 128)) >> 8);
	z = ((z * (inf->asa.asa[AXIS_Z] + 128)) >> 8);
	mutex_lock(&inf->mutex_data);
	inf->xyz[AXIS_X] = x;
	inf->xyz[AXIS_Y] = y;
	inf->xyz[AXIS_Z] = z;
	mutex_unlock(&inf->mutex_data);
}

static void akm_report(struct akm_inf *inf, u8 *data, s64 ts)
{
	akm_calc(inf, data);
	input_report_rel(inf->idev, REL_X, inf->xyz[AXIS_X]);
	input_report_rel(inf->idev, REL_Y, inf->xyz[AXIS_Y]);
	input_report_rel(inf->idev, REL_Z, inf->xyz[AXIS_Z]);
	input_report_rel(inf->idev, REL_MISC, (unsigned int)(ts >> 32));
	input_report_rel(inf->idev, REL_WHEEL,
			 (unsigned int)(ts & 0xffffffff));
	input_sync(inf->idev);
}

static int akm_read_sts(struct akm_inf *inf, u8 *data)
{
	int err = -1; /* assume something wrong */
	/* data[0] = AKM_REG_ST1
	 * data[7] = AKM_REG_ST2
	 * data[8] = AKM_REG_CNTL1
	 */
	if ((data[0] & AKM_ST1_DRDY) && (!(data[7] &
					   (AKM_ST2_HOFL | AKM_ST2_DERR))))
		err = 1; /* data ready to be reported */
	else if ((data[8] & AKM_CNTL1_MODE_MASK) == (inf->data_out &
						     AKM_CNTL1_MODE_MASK))
		err = 0; /* still processing */
	return err;
}

static s64 akm_timestamp_ns(void)
{
	struct timespec ts;
	s64 ns;

	ktime_get_ts(&ts);
	ns = timespec_to_ns(&ts);
	return ns;
}

static int akm_read(struct akm_inf *inf)
{
	long long timestamp1;
	long long timestamp2;
	u8 data[9];
	int err;

	timestamp1 = akm_timestamp_ns();
	err = akm_i2c_rd(inf, AKM_REG_ST1, 9, data);
	timestamp2 = akm_timestamp_ns();
	if (err)
		return err;

	err = akm_read_sts(inf, data);
	if (err > 0) {
		timestamp2 = (timestamp2 - timestamp1) / 2;
		timestamp1 += timestamp2;
		akm_report(inf, data, timestamp1);
		if ((inf->data_out & AKM_CNTL1_MODE_MASK) ==
							 AKM_CNTL1_MODE_SINGLE)
			akm_i2c_wr(inf, AKM_REG_CNTL1, inf->data_out);
	} else if (err < 0) {
			dev_err(&inf->i2c->dev, "%s ERR\n", __func__);
			akm_mode_wr(inf, true, inf->range_index,
				    inf->data_out & AKM_CNTL1_MODE_MASK);
	}
	return err;
}

static void akm_mpu_handler(u8 *data, unsigned int len, s64 ts, void *p_val)
{
	struct akm_inf *inf;
	int err;

	inf = (struct akm_inf *)p_val;
	if (inf->enable) {
		if (inf->cycle && (!inf->port_en[WR])) {
			ts -= inf->cycle_ts;
			if (ts > (inf->cycle_delay_us * 1000))
				akm_port_enable(inf, WR, true);
			return;
		}
		err = akm_read_sts(inf, data);
		if (err > 0) {
			akm_report(inf, data, ts);
			if (inf->cycle) {
				akm_port_enable(inf, WR, false);
				inf->cycle_ts = ts;
			}
		}
	}
}

static int akm_id(struct akm_inf *inf)
{
	struct nvi_mpu_port nmp;
	u8 config_boot;
	u8 val = 0;
	int err;

	config_boot = inf->pdata.config & NVI_CONFIG_BOOT_MASK;
	if (config_boot == NVI_CONFIG_BOOT_AUTO) {
		nmp.addr = inf->i2c->addr | 0x80;
		nmp.reg = AKM_REG_WIA;
		nmp.ctrl = 1;
		err = nvi_mpu_dev_valid(&nmp, &val);
		/* see mpu.h for possible return values */
		if ((err == -EAGAIN) || (err == -EBUSY))
			return -EAGAIN;

		if (((!err) && (val == AKM_WIA_ID)) || (err == -EIO))
			config_boot = NVI_CONFIG_BOOT_MPU;
	}
	if (config_boot == NVI_CONFIG_BOOT_MPU) {
		inf->use_mpu = true;
		nmp.addr = inf->i2c->addr | 0x80;
		nmp.reg = AKM_REG_ST1;
		nmp.ctrl = 9;
		nmp.data_out = 0;
		nmp.delay_ms = 0;
		nmp.delay_us = inf->poll_delay_us;
		nmp.shutdown_bypass = false;
		nmp.handler = &akm_mpu_handler;
		nmp.ext_driver = (void *)inf;
		err = nvi_mpu_port_alloc(&nmp);
		if (err < 0)
			return err;

		inf->port_id[RD] = err;
		nmp.addr = inf->i2c->addr;
		nmp.reg = AKM_REG_CNTL1;
		nmp.ctrl = 1;
		nmp.data_out = inf->data_out;
		nmp.delay_ms = AKM_HW_DELAY_TSM_MS;
		nmp.delay_us = 0;
		nmp.shutdown_bypass = false;
		nmp.handler = NULL;
		nmp.ext_driver = NULL;
		err = nvi_mpu_port_alloc(&nmp);
		if (err < 0) {
			akm_ports_free(inf);
		} else {
			inf->port_id[WR] = err;
			err = 0;
		}
		return err;
	}

	/* NVI_CONFIG_BOOT_EXTERNAL */
	inf->use_mpu = false;
	err = akm_i2c_rd(inf, AKM_REG_WIA, 1, &val);
	if ((!err) && (val == AKM_WIA_ID))
		return 0;

	return -ENODEV;
}

static void akm_work(struct work_struct *ws)
{
	struct akm_inf *inf;

	inf = container_of(ws, struct akm_inf, dw.work);
	akm_read(inf);
	queue_delayed_work(inf->wq, &inf->dw,
			   usecs_to_jiffies(inf->poll_delay_us));
}

static int akm_enable(struct akm_inf *inf, bool enable)
{
	int err = 0;

	akm_pm(inf, true);
	if (!inf->initd)
		err = akm_init_hw(inf);
	if (!err) {
		if (enable) {
			inf->data_out = AKM_CNTL1_MODE_SINGLE;
			err = akm_delay(inf, inf->poll_delay_us);
			err |= akm_mode_wr(inf, true, inf->range_index,
					  inf->data_out & AKM_CNTL1_MODE_MASK);
			if (!err)
				inf->enable = true;
		} else {
			inf->enable = false;
			err = akm_mode_wr(inf, false, inf->range_index,
					  AKM_CNTL1_MODE_POWERDOWN);
			if (!err)
				akm_pm(inf, false);
		}
	}
	return err;
}

static ssize_t akm_enable_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned int enable;
	bool en;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtouint(buf, 10, &enable);
	if (err)
		return -EINVAL;

	if (enable)
		en = true;
	else
		en = false;
	dev_dbg(&inf->i2c->dev, "%s: %x", __func__, en);
	err = akm_enable(inf, en);
	if (err) {
		dev_err(&inf->i2c->dev, "%s: %x ERR=%d", __func__, en, err);
		return err;
	}

	return count;
}

static ssize_t akm_enable_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;
	unsigned int enable = 0;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		enable = 1;
	return sprintf(buf, "%u\n", enable);
}

static ssize_t akm_delay_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned long delay_us;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtoul(buf, 10, &delay_us);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s: %lu\n", __func__, delay_us);
	if (delay_us < (AKM_INPUT_DELAY_MS_MIN * 1000))
		delay_us = (AKM_INPUT_DELAY_MS_MIN * 1000);
	if ((inf->enable) && (delay_us != inf->poll_delay_us))
		err = akm_delay(inf, delay_us);
	if (!err) {
		inf->poll_delay_us = delay_us;
	} else {
		dev_err(&inf->i2c->dev, "%s: %lu ERR=%d\n",
			__func__, delay_us, err);
		return err;
	}

	return count;
}

static ssize_t akm_delay_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		return sprintf(buf, "%lu\n", inf->poll_delay_us);

	return sprintf(buf, "%u\n", (AKM_INPUT_DELAY_MS_MIN * 1000));
}

static ssize_t akm_resolution_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (kstrtouint(buf, 10, &resolution))
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u", __func__, resolution);
	inf->resolution = resolution;
	return count;
}

static ssize_t akm_resolution_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;
	unsigned int resolution;

	inf = dev_get_drvdata(dev);
	if (inf->enable)
		resolution = inf->resolution;
	else
		resolution = AKM_INPUT_RESOLUTION;
	return sprintf(buf, "%u\n", resolution);
}

static ssize_t akm_max_range_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct akm_inf *inf;
	u8 range_index;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtou8(buf, 10, &range_index);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %u", __func__, range_index);
	if (inf->compass_id == COMPASS_ID_AK8963) {
		if (range_index > 1)
			return -EINVAL;

		if (inf->enable) {
			err = akm_mode_wr(inf, false, range_index,
					  inf->data_out & AKM_CNTL1_MODE_MASK);
			if (err)
				return err;
		} else {
			inf->range_index = range_index;
		}
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t akm_max_range_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;
	unsigned int range;

	inf = dev_get_drvdata(dev);
	if (inf->enable) {
		range = inf->range_index;
	} else {
		if (inf->compass_id == COMPASS_ID_AK8963) {
			if (inf->range_index)
				range = AKM8963_SCALE16;
			else
				range = AKM8963_SCALE14;
		} else if (inf->compass_id == COMPASS_ID_AK8972) {
			range = AKM8972_SCALE;
		} else {
			range = AKM8975_SCALE;
		}
	}
	return sprintf(buf, "%u\n", range);
}

static ssize_t akm_cycle_delay_us_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct akm_inf *inf;
	unsigned long cycle_delay_us;
	int err;

	inf = dev_get_drvdata(dev);
	err = kstrtoul(buf, 10, &cycle_delay_us);
	if (err)
		return -EINVAL;

	dev_dbg(&inf->i2c->dev, "%s %lu", __func__, cycle_delay_us);
	inf->cycle_delay_us = cycle_delay_us;
	return count;
}

static ssize_t akm_cycle_delay_us_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct akm_inf *inf = dev_get_drvdata(dev);

	return sprintf(buf, "%lu\n", inf->cycle_delay_us);
}

static ssize_t akm_divisor_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;

	inf = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", AKM_INPUT_DIVISOR);
}

static ssize_t akm_microamp_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct akm_inf *inf;

	inf = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", AKM_INPUT_POWER_UA);
}

static ssize_t akm_magnetic_field_show(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	struct akm_inf *inf;
	short x;
	short y;
	short z;

	inf = dev_get_drvdata(dev);
	if (inf->enable) {
		mutex_lock(&inf->mutex_data);
		x = inf->xyz[AXIS_X];
		y = inf->xyz[AXIS_Y];
		z = inf->xyz[AXIS_Z];
		mutex_unlock(&inf->mutex_data);
		return sprintf(buf, "%hd, %hd, %hd\n", x, y, z);
	}

	return -EPERM;
}

static ssize_t akm_orientation_show(struct device *dev,
				    struct device_attribute *attr,
				    char *buf)
{
	struct akm_inf *inf;
	signed char *m;

	inf = dev_get_drvdata(dev);
	m = inf->pdata.orientation;
	return sprintf(buf, "%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		       m[0], m[1], m[2], m[3], m[4], m[5], m[6], m[7], m[8]);
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWOTH,
		   akm_enable_show, akm_enable_store);
static DEVICE_ATTR(delay, S_IRUGO | S_IWUSR | S_IWOTH,
		   akm_delay_show, akm_delay_store);
static DEVICE_ATTR(resolution, S_IRUGO | S_IWUSR | S_IWOTH,
		   akm_resolution_show, akm_resolution_store);
static DEVICE_ATTR(max_range, S_IRUGO | S_IWUSR | S_IWOTH,
		   akm_max_range_show, akm_max_range_store);
static DEVICE_ATTR(cycle_delay_us, S_IRUGO | S_IWUSR | S_IWOTH,
		   akm_cycle_delay_us_show, akm_cycle_delay_us_store);
static DEVICE_ATTR(divisor, S_IRUGO,
		   akm_divisor_show, NULL);
static DEVICE_ATTR(microamp, S_IRUGO,
		   akm_microamp_show, NULL);
static DEVICE_ATTR(magnetic_field, S_IRUGO,
		   akm_magnetic_field_show, NULL);
static DEVICE_ATTR(orientation, S_IRUGO,
		   akm_orientation_show, NULL);

static struct attribute *akm_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_delay.attr,
	&dev_attr_resolution.attr,
	&dev_attr_max_range.attr,
	&dev_attr_divisor.attr,
	&dev_attr_microamp.attr,
	&dev_attr_magnetic_field.attr,
	&dev_attr_orientation.attr,
	&dev_attr_cycle_delay_us.attr,
	NULL
};

static struct attribute_group akm_attr_group = {
	.name = AKM_NAME,
	.attrs = akm_attrs
};

static int akm_sysfs_create(struct akm_inf *inf)
{
	int err;

	err = sysfs_create_group(&inf->idev->dev.kobj, &akm_attr_group);
	return err;
}

static void akm_input_close(struct input_dev *idev)
{
	struct akm_inf *inf;

	inf = input_get_drvdata(idev);
	if (inf != NULL)
		akm_enable(inf, false);
}

static int akm_input_create(struct akm_inf *inf)
{
	int err;

	inf->idev = input_allocate_device();
	if (!inf->idev) {
		err = -ENOMEM;
		dev_err(&inf->i2c->dev, "%s ERR %d\n", __func__, err);
		return err;
	}

	inf->idev->name = AKM_NAME;
	inf->idev->dev.parent = &inf->i2c->dev;
	inf->idev->close = akm_input_close;
	input_set_drvdata(inf->idev, inf);
	input_set_capability(inf->idev, EV_REL, REL_X);
	input_set_capability(inf->idev, EV_REL, REL_Y);
	input_set_capability(inf->idev, EV_REL, REL_Z);
	input_set_capability(inf->idev, EV_REL, REL_MISC);
	input_set_capability(inf->idev, EV_REL, REL_WHEEL);
	err = input_register_device(inf->idev);
	if (err) {
		input_free_device(inf->idev);
		inf->idev = NULL;
	}
	return err;
}

static int akm_remove(struct i2c_client *client)
{
	struct akm_inf *inf;

	inf = i2c_get_clientdata(client);
	if (inf != NULL) {
		if (inf->idev) {
			input_unregister_device(inf->idev);
			input_free_device(inf->idev);
		}
		if (inf->wq)
			destroy_workqueue(inf->wq);
		akm_pm_exit(inf);
		if (&inf->mutex_data)
			mutex_destroy(&inf->mutex_data);
		kfree(inf);
	}
	dev_info(&client->dev, "%s\n", __func__);
	return 0;
}

static void akm_shutdown(struct i2c_client *client)
{
	akm_remove(client);
}

static int akm_probe(struct i2c_client *client,
		     const struct i2c_device_id *devid)
{
	struct akm_inf *inf;
	struct mpu_platform_data *pd;
	int err;

	dev_info(&client->dev, "%s\n", __func__);
	inf = kzalloc(sizeof(*inf), GFP_KERNEL);
	if (IS_ERR_OR_NULL(inf)) {
		dev_err(&client->dev, "%s kzalloc ERR\n", __func__);
		return -ENOMEM;
	}

	inf->i2c = client;
	i2c_set_clientdata(client, inf);
	pd = (struct mpu_platform_data *)dev_get_platdata(&client->dev);
	if (pd == NULL)
		dev_err(&client->dev, "%s No %s platform data ERR\n",
			__func__, devid->name);
	else
		inf->pdata = *pd;
	akm_pm_init(inf);
	err = akm_id(inf);
	akm_pm(inf, false);
	if (err == -EAGAIN)
		goto akm_probe_again;
	else if (err)
		goto akm_probe_err;

	mutex_init(&inf->mutex_data);
	err = akm_input_create(inf);
	if (err)
		goto akm_probe_err;

	inf->wq = create_singlethread_workqueue(AKM_NAME);
	if (!inf->wq) {
		dev_err(&client->dev, "%s workqueue ERR\n", __func__);
		err = -ENOMEM;
		goto akm_probe_err;
	}

	INIT_DELAYED_WORK(&inf->dw, akm_work);
	err = akm_sysfs_create(inf);
	if (err)
		goto akm_probe_err;

	return 0;

akm_probe_err:
	dev_err(&client->dev, "%s ERR %d\n", __func__, err);
akm_probe_again:
	akm_remove(client);
	return err;
}

static const struct i2c_device_id akm_i2c_device_id[] = {
	{AKM_NAME, 0},
	{"ak8963", 0},
	{"ak8972", 0},
	{"ak8975", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, akm_i2c_device_id);

static struct i2c_driver akm_driver = {
	.class		= I2C_CLASS_HWMON,
	.probe		= akm_probe,
	.remove		= akm_remove,
	.driver = {
		.name	= AKM_NAME,
		.owner	= THIS_MODULE,
	},
	.id_table	= akm_i2c_device_id,
	.shutdown	= akm_shutdown,
};

static int __init akm_init(void)
{
	return i2c_add_driver(&akm_driver);
}

static void __exit akm_exit(void)
{
	i2c_del_driver(&akm_driver);
}

module_init(akm_init);
module_exit(akm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AKM driver");

