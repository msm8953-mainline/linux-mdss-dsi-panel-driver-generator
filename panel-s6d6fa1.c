// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2013, The Linux Foundation. All rights reserved.

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <video/mipi_display.h>

struct s6d6fa1 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct backlight_device *backlight;
	struct gpio_desc *reset_gpio;

	bool prepared;
	bool enabled;
};

static inline struct s6d6fa1 *to_s6d6fa1(struct drm_panel *panel)
{
	return container_of(panel, struct s6d6fa1, panel);
}

#define dsi_generic_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_generic_write(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

#define dsi_dcs_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_dcs_write_buffer(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static void s6d6fa1_reset(struct s6d6fa1 *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(10);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	msleep(10);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(10);
}

static int s6d6fa1_on(struct s6d6fa1 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dsi_generic_write_seq(dsi, 0xf0, 0x5a, 0x5a);
	dsi_generic_write_seq(dsi, 0xf1, 0x5a, 0x5a);
	dsi_generic_write_seq(dsi, 0xfc, 0x5a, 0x5a);
	dsi_generic_write_seq(dsi, 0xf5,
			      0x10, 0x18, 0x00, 0xd1, 0xa7, 0x11, 0x08);
	dsi_generic_write_seq(dsi, 0xb3, 0x10, 0xf0, 0x00, 0xbb, 0x04, 0x08);
	dsi_generic_write_seq(dsi, 0xb6, 0x29, 0x10, 0x2c, 0x64, 0x64, 0x01);
	dsi_generic_write_seq(dsi, 0xb7,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x09,
			      0x03, 0xf5, 0x00, 0x00, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xb8,
			      0x3b, 0x00, 0x18, 0x18, 0x03, 0x21, 0x11, 0x00,
			      0x83, 0xf4, 0xbb, 0x00, 0x03, 0x1e, 0x21, 0x00,
			      0xbe, 0xbb, 0x0b, 0xe0, 0x70, 0xff, 0xff, 0x00,
			      0x20, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xb9,
			      0x1d, 0x00, 0x18, 0x18, 0x03, 0x00, 0x11, 0x00,
			      0x02, 0xf4, 0xbb, 0x00, 0x03, 0x00, 0x00, 0x11,
			      0x86, 0x83, 0x18, 0x61, 0x20, 0xff, 0xff, 0x00,
			      0x40, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xba,
			      0x3b, 0x00, 0x18, 0x18, 0x03, 0x21, 0x11, 0x00,
			      0x83, 0xf4, 0xbb, 0x00, 0x03, 0x1e, 0x21, 0x00,
			      0xbe, 0xbb, 0x0b, 0xe0, 0x70, 0xff, 0xff, 0x00,
			      0x20, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xbb,
			      0x3b, 0x00, 0x18, 0x18, 0x03, 0x21, 0x11, 0x00,
			      0x83, 0xf4, 0xbb, 0x00, 0x03, 0x1e, 0x21, 0x00,
			      0xbe, 0xbb, 0x0b, 0xe0, 0x70, 0xff, 0xff, 0x00,
			      0x20, 0x00, 0x00);
	dsi_generic_write_seq(dsi, 0xbc,
			      0x20, 0x80, 0x3c, 0x19, 0x01, 0x02, 0x06, 0x00,
			      0x00, 0x2d, 0x06, 0x0f, 0x13, 0x0b, 0x1e, 0x04,
			      0x02, 0x2a, 0x00, 0x00, 0x02, 0x00, 0x99);
	dsi_generic_write_seq(dsi, 0xbd,
			      0x01, 0x07, 0x07, 0x07, 0x00, 0x00, 0x00, 0x00,
			      0x05, 0x01, 0x00, 0x00, 0x00, 0x40, 0x00, 0x05,
			      0x2d, 0x05, 0x05, 0x05);
	dsi_generic_write_seq(dsi, 0xbe,
			      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0b,
			      0x0a, 0x0b, 0x07, 0x0b, 0x04, 0x06, 0x02, 0x01,
			      0x00, 0x09, 0x0b, 0x0e, 0x0d, 0x0c, 0x11, 0x10,
			      0x0f, 0x0b, 0x0b, 0x0b, 0x0b, 0x12, 0x03, 0x05,
			      0x02, 0x01, 0x00, 0x08, 0x0b, 0x0e, 0x0d, 0x0c,
			      0x11, 0x10, 0x0f);
	dsi_generic_write_seq(dsi, 0xbf,
			      0x00, 0x00, 0x00, 0x00, 0x0c, 0xcc, 0xcc, 0xcc,
			      0xcc, 0x0c, 0xc6, 0xcc, 0xc7, 0xcc);
	dsi_generic_write_seq(dsi, 0xd0, 0x08);
	dsi_generic_write_seq(dsi, 0xe3, 0x22);
	dsi_generic_write_seq(dsi, 0xe8,
			      0x00, 0x03, 0x10, 0x04, 0x02, 0x06, 0xd0);
	dsi_generic_write_seq(dsi, 0xf2, 0x46, 0x43, 0x13, 0x33, 0xc1, 0x16);
	dsi_generic_write_seq(dsi, 0xf4,
			      0x50, 0x50, 0x70, 0x14, 0x14, 0x14, 0x14, 0x06,
			      0x16, 0x26, 0x00, 0x6e, 0x1f, 0x14, 0x0e, 0xc9,
			      0x02, 0x55, 0x35, 0x54, 0xaa, 0x55, 0x05, 0x04,
			      0x44, 0x48, 0x30);
	dsi_generic_write_seq(dsi, 0xf7, 0x00, 0x00, 0x3f, 0xff);
	dsi_generic_write_seq(dsi, 0xfa,
			      0x05, 0x7c, 0x19, 0x21, 0x2d, 0x39, 0x3f, 0x45,
			      0x48, 0x48, 0x56, 0x5b, 0x3c, 0x3d, 0x3d, 0x46,
			      0x43, 0x3e, 0x41, 0x3d, 0x3a, 0x3a, 0x23, 0x23,
			      0x26, 0x2b, 0x1a, 0x05, 0x7c, 0x19, 0x22, 0x2e,
			      0x3a, 0x40, 0x45, 0x48, 0x48, 0x56, 0x5b, 0x3c,
			      0x3d, 0x3d, 0x46, 0x44, 0x3f, 0x42, 0x3e, 0x3b,
			      0x3b, 0x25, 0x24, 0x27, 0x2c, 0x1a, 0x05, 0x7c,
			      0x22, 0x2f, 0x39, 0x43, 0x48, 0x4c, 0x4d, 0x4c,
			      0x5a, 0x5e, 0x3e, 0x3f, 0x3e, 0x46, 0x44, 0x3e,
			      0x41, 0x3d, 0x3a, 0x3a, 0x23, 0x22, 0x25, 0x2b,
			      0x1a, 0x00);
	dsi_generic_write_seq(dsi, 0xfb,
			      0x05, 0x7c, 0x19, 0x21, 0x2d, 0x39, 0x3f, 0x45,
			      0x48, 0x48, 0x56, 0x5b, 0x3c, 0x3d, 0x3d, 0x46,
			      0x43, 0x3e, 0x41, 0x3d, 0x3a, 0x3a, 0x23, 0x23,
			      0x26, 0x2b, 0x1a, 0x05, 0x7c, 0x19, 0x22, 0x2e,
			      0x3a, 0x40, 0x45, 0x48, 0x48, 0x56, 0x5b, 0x3c,
			      0x3d, 0x3d, 0x46, 0x44, 0x3f, 0x42, 0x3e, 0x3b,
			      0x3b, 0x25, 0x24, 0x27, 0x2c, 0x1a, 0x05, 0x7c,
			      0x22, 0x2f, 0x39, 0x43, 0x48, 0x4c, 0x4d, 0x4c,
			      0x5a, 0x5e, 0x3e, 0x3f, 0x3e, 0x46, 0x44, 0x3e,
			      0x41, 0x3d, 0x3a, 0x3a, 0x23, 0x22, 0x25, 0x2b,
			      0x1a);
	dsi_generic_write_seq(dsi, 0xf0, 0xa5, 0xa5);
	dsi_generic_write_seq(dsi, 0xf1, 0xa5, 0xa5);
	dsi_generic_write_seq(dsi, 0xfc, 0xa5, 0xa5);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x00ff);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}
	msleep(1);

	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x2c);
	msleep(1);
	dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x00);
	msleep(1);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}
	msleep(1);

	ret = mipi_dsi_dcs_exit_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to exit sleep mode: %d\n", ret);
		return ret;
	}
	msleep(120);

	ret = mipi_dsi_dcs_set_display_on(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display on: %d\n", ret);
		return ret;
	}
	msleep(40);

	return 0;
}

static int s6d6fa1_off(struct s6d6fa1 *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	msleep(20);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(80);

	return 0;
}

static int s6d6fa1_prepare(struct drm_panel *panel)
{
	struct s6d6fa1 *ctx = to_s6d6fa1(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	s6d6fa1_reset(ctx);

	ret = s6d6fa1_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 0);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int s6d6fa1_unprepare(struct drm_panel *panel)
{
	struct s6d6fa1 *ctx = to_s6d6fa1(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = s6d6fa1_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 0);

	ctx->prepared = false;
	return 0;
}

static int s6d6fa1_enable(struct drm_panel *panel)
{
	struct s6d6fa1 *ctx = to_s6d6fa1(panel);
	int ret;

	if (ctx->enabled)
		return 0;

	ret = backlight_enable(ctx->backlight);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Failed to enable backlight: %d\n", ret);
		return ret;
	}

	ctx->enabled = true;
	return 0;
}

static int s6d6fa1_disable(struct drm_panel *panel)
{
	struct s6d6fa1 *ctx = to_s6d6fa1(panel);
	int ret;

	if (!ctx->enabled)
		return 0;

	ret = backlight_disable(ctx->backlight);
	if (ret < 0) {
		dev_err(&ctx->dsi->dev, "Failed to disable backlight: %d\n", ret);
		return ret;
	}

	ctx->enabled = false;
	return 0;
}

static const struct drm_display_mode s6d6fa1_mode = {
	.clock = (1080 + 216 + 16 + 52) * (1920 + 4 + 1 + 3) * 57 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 216,
	.hsync_end = 1080 + 216 + 16,
	.htotal = 1080 + 216 + 16 + 52,
	.vdisplay = 1920,
	.vsync_start = 1920 + 4,
	.vsync_end = 1920 + 4 + 1,
	.vtotal = 1920 + 4 + 1 + 3,
	.vrefresh = 57,
	.width_mm = 62,
	.height_mm = 110,
};

static int s6d6fa1_get_modes(struct drm_panel *panel)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(panel->drm, &s6d6fa1_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	panel->connector->display_info.width_mm = mode->width_mm;
	panel->connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(panel->connector, mode);

	return 1;
}

static const struct drm_panel_funcs s6d6fa1_panel_funcs = {
	.disable = s6d6fa1_disable,
	.unprepare = s6d6fa1_unprepare,
	.prepare = s6d6fa1_prepare,
	.enable = s6d6fa1_enable,
	.get_modes = s6d6fa1_get_modes,
};

static int dsi_dcs_bl_get_brightness(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	int ret;
	u16 brightness = bl->props.brightness;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_get_display_brightness(dsi, &brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return brightness & 0xff;
}

static int dsi_dcs_bl_update_status(struct backlight_device *bl)
{
	struct mipi_dsi_device *dsi = bl_get_data(bl);
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_brightness(dsi, bl->props.brightness);
	if (ret < 0)
		return ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	return 0;
}

static const struct backlight_ops dsi_bl_ops = {
	.update_status = dsi_dcs_bl_update_status,
	.get_brightness = dsi_dcs_bl_get_brightness,
};

static struct backlight_device *
s6d6fa1_create_backlight(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct backlight_properties props;

	memset(&props, 0, sizeof(props));
	props.type = BACKLIGHT_RAW;
	props.brightness = 255;
	props.max_brightness = 255;

	return devm_backlight_device_register(dev, dev_name(dev), dev, dsi,
					      &dsi_bl_ops, &props);
}

static int s6d6fa1_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct s6d6fa1 *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_LOW);
	if (IS_ERR(ctx->reset_gpio)) {
		ret = PTR_ERR(ctx->reset_gpio);
		dev_err(dev, "Failed to get reset-gpios: %d\n", ret);
		return ret;
	}

	ctx->backlight = s6d6fa1_create_backlight(dsi);
	if (IS_ERR(ctx->backlight)) {
		ret = PTR_ERR(ctx->backlight);
		dev_err(dev, "Failed to create backlight: %d\n", ret);
		return ret;
	}

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel);
	ctx->panel.dev = dev;
	ctx->panel.funcs = &s6d6fa1_panel_funcs;

	ret = drm_panel_add(&ctx->panel);
	if (ret < 0) {
		dev_err(dev, "Failed to add panel: %d\n", ret);
		return ret;
	}

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		return ret;
	}

	return 0;
}

static int s6d6fa1_remove(struct mipi_dsi_device *dsi)
{
	struct s6d6fa1 *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);

	return 0;
}

static const struct of_device_id s6d6fa1_of_match[] = {
	{ .compatible = "mdss,s6d6fa1" }, // FIXME
	{ }
};
MODULE_DEVICE_TABLE(of, s6d6fa1_of_match);

static struct mipi_dsi_driver s6d6fa1_driver = {
	.probe = s6d6fa1_probe,
	.remove = s6d6fa1_remove,
	.driver = {
		.name = "panel-s6d6fa1",
		.of_match_table = s6d6fa1_of_match,
	},
};
module_mipi_dsi_driver(s6d6fa1_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>");
MODULE_DESCRIPTION("DRM driver for S6D6FA1 1080p video mode dsi panel");
MODULE_LICENSE("GPL v2");
