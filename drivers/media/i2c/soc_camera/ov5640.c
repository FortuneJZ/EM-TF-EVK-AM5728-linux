/*
 * Omnivision ov5640 CMOS Image Sensor driver
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/media.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_graph.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/videodev2.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/v4l2-common.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-event.h>
#include <media/v4l2-image-sizes.h>
#include <media/v4l2-mediabus.h>
#include <media/v4l2-of.h>
#include <media/v4l2-subdev.h>
#include <media/soc_camera.h>
#include <media/v4l2-clk.h>

#include "ov5640-common.h"

static const struct v4l2_ctrl_ops ov5640_ctrl_ops = {
	.s_ctrl = ov5640_s_ctrl,
};

static const struct v4l2_subdev_video_ops ov5640_subdev_video_ops = {
	.s_stream = ov5640_start_stop_stream,
	.s_parm 	= ov5640_set_framerate,
	.g_parm  	= ov5640_get_framerate,
};

static const struct v4l2_subdev_pad_ops ov5640_subdev_pad_ops = {
	.enum_mbus_code = ov5640_enum_mbus_code,
	.enum_frame_size = ov5640_enum_frame_sizes,
	.get_fmt = ov5640_get_fmt,
	.set_fmt = ov5640_set_fmt,
};

static const struct v4l2_subdev_ops ov5640_subdev_ops = {
	.video = &ov5640_subdev_video_ops,
	.pad   = &ov5640_subdev_pad_ops,
};

/*!
 * ov5640_probe - device probe function. 
 *			  
 * @client: pointer to standard i2c_client structure
 * @id: pointer to standard i2c_device_id structure
 *
 * Return 0 if successful, otherwise negative.
 */
static int ov5640_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	const struct ov5640_platform_data *pdata;
	struct v4l2_subdev *sd;
	struct ov5640 *ov5640;
	struct clk *clk;
	int ret;
	struct device *dev = &client->dev;
	
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
	/* check the platform data */
	pdata = ov5640_get_pdata(client);
	if (!pdata) {
		dev_err(&client->dev, "platform data not specified\n");
		return -EINVAL;
	}

	/* request power down pin */
	pwn_gpio = of_get_named_gpio(dev->of_node, "pwn-gpios", 0);
	if (!gpio_is_valid(pwn_gpio)) {
		dev_err(dev, "no sensor pwdn pin available\n");
		return -ENODEV;
	}

	/* enable the ov5640 */
	ret = devm_gpio_request_one(dev, pwn_gpio, GPIOF_OUT_INIT_LOW,
					"ov5640_pwdn");
	if (ret < 0)
		return ret;

	/* request reset pin */
	rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(rst_gpio)) {
		dev_err(dev, "no sensor reset pin available\n");
		return -EINVAL;
	}
	
	/* unreset the ov5640 */
	ret = devm_gpio_request_one(dev, rst_gpio, GPIOF_OUT_INIT_HIGH,
					"ov5640_reset");
	if (ret < 0)
		return ret;

	/* request and init a ov5640 device */
	ov5640 = devm_kzalloc(&client->dev, sizeof(*ov5640), GFP_KERNEL);
	if (!ov5640)
		return -ENOMEM;

	/* init the ov5640 parameters*/
	ov5640->pdata = pdata;
	ov5640->client = client;

	/* get the mclk frequency,set it before clk on */
	clk = devm_clk_get(&client->dev, "xvclk");
	if (IS_ERR(clk)) {
		dev_err(dev, "get xvclk failed\n");
		return PTR_ERR(clk);
    }

	ov5640->xvclk_frequency = clk_get_rate(clk);
	if (ov5640->xvclk_frequency < 6000000 ||
	    ov5640->xvclk_frequency > 27000000)
		return -EINVAL;

	/* register the i2c device to v4l2 */
	sd = &ov5640->sd;
	client->flags |= I2C_CLIENT_SCCB;
	v4l2_i2c_subdev_init(sd, client, &ov5640_subdev_ops);
	
	v4l2_ctrl_handler_init(&ov5640->ctrl_handler, 10);
	v4l2_ctrl_new_std(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
			V4L2_CID_BRIGHTNESS, 0, 255, 1, 0);
	v4l2_ctrl_new_std(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
			V4L2_CID_CONTRAST, -3, 3, 1, 0);
	v4l2_ctrl_new_std(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
			V4L2_CID_SCENE_MODE_ZGB, 0, 255, 1, 0);
	v4l2_ctrl_new_std(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
			V4L2_CID_EXPOSURE, -4, 4, 1, 0);
	v4l2_ctrl_new_std(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
			V4L2_CID_MIRROR_ZGB, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
			V4L2_CID_FLIP_ZGB, 0, 1, 1, 0);
	v4l2_ctrl_new_std(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
			V4L2_SPECIAL_EFFECTS_ZGB, 0, 255, 1, 0);
	v4l2_ctrl_new_std(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
			V4L2_CID_WHITE_BALANCE_ZGB, 0, 4, 1, 0);
	v4l2_ctrl_new_std_menu_items(&ov5640->ctrl_handler, &ov5640_ctrl_ops,
				     V4L2_CID_TEST_PATTERN,
				     ARRAY_SIZE(ov5640_test_pattern_menu) - 1,
				     0, 0, ov5640_test_pattern_menu);
	
	ov5640->sd.ctrl_handler = &ov5640->ctrl_handler;

	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE |
		     V4L2_SUBDEV_FL_HAS_EVENTS;

	mutex_init(&ov5640->lock);

	ov5640_get_default_format(&ov5640->format);
	ov5640->frame_size = &ov5640_framesizes[2];
	ov5640->format_ctrl_regs = ov5640_formats[0].format_ctrl_regs;

	/* get the clock */
	ov5640->clk = v4l2_clk_get(&client->dev, "xvclk");
	if (IS_ERR(ov5640->clk))
		return PTR_ERR(ov5640->clk);
	
	/* init the device */
	ret = ov5640_init(sd);
	if (ret < 0)
		goto error;

	ret = v4l2_async_register_subdev(&ov5640->sd);
	if (ret)
		goto error;

	embest_debug("embest_debug: %s(%d) %s sensor driver registered !!\n",__FUNCTION__,__LINE__,sd->name);

	return 0;

error:
	mutex_destroy(&ov5640->lock);
	v4l2_clk_put(ov5640->clk);

	return ret;
}

/*!
 * ov5640_remove - device remove function. 
 *			  
 * @client: pointer to standard i2c_client structure
 *
 * Return 0 if successful.
 */
static int ov5640_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct ov5640 *ov5640 = to_ov5640(sd);
	struct soc_camera_subdev_desc *ssdd = soc_camera_i2c_to_desc(client);

	if (ssdd->free_bus)
		ssdd->free_bus(ssdd);
	if(ov5640->clk)
	v4l2_clk_put(ov5640->clk);

	v4l2_async_unregister_subdev(sd);
	mutex_destroy(&ov5640->lock);
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);

	return 0;
}

static const struct i2c_device_id ov5640_id[] = {
	{ "ov5640", 0 },
	/* sentinel */
	{  },
};
MODULE_DEVICE_TABLE(i2c, ov5640_id);

static struct i2c_driver ov5640_i2c_driver = {
	.driver = {
		.name	= DRIVER_NAME,
	},
	.probe		= ov5640_probe,
	.remove		= ov5640_remove,
	.id_table	= ov5640_id,
};

module_i2c_driver(ov5640_i2c_driver);

MODULE_AUTHOR("George zheng <george.zheng@embest-tech.com>");
MODULE_DESCRIPTION("ov5640 CMOS Image Sensor driver");
MODULE_LICENSE("GPL v2");
