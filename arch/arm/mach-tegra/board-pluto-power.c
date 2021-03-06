/*
 * arch/arm/mach-tegra/board-pluto-power.c
 *
 * Copyright (C) 2012-2013 NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/i2c.h>
#include <linux/pda_power.h>
#include <linux/platform_device.h>
#include <linux/resource.h>
#include <linux/io.h>

#include <mach/iomap.h>
#include <mach/edp.h>
#include <mach/irqs.h>
#include <mach/hardware.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/palmas.h>
#include <linux/regulator/machine.h>
#include <linux/irq.h>
#include <linux/platform_data/tegra_edp.h>

#include <asm/mach-types.h>

#include "cpu-tegra.h"
#include "pm.h"
#include "board.h"
#include "board-common.h"
#include "board-pluto.h"
#include "board-pmu-defines.h"
#include "tegra_cl_dvfs.h"
#include "devices.h"
#include "tegra11_soctherm.h"
#include "tegra3_tsensor.h"

#define PMC_CTRL		0x0
#define PMC_CTRL_INTR_LOW	(1 << 17)

/************************ Pluto based regulator ****************/
static struct regulator_consumer_supply palmas_smps123_supply[] = {
	REGULATOR_SUPPLY("vdd_cpu", NULL),
};

static struct regulator_consumer_supply palmas_smps45_supply[] = {
	REGULATOR_SUPPLY("vdd_core", NULL),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("vdd_core", "sdhci-tegra.3"),
};

static struct regulator_consumer_supply palmas_smps6_supply[] = {
	REGULATOR_SUPPLY("vdd_core_bb", NULL),
};

static struct regulator_consumer_supply palmas_smps7_supply[] = {
	REGULATOR_SUPPLY("vddio_ddr", NULL),
	REGULATOR_SUPPLY("vddio_lpddr3", NULL),
	REGULATOR_SUPPLY("vcore2_lpddr3", NULL),
	REGULATOR_SUPPLY("vcore_audio_1v2", NULL),
};

static struct regulator_consumer_supply palmas_smps8_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_osc", NULL),
	REGULATOR_SUPPLY("vddio_sys", NULL),
	REGULATOR_SUPPLY("vddio_bb", NULL),
	REGULATOR_SUPPLY("pwrdet_bb", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.0"),
	REGULATOR_SUPPLY("pwrdet_sdmmc1", NULL),
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("vdd_emmc", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("pwrdet_sdmmc4", NULL),
	REGULATOR_SUPPLY("vddio_audio", NULL),
	REGULATOR_SUPPLY("pwrdet_audio", NULL),
	REGULATOR_SUPPLY("vddio_uart", NULL),
	REGULATOR_SUPPLY("pwrdet_uart", NULL),
	REGULATOR_SUPPLY("vddio_gmi", NULL),
	REGULATOR_SUPPLY("pwrdet_nand", NULL),
	REGULATOR_SUPPLY("vddio_cam", "vi"),
	REGULATOR_SUPPLY("pwrdet_cam", NULL),
	REGULATOR_SUPPLY("vdd_gps", NULL),
	REGULATOR_SUPPLY("vdd_nfc", NULL),
	REGULATOR_SUPPLY("vlogic", "0-0069"),
	REGULATOR_SUPPLY("vid", "0-000d"),
	REGULATOR_SUPPLY("vddio", "0-0078"),
	REGULATOR_SUPPLY("vdd_dtv", NULL),
	REGULATOR_SUPPLY("vdd_bb", NULL),
	REGULATOR_SUPPLY("vcore1_lpddr", NULL),
	REGULATOR_SUPPLY("vcore_lpddr", NULL),
	REGULATOR_SUPPLY("vddio_lpddr", NULL),
	REGULATOR_SUPPLY("vdd_rf", NULL),
	REGULATOR_SUPPLY("vdd_modem2", NULL),
	REGULATOR_SUPPLY("vdd_dbg", NULL),
	REGULATOR_SUPPLY("vdd_sim_1v8", NULL),
	REGULATOR_SUPPLY("vdd_sim1a_1v8", NULL),
	REGULATOR_SUPPLY("vdd_sim1b_1v8", NULL),
	REGULATOR_SUPPLY("dvdd_audio", NULL),
	REGULATOR_SUPPLY("avdd_audio", NULL),
	REGULATOR_SUPPLY("vdd_com_1v8", NULL),
	REGULATOR_SUPPLY("vdd_bt_1v8", NULL),
	REGULATOR_SUPPLY("dvdd", "spi3.2"),
	REGULATOR_SUPPLY("avdd_pll_bb", NULL),
};

static struct regulator_consumer_supply palmas_smps9_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.3"),
	REGULATOR_SUPPLY("vdd_sim_mmc", NULL),
	REGULATOR_SUPPLY("vdd_sim1a_mmc", NULL),
	REGULATOR_SUPPLY("vdd_sim1b_mmc", NULL),
};

static struct regulator_consumer_supply palmas_smps10_supply[] = {
	REGULATOR_SUPPLY("unused_smps10", NULL),
	REGULATOR_SUPPLY("usb_vbus", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("usb_vbus", "tegra-xhci"),

	REGULATOR_SUPPLY("vdd_lcd", NULL),
};

static struct regulator_consumer_supply palmas_ldo1_supply[] = {
	REGULATOR_SUPPLY("avdd_hdmi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_csi_dsi_pll", "vi"),
	REGULATOR_SUPPLY("avdd_pllm", NULL),
	REGULATOR_SUPPLY("avdd_pllu", NULL),
	REGULATOR_SUPPLY("avdd_plla_p_c", NULL),
	REGULATOR_SUPPLY("avdd_pllx", NULL),
	REGULATOR_SUPPLY("vdd_ddr_hs", NULL),
	REGULATOR_SUPPLY("avdd_plle", NULL),
};

static struct regulator_consumer_supply palmas_ldo2_supply[] = {
	REGULATOR_SUPPLY("avdd_lcd", NULL),
};

static struct regulator_consumer_supply palmas_ldo3_supply[] = {
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.0"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "tegradc.1"),
	REGULATOR_SUPPLY("avdd_dsi_csi", "vi"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.1"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-ehci.2"),
	REGULATOR_SUPPLY("vddio_hsic", "tegra-xhci"),
	REGULATOR_SUPPLY("pwrdet_mipi", NULL),
	REGULATOR_SUPPLY("vddio_hsic_bb", NULL),
	REGULATOR_SUPPLY("vddio_hsic_modem2", NULL),
};

static struct regulator_consumer_supply palmas_ldo4_supply[] = {
	REGULATOR_SUPPLY("vdd_spare", NULL),
};

static struct regulator_consumer_supply palmas_ldo5_supply[] = {
	REGULATOR_SUPPLY("avdd_cam1", NULL),
	REGULATOR_SUPPLY("vana", "2-0010"),
};

static struct regulator_consumer_supply palmas_ldo6_supply[] = {
	REGULATOR_SUPPLY("vdd_temp", NULL),
	REGULATOR_SUPPLY("vdd_mb", NULL),
	REGULATOR_SUPPLY("vin", "1-004d"),
	REGULATOR_SUPPLY("vdd_nfc_3v0", NULL),
	REGULATOR_SUPPLY("vdd_irled", NULL),
	REGULATOR_SUPPLY("vdd_sensor_3v0", NULL),
	REGULATOR_SUPPLY("vdd_3v0_pm", NULL),
	REGULATOR_SUPPLY("vaux_3v3", NULL),
	REGULATOR_SUPPLY("vdd", "0-0044"),
	REGULATOR_SUPPLY("vdd", "0-004c"),
	REGULATOR_SUPPLY("avdd", "spi3.2"),
	REGULATOR_SUPPLY("vdd", "0-0069"),
	REGULATOR_SUPPLY("vdd", "0-000d"),
	REGULATOR_SUPPLY("vdd", "0-0078"),
};

static struct regulator_consumer_supply palmas_ldo7_supply[] = {
	REGULATOR_SUPPLY("vdd_af_cam1", NULL),
	REGULATOR_SUPPLY("vdd", "2-000e"),
};
static struct regulator_consumer_supply palmas_ldo8_supply[] = {
	REGULATOR_SUPPLY("vdd_rtc", NULL),
};
static struct regulator_consumer_supply palmas_ldo9_supply[] = {
	REGULATOR_SUPPLY("vddio_sdmmc", "sdhci-tegra.2"),
	REGULATOR_SUPPLY("pwrdet_sdmmc3", NULL),
};
static struct regulator_consumer_supply palmas_ldoln_supply[] = {
	REGULATOR_SUPPLY("avdd_cam2", NULL),
	REGULATOR_SUPPLY("vana", "2-0036"),
};

static struct regulator_consumer_supply palmas_ldousb_supply[] = {
	REGULATOR_SUPPLY("avdd_usb", "tegra-udc.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.0"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.1"),
	REGULATOR_SUPPLY("avdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("hvdd_usb", "tegra-xhci"),
	REGULATOR_SUPPLY("avdd_hdmi", "tegradc.1"),
	REGULATOR_SUPPLY("vddio_hv", "tegradc.1"),
	REGULATOR_SUPPLY("pwrdet_hv", NULL),
	REGULATOR_SUPPLY("vdd_dtv_3v3", NULL),

};

static struct regulator_consumer_supply palmas_regen1_supply[] = {
	REGULATOR_SUPPLY("mic_ventral", NULL),
};

static struct regulator_consumer_supply palmas_regen2_supply[] = {
	REGULATOR_SUPPLY("vdd_mic", NULL),
};

PALMAS_PDATA_INIT(smps123, 900,  1300, NULL, 0, 0, 0, NORMAL);
PALMAS_PDATA_INIT(smps45, 900,  1400, NULL, 0, 0, 0, NORMAL);
PALMAS_PDATA_INIT(smps6, 850,  850, NULL, 0, 0, 1, NORMAL);
PALMAS_PDATA_INIT(smps7, 1200,  1200, NULL, 0, 0, 1, NORMAL);
PALMAS_PDATA_INIT(smps8, 1800,  1800, NULL, 1, 1, 1, NORMAL);
PALMAS_PDATA_INIT(smps9, 2800,  2800, NULL, 1, 0, 1, NORMAL);
PALMAS_PDATA_INIT(smps10, 5000,  5000, NULL, 0, 0, 0, 0);
PALMAS_PDATA_INIT(ldo1, 1050,  1050, palmas_rails(smps7), 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldo2, 2800,  3000, NULL, 0, 0, 0, 0);
PALMAS_PDATA_INIT(ldo3, 1200,  1200, palmas_rails(smps8), 0, 1, 1, 0);
PALMAS_PDATA_INIT(ldo4, 900,  3300, NULL, 0, 0, 0, 0);
PALMAS_PDATA_INIT(ldo5, 2700,  2700, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldo6, 3000,  3000, NULL, 1, 1, 1, 0);
PALMAS_PDATA_INIT(ldo7, 2800,  2800, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldo8, 900,  900, NULL, 1, 1, 1, 0);
PALMAS_PDATA_INIT(ldo9, 1800,  3300, palmas_rails(smps9), 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldoln, 2700, 2700, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(ldousb, 3300,  3300, NULL, 0, 0, 1, 0);
PALMAS_PDATA_INIT(regen1, 4300,  4300, NULL, 0, 0, 0, 0);
PALMAS_PDATA_INIT(regen2, 4300,  4300, palmas_rails(smps8), 0, 0, 0, 0);

#define PALMAS_REG_PDATA(_sname) &reg_idata_##_sname

static struct regulator_init_data *pluto_reg_data[PALMAS_NUM_REGS] = {
	NULL,
	PALMAS_REG_PDATA(smps123),
	NULL,
	PALMAS_REG_PDATA(smps45),
	NULL,
	PALMAS_REG_PDATA(smps6),
	PALMAS_REG_PDATA(smps7),
	PALMAS_REG_PDATA(smps8),
	PALMAS_REG_PDATA(smps9),
	PALMAS_REG_PDATA(smps10),
	PALMAS_REG_PDATA(ldo1),
	PALMAS_REG_PDATA(ldo2),
	PALMAS_REG_PDATA(ldo3),
	PALMAS_REG_PDATA(ldo4),
	PALMAS_REG_PDATA(ldo5),
	PALMAS_REG_PDATA(ldo6),
	PALMAS_REG_PDATA(ldo7),
	PALMAS_REG_PDATA(ldo8),
	PALMAS_REG_PDATA(ldo9),
	PALMAS_REG_PDATA(ldoln),
	PALMAS_REG_PDATA(ldousb),
	PALMAS_REG_PDATA(regen1),
	PALMAS_REG_PDATA(regen2),
	NULL,
	NULL,
	NULL,
};

#define PALMAS_REG_INIT(_name, _warm_reset, _roof_floor, _mode_sleep,	\
		_tstep, _vsel)						\
	static struct palmas_reg_init reg_init_data_##_name = {		\
		.warm_reset = _warm_reset,				\
		.roof_floor =	_roof_floor,				\
		.mode_sleep = _mode_sleep,		\
		.tstep = _tstep,			\
		.vsel = _vsel,		\
	}

PALMAS_REG_INIT(smps12, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(smps123, 0, PALMAS_EXT_CONTROL_ENABLE1, 0, 0, 0);
PALMAS_REG_INIT(smps3, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(smps45, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REG_INIT(smps457, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(smps6, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(smps7, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(smps8, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(smps9, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(smps10, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo1, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REG_INIT(ldo2, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REG_INIT(ldo3, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo4, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REG_INIT(ldo5, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REG_INIT(ldo6, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo7, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REG_INIT(ldo8, 0, 0, 0, 0, 0);
PALMAS_REG_INIT(ldo9, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REG_INIT(ldoln, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);
PALMAS_REG_INIT(ldousb, 0, PALMAS_EXT_CONTROL_NSLEEP, 0, 0, 0);

#define PALMAS_REG_INIT_DATA(_sname) &reg_init_data_##_sname
static struct palmas_reg_init *pluto_reg_init[PALMAS_NUM_REGS] = {
	PALMAS_REG_INIT_DATA(smps12),
	PALMAS_REG_INIT_DATA(smps123),
	PALMAS_REG_INIT_DATA(smps3),
	PALMAS_REG_INIT_DATA(smps45),
	PALMAS_REG_INIT_DATA(smps457),
	PALMAS_REG_INIT_DATA(smps6),
	PALMAS_REG_INIT_DATA(smps7),
	PALMAS_REG_INIT_DATA(smps8),
	PALMAS_REG_INIT_DATA(smps9),
	PALMAS_REG_INIT_DATA(smps10),
	PALMAS_REG_INIT_DATA(ldo1),
	PALMAS_REG_INIT_DATA(ldo2),
	PALMAS_REG_INIT_DATA(ldo3),
	PALMAS_REG_INIT_DATA(ldo4),
	PALMAS_REG_INIT_DATA(ldo5),
	PALMAS_REG_INIT_DATA(ldo6),
	PALMAS_REG_INIT_DATA(ldo7),
	PALMAS_REG_INIT_DATA(ldo8),
	PALMAS_REG_INIT_DATA(ldo9),
	PALMAS_REG_INIT_DATA(ldoln),
	PALMAS_REG_INIT_DATA(ldousb),
};

static int ac_online(void)
{
	return 1;
}

static struct resource pluto_pda_resources[] = {
	[0] = {
		.name	= "ac",
	},
};

static struct pda_power_pdata pluto_pda_data = {
	.is_ac_online	= ac_online,
};

static struct platform_device pluto_pda_power_device = {
	.name		= "pda-power",
	.id		= -1,
	.resource	= pluto_pda_resources,
	.num_resources	= ARRAY_SIZE(pluto_pda_resources),
	.dev	= {
		.platform_data	= &pluto_pda_data,
	},
};

/* Always ON /Battery regulator */
static struct regulator_consumer_supply fixed_reg_en_battery_supply[] = {
		REGULATOR_SUPPLY("vdd_sys_cam", NULL),
		REGULATOR_SUPPLY("vdd_sys_bl", NULL),
		REGULATOR_SUPPLY("vdd_sys_com", NULL),
		REGULATOR_SUPPLY("vdd_sys_gps", NULL),
		REGULATOR_SUPPLY("vdd_sys_bt", NULL),
		REGULATOR_SUPPLY("vdd_sys_audio", NULL),
		REGULATOR_SUPPLY("vdd_vbrtr", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_1v8_cam_supply[] = {
	REGULATOR_SUPPLY("vddio_cam_mb", NULL),
	REGULATOR_SUPPLY("vdd_1v8_cam12", NULL),
	REGULATOR_SUPPLY("vif", "2-0010"),
	REGULATOR_SUPPLY("vif", "2-0036"),
	REGULATOR_SUPPLY("vdd_i2c", "2-000e"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_1v2_cam_supply[] = {
	REGULATOR_SUPPLY("vdd_1v2_cam", NULL),
	REGULATOR_SUPPLY("vdig", "2-0010"),
	REGULATOR_SUPPLY("vdig", "2-0036"),
};

static struct regulator_consumer_supply fixed_reg_en_avdd_usb3_1v05_supply[] = {
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avddio_usb", "tegra-ehci.2"),
	REGULATOR_SUPPLY("avdd_usb_pll", "tegra-xhci"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_mmc_sdmmc3_supply[] = {
	REGULATOR_SUPPLY("vddio_sd_slot", "sdhci-tegra.2"),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_lcd_1v8_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_1v8_s", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_lcd_mmc_supply[] = {
	REGULATOR_SUPPLY("vdd_lcd_mmc_s_f", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_1v8_mic_supply[] = {
	REGULATOR_SUPPLY("vdd_1v8_mic", NULL),
};

static struct regulator_consumer_supply fixed_reg_en_vdd_hdmi_5v0_supply[] = {
	REGULATOR_SUPPLY("vdd_hdmi_5v0", "tegradc.1"),
};

static struct regulator_consumer_supply fixed_reg_en_vpp_fuse_supply[] = {
	REGULATOR_SUPPLY("vpp_fuse", NULL),
	REGULATOR_SUPPLY("v_efuse", NULL),
};

/* Macro for defining fixed regulator sub device data */
#define FIXED_SUPPLY(_name) "fixed_reg_en"#_name
#define FIXED_REG(_id, _var, _name, _in_supply, _always_on, _boot_on,	\
	_gpio_nr, _open_drain, _active_high, _boot_state, _millivolts,	\
	_sdelay)							\
	static struct regulator_init_data ri_data_##_var =		\
	{								\
		.supply_regulator = _in_supply,				\
		.num_consumer_supplies =				\
			ARRAY_SIZE(fixed_reg_en_##_name##_supply),	\
		.consumer_supplies = fixed_reg_en_##_name##_supply,	\
		.constraints = {					\
			.valid_modes_mask = (REGULATOR_MODE_NORMAL |	\
					REGULATOR_MODE_STANDBY),	\
			.valid_ops_mask = (REGULATOR_CHANGE_MODE |	\
					REGULATOR_CHANGE_STATUS |	\
					REGULATOR_CHANGE_VOLTAGE),	\
			.always_on = _always_on,			\
			.boot_on = _boot_on,				\
		},							\
	};								\
	static struct fixed_voltage_config fixed_reg_en_##_var##_pdata = \
	{								\
		.supply_name = FIXED_SUPPLY(_name),			\
		.microvolts = _millivolts * 1000,			\
		.gpio = _gpio_nr,					\
		.gpio_is_open_drain = _open_drain,			\
		.enable_high = _active_high,				\
		.enabled_at_boot = _boot_state,				\
		.init_data = &ri_data_##_var,				\
		.startup_delay = _sdelay				\
	};								\
	static struct platform_device fixed_reg_en_##_var##_dev = {	\
		.name = "reg-fixed-voltage",				\
		.id = _id,						\
		.dev = {						\
			.platform_data = &fixed_reg_en_##_var##_pdata,	\
		},							\
	}

FIXED_REG(0,	battery,	battery,
	NULL,	0,	0,
	-1,	false, true,	0,	3300,	0);

FIXED_REG(1,	vdd_1v8_cam,	vdd_1v8_cam,
	palmas_rails(smps8),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO1,	false, true,	0,	1800,
	0);

FIXED_REG(2,	vdd_1v2_cam,	vdd_1v2_cam,
	palmas_rails(smps7),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO2,	false, true,	0,	1200,
	0);

FIXED_REG(3,	avdd_usb3_1v05,	avdd_usb3_1v05,
	palmas_rails(smps8),	0,	0,
	TEGRA_GPIO_PK5,	false,	true,	0,	1050,	0);

FIXED_REG(4,	vdd_mmc_sdmmc3,	vdd_mmc_sdmmc3,
	palmas_rails(smps9),	0,	0,
	TEGRA_GPIO_PK1,	false,	true,	0,	3300,	0);

FIXED_REG(5,	vdd_lcd_1v8,	vdd_lcd_1v8,
	palmas_rails(smps8),	0,	0,
	PALMAS_TEGRA_GPIO_BASE + PALMAS_GPIO4,	false,	true,	0,	1800,
	0);

FIXED_REG(6,	vdd_lcd_mmc,	vdd_lcd_mmc,
	palmas_rails(smps9),	0,	0,
	TEGRA_GPIO_PI4,	false,	true,	0,	1800,	0);

FIXED_REG(7,	vdd_1v8_mic,	vdd_1v8_mic,
	palmas_rails(smps8),	0,	0,
	-1,	false,	true,	0,	1800,	0);

FIXED_REG(8,	vdd_hdmi_5v0,	vdd_hdmi_5v0,
	NULL,	0,	0,
	TEGRA_GPIO_PK6,	true,	true,	0,	5000,	5000);

FIXED_REG(9,	vpp_fuse,	vpp_fuse,
	palmas_rails(smps8),	0,	0,
	TEGRA_GPIO_PX4,	false,	true,	0,	1800,	0);

/*
 * Creating the fixed regulator device tables
 */
#define ADD_FIXED_REG(_name)    (&fixed_reg_en_##_name##_dev)

#define E1580_COMMON_FIXED_REG			\
	ADD_FIXED_REG(battery),			\
	ADD_FIXED_REG(vdd_1v8_cam),		\
	ADD_FIXED_REG(vdd_1v2_cam),		\
	ADD_FIXED_REG(avdd_usb3_1v05),		\
	ADD_FIXED_REG(vdd_mmc_sdmmc3),		\
	ADD_FIXED_REG(vdd_lcd_1v8),		\
	ADD_FIXED_REG(vdd_lcd_mmc),		\
	ADD_FIXED_REG(vdd_1v8_mic),		\
	ADD_FIXED_REG(vdd_hdmi_5v0),

#define E1580_T114_FIXED_REG			\
	ADD_FIXED_REG(vpp_fuse),

/* Gpio switch regulator platform data for Pluto E1580 */
static struct platform_device *pfixed_reg_devs[] = {
	E1580_COMMON_FIXED_REG
	E1580_T114_FIXED_REG
};

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
/* board parameters for cpu dfll */
static struct tegra_cl_dvfs_cfg_param pluto_cl_dvfs_param = {
	.sample_rate = 12500,

	.force_mode = TEGRA_CL_DVFS_FORCE_FIXED,
	.cf = 10,
	.ci = 0,
	.cg = 2,

	.droop_cut_value = 0xF,
	.droop_restore_ramp = 0x0,
	.scale_out_ramp = 0x0,
};
#endif

/* palmas: fixed 10mV steps from 600mV to 1400mV, with offset 0x10 */
#define PMU_CPU_VDD_MAP_SIZE ((1400000 - 600000) / 10000 + 1)
static struct voltage_reg_map pmu_cpu_vdd_map[PMU_CPU_VDD_MAP_SIZE];
static inline void fill_reg_map(void)
{
	int i;
	for (i = 0; i < PMU_CPU_VDD_MAP_SIZE; i++) {
		pmu_cpu_vdd_map[i].reg_value = i + 0x10;
		pmu_cpu_vdd_map[i].reg_uV = 600000 + 10000 * i;
	}
}

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
static struct tegra_cl_dvfs_platform_data pluto_cl_dvfs_data = {
	.dfll_clk_name = "dfll_cpu",
	.pmu_if = TEGRA_CL_DVFS_PMU_I2C,
	.u.pmu_i2c = {
		.fs_rate = 400000,
		.slave_addr = 0xb0,
		.reg = 0x23,
	},
	.vdd_map = pmu_cpu_vdd_map,
	.vdd_map_size = PMU_CPU_VDD_MAP_SIZE,

	.cfg_param = &pluto_cl_dvfs_param,
};

static int __init pluto_cl_dvfs_init(void)
{
	fill_reg_map();
	if (tegra_revision < TEGRA_REVISION_A02)
		pluto_cl_dvfs_data.out_quiet_then_disable = true;
	tegra_cl_dvfs_device.dev.platform_data = &pluto_cl_dvfs_data;
	platform_device_register(&tegra_cl_dvfs_device);

	return 0;
}
#endif

static struct palmas_dvfs_init_data palmas_dvfs_idata[] = {
	{
		.en_pwm = false,
	}, {
		.en_pwm = true,
		.ext_ctrl = PALMAS_EXT_CONTROL_ENABLE2,
		.reg_id = PALMAS_REG_SMPS6,
		.step_20mV = true,
		.base_voltage_uV = 500000,
		.max_voltage_uV = 1100000,
	},
};

static struct palmas_pmic_platform_data pmic_platform = {
	.enable_ldo8_tracking = true,
	.disabe_ldo8_tracking_suspend = true,
	.disable_smps10_boost_suspend = true,
	.dvfs_init_data = palmas_dvfs_idata,
	.dvfs_init_data_size = ARRAY_SIZE(palmas_dvfs_idata),
};

struct palmas_clk32k_init_data palmas_clk32k_idata[] = {
	{
		.clk32k_id = PALMAS_CLOCK32KG,
		.enable = true,
	}, {
		.clk32k_id = PALMAS_CLOCK32KG_AUDIO,
		.enable = true,
	},
};

static struct palmas_pinctrl_config palmas_pincfg[] = {
	PALMAS_PINMUX(POWERGOOD, POWERGOOD, DEFAULT, DEFAULT),
	PALMAS_PINMUX(VAC, VAC, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO0, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO1, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO2, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO3, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO4, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO5, CLK32KGAUDIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO6, GPIO, DEFAULT, DEFAULT),
	PALMAS_PINMUX(GPIO7, GPIO, DEFAULT, DEFAULT),
};

static struct palmas_pinctrl_platform_data palmas_pinctrl_pdata = {
	.pincfg = palmas_pincfg,
	.num_pinctrl = ARRAY_SIZE(palmas_pincfg),
	.dvfs1_enable = false,
	.dvfs2_enable = true,
};

static struct palmas_platform_data palmas_pdata = {
	.gpio_base = PALMAS_TEGRA_GPIO_BASE,
	.irq_base = PALMAS_TEGRA_IRQ_BASE,
	.pmic_pdata = &pmic_platform,
	.clk32k_init_data =  palmas_clk32k_idata,
	.clk32k_init_data_size = ARRAY_SIZE(palmas_clk32k_idata),
	.irq_type = IRQ_TYPE_LEVEL_HIGH,
	.use_power_off = true,
	.pinctrl_pdata = &palmas_pinctrl_pdata,
};

static struct i2c_board_info palma_device[] = {
	{
		I2C_BOARD_INFO("tps65913", 0x58),
		.irq		= INT_EXTERNAL_PMU,
		.platform_data	= &palmas_pdata,
	},
};

int __init pluto_regulator_init(void)
{
	void __iomem *pmc = IO_ADDRESS(TEGRA_PMC_BASE);
	u32 pmc_ctrl;
	int i;

#ifdef CONFIG_ARCH_TEGRA_HAS_CL_DVFS
	pluto_cl_dvfs_init();
#endif

	/* TPS65913: Normal state of INT request line is LOW.
	 * configure the power management controller to trigger PMU
	 * interrupts when HIGH.
	 */
	pmc_ctrl = readl(pmc + PMC_CTRL);
	writel(pmc_ctrl & ~PMC_CTRL_INTR_LOW, pmc + PMC_CTRL);

	for (i = 0; i < PALMAS_NUM_REGS ; i++) {
		pmic_platform.reg_data[i] = pluto_reg_data[i];
		pmic_platform.reg_init[i] = pluto_reg_init[i];
	}

	platform_device_register(&pluto_pda_power_device);
	i2c_register_board_info(4, palma_device,
			ARRAY_SIZE(palma_device));
	return 0;
}

static int __init pluto_fixed_regulator_init(void)
{
	if (!machine_is_tegra_pluto())
		return 0;

	return platform_add_devices(pfixed_reg_devs,
			ARRAY_SIZE(pfixed_reg_devs));
}
subsys_initcall_sync(pluto_fixed_regulator_init);

static struct tegra_suspend_platform_data pluto_suspend_data = {
	.cpu_timer	= 300,
	.cpu_off_timer	= 300,
	.suspend_mode	= TEGRA_SUSPEND_LP0,
	.core_timer	= 0x157e,
	.core_off_timer = 2000,
	.corereq_high	= true,
	.sysclkreq_high	= true,
	.cpu_lp2_min_residency = 1000,
	.min_residency_crail = 20000,
#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	.lp1_lowvolt_support = true,
	.i2c_base_addr = TEGRA_I2C5_BASE,
	.pmuslave_addr = 0xB0,
	.core_reg_addr = 0x2B,
	.lp1_core_volt_low_cold = 0x33,
	.lp1_core_volt_low = 0x2e,
	.lp1_core_volt_high = 0x42,
#endif
};

int __init pluto_suspend_init(void)
{
	tegra_init_suspend(&pluto_suspend_data);
	return 0;
}

int __init pluto_edp_init(void)
{
	unsigned int regulator_mA;

	regulator_mA = get_maximum_cpu_current_supported();
	if (!regulator_mA)
		regulator_mA = 9000;

	pr_info("%s: CPU regulator %d mA\n", __func__, regulator_mA);
	tegra_init_cpu_edp_limits(regulator_mA);

	regulator_mA = get_maximum_core_current_supported();
	if (!regulator_mA)
		regulator_mA = 4000;

	pr_info("%s: core regulator %d mA\n", __func__, regulator_mA);
	tegra_init_core_edp_limits(regulator_mA);

	return 0;
}

static struct thermal_zone_params pluto_soctherm_therm_cpu_tzp = {
	.governor_name = "pid_thermal_gov",
};

static struct tegra_tsensor_pmu_data tpdata_palmas = {
	.reset_tegra = 1,
	.pmu_16bit_ops = 0,
	.controller_type = 0,
	.pmu_i2c_addr = 0x58,
	.i2c_controller_id = 4,
	.poweroff_reg_addr = 0xa0,
	.poweroff_reg_data = 0x0,
};

static struct soctherm_platform_data pluto_soctherm_data = {
	.oc_irq_base = TEGRA_SOC_OC_IRQ_BASE,
	.num_oc_irqs = TEGRA_SOC_OC_NUM_IRQ,
	.therm = {
		[THERM_CPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 90000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 100000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &pluto_soctherm_therm_cpu_tzp,
		},
		[THERM_GPU] = {
			.zone_enable = true,
			.passive_delay = 1000,
			.hotspot_offset = 6000,
			.num_trips = 3,
			.trips = {
				{
					.cdev_type = "tegra-balanced",
					.trip_temp = 90000,
					.trip_type = THERMAL_TRIP_PASSIVE,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-heavy",
					.trip_temp = 100000,
					.trip_type = THERMAL_TRIP_HOT,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
				{
					.cdev_type = "tegra-shutdown",
					.trip_temp = 102000,
					.trip_type = THERMAL_TRIP_CRITICAL,
					.upper = THERMAL_NO_LIMIT,
					.lower = THERMAL_NO_LIMIT,
				},
			},
			.tzp = &pluto_soctherm_therm_cpu_tzp,
		},
		[THERM_PLL] = {
			.zone_enable = true,
		},
	},
	.throttle = {
		[THROTTLE_HEAVY] = {
			.priority = 100,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 80,
				},
			},
		},
		[THROTTLE_OC4] = {
			.throt_mode = BRIEF,
			.polarity = 1,
			.intr = true,
			.devs = {
				[THROTTLE_DEV_CPU] = {
					.enable = true,
					.depth = 50,
				},
				[THROTTLE_DEV_GPU] = {
					.enable = true,
					.depth = 50,
				},
			},
		},
	},
	.tshut_pmu_trip_data = &tpdata_palmas,
};

int __init pluto_soctherm_init(void)
{
	tegra_platform_edp_init(pluto_soctherm_data.therm[THERM_CPU].trips,
			&pluto_soctherm_data.therm[THERM_CPU].num_trips,
			8000);  /* edp temperature margin */
	tegra_add_tj_trips(pluto_soctherm_data.therm[THERM_CPU].trips,
			&pluto_soctherm_data.therm[THERM_CPU].num_trips);
	tegra_add_vc_trips(pluto_soctherm_data.therm[THERM_CPU].trips,
			&pluto_soctherm_data.therm[THERM_CPU].num_trips);

	return tegra11_soctherm_init(&pluto_soctherm_data);
}

static struct tegra_sysedp_corecap pluto_sysedp_corecap[] = {
	{  1000,    0, {   748, 240, 204 }, {   748, 240, 204 } },
	{  2000,    0, {   748, 240, 204 }, {   748, 240, 204 } },
	{  3000,    0, {   748, 240, 204 }, {   748, 240, 204 } },
	{  4000,    0, {   748, 240, 204 }, {   748, 240, 204 } },
	{  5000,    0, {   748, 240, 204 }, {   748, 240, 204 } },
	{  6000,    0, {  1748, 240, 204 }, {  1748, 240, 204 } },
	{  7000,    0, {  2748, 240, 204 }, {  2748, 240, 204 } },
	{  7000, 1000, {  3748, 240, 204 }, {  3748, 240, 204 } },
	{  7000, 2000, {  4748, 240, 204 }, {  4696, 324, 204 } },
	{  7000, 3000, {  4843, 240, 624 }, {  4913, 420, 408 } },
	{  7000, 4000, {  5843, 240, 624 }, {  5306, 420, 624 } },
	{  8000,    0, {  3748, 240, 204 }, {  3748, 240, 204 } },
	{  8000, 1000, {  4748, 240, 204 }, {  4696, 324, 204 } },
	{  8000, 2000, {  4843, 240, 624 }, {  4975, 324, 408 } },
	{  8000, 3000, {  5843, 240, 624 }, {  5913, 420, 408 } },
	{  8000, 4000, {  6843, 240, 624 }, {  5895, 528, 624 } },
	{  9000,    0, {  4748, 240, 204 }, {  3975, 324, 408 } },
	{  9000, 1000, {  4843, 240, 624 }, {  4975, 324, 408 } },
	{  9000, 2000, {  5843, 240, 624 }, {  5913, 420, 408 } },
	{  9000, 3000, {  6565, 240, 792 }, {  5895, 528, 624 } },
	{  9000, 4000, {  7565, 240, 792 }, {  6405, 600, 624 } },
	{ 10000,    0, {  4565, 240, 792 }, {  3895, 528, 624 } },
	{ 10000, 1000, {  5565, 240, 792 }, {  4895, 528, 624 } },
	{ 10000, 2000, {  6565, 240, 792 }, {  5405, 600, 624 } },
	{ 10000, 3000, {  7565, 240, 792 }, {  5277, 600, 792 } },
	{ 10000, 4000, {  8565, 240, 792 }, {  6277, 600, 792 } },
	{ 11000,    0, {  5565, 240, 792 }, {  5553, 324, 624 } },
	{ 11000, 1000, {  6565, 240, 792 }, {  5405, 600, 624 } },
	{ 11000, 2000, {  7565, 240, 792 }, {  5277, 600, 792 } },
	{ 11000, 3000, {  8565, 240, 792 }, {  6277, 600, 792 } },
	{ 11000, 4000, {  9565, 240, 792 }, {  7277, 600, 792 } },
	{ 12000,    0, {  6565, 240, 792 }, {  5405, 600, 624 } },
	{ 12000, 1000, {  7565, 240, 792 }, {  5277, 600, 792 } },
	{ 12000, 2000, {  8565, 240, 792 }, {  6277, 600, 792 } },
	{ 12000, 3000, {  9565, 240, 792 }, {  7277, 600, 792 } },
	{ 12000, 4000, { 10565, 240, 792 }, {  8277, 600, 792 } },
	{ 13000,    0, {  7565, 240, 792 }, {  6405, 600, 624 } },
	{ 13000, 1000, {  8565, 240, 792 }, {  6277, 600, 792 } },
	{ 13000, 2000, {  9565, 240, 792 }, {  7277, 600, 792 } },
	{ 13000, 3000, { 10565, 240, 792 }, {  8277, 600, 792 } },
	{ 13000, 4000, { 11565, 240, 792 }, {  9277, 600, 792 } },
	{ 14000,    0, {  8565, 240, 792 }, {  6277, 600, 792 } },
	{ 14000, 1000, {  9565, 240, 792 }, {  7277, 600, 792 } },
	{ 14000, 2000, { 10565, 240, 792 }, {  8277, 600, 792 } },
	{ 14000, 3000, { 11565, 240, 792 }, {  9277, 600, 792 } },
	{ 14000, 4000, { 12565, 240, 792 }, { 10277, 600, 792 } },
	{ 15000,    0, {  9565, 240, 792 }, {  7277, 600, 792 } },
	{ 15000, 1000, { 10565, 240, 792 }, {  8277, 600, 792 } },
	{ 15000, 2000, { 11565, 240, 792 }, {  9277, 600, 792 } },
	{ 15000, 3000, { 12565, 240, 792 }, { 10277, 600, 792 } },
	{ 15000, 4000, { 13565, 240, 792 }, { 11277, 600, 792 } },
	{ 16000,    0, { 10565, 240, 792 }, {  8277, 600, 792 } },
	{ 16000, 1000, { 11565, 240, 792 }, {  9277, 600, 792 } },
	{ 16000, 2000, { 12565, 240, 792 }, { 10277, 600, 792 } },
	{ 16000, 3000, { 13565, 240, 792 }, { 11277, 600, 792 } },
	{ 16000, 4000, { 14565, 240, 792 }, { 12277, 600, 792 } },
	{ 17000,    0, { 11565, 240, 792 }, {  9277, 600, 792 } },
	{ 17000, 1000, { 12565, 240, 792 }, { 10277, 600, 792 } },
	{ 17000, 2000, { 13565, 240, 792 }, { 11277, 600, 792 } },
	{ 17000, 3000, { 14565, 240, 792 }, { 12277, 600, 792 } },
	{ 17000, 4000, { 15565, 240, 792 }, { 13277, 600, 792 } },
	{ 18000,    0, { 12565, 240, 792 }, { 10277, 600, 792 } },
	{ 18000, 1000, { 13565, 240, 792 }, { 11277, 600, 792 } },
	{ 18000, 2000, { 14565, 240, 792 }, { 12277, 600, 792 } },
	{ 18000, 3000, { 15565, 240, 792 }, { 13277, 600, 792 } },
	{ 18000, 4000, { 16565, 240, 792 }, { 14277, 600, 792 } },
	{ 19000,    0, { 13565, 240, 792 }, { 11277, 600, 792 } },
	{ 19000, 1000, { 14565, 240, 792 }, { 12277, 600, 792 } },
	{ 19000, 2000, { 15565, 240, 792 }, { 13277, 600, 792 } },
	{ 19000, 3000, { 16565, 240, 792 }, { 14277, 600, 792 } },
	{ 19000, 4000, { 17565, 240, 792 }, { 15277, 600, 792 } },
	{ 23000,    0, { 15277, 600, 792 }, { 15277, 600, 792 } }
};

static struct tegra_sysedp_platform_data pluto_sysedp_platdata = {
	.corecap = pluto_sysedp_corecap,
	.corecap_size = ARRAY_SIZE(pluto_sysedp_corecap)
};

static struct platform_device pluto_sysedp_device = {
	.name = "tegra_sysedp",
	.id = -1,
	.dev = { .platform_data = &pluto_sysedp_platdata }
};

static __init int pluto_sysedp_init(void)
{
	pluto_sysedp_platdata.cpufreq_lim = tegra_get_system_edp_entries(
			&pluto_sysedp_platdata.cpufreq_lim_size);
	if (!pluto_sysedp_platdata.cpufreq_lim) {
		WARN_ON(1);
		return -ENODEV;
	}

	return platform_device_register(&pluto_sysedp_device);
}
late_initcall(pluto_sysedp_init);
