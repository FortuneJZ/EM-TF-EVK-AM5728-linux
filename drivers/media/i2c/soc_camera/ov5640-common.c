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

/*static*/ int pwn_gpio;
EXPORT_SYMBOL(pwn_gpio);
/* reset gpio */
/*static*/ int rst_gpio;
EXPORT_SYMBOL(rst_gpio);

#include "ov5640-common.h"

/*!
 * ov5640_write - write a ov5640 register
 *			   
 * @client: pointer to standard i2c client structure
 * @reg: the register address
 * @val: the value to write
 *
 * Return 0 if successful, otherwise -EIO.
 */
int ov5640_write(struct i2c_client *client, u16 reg, u8 val)
{
	int ret;
	unsigned char data[3] = { reg >> 8, reg & 0xff, val };

	if(client == NULL)
		return  -EIO;

	ret = i2c_master_send(client, data, 3);
	if (ret < 3) {
		dev_err(&client->dev, "%s: i2c write error, reg: %x\n",
			__func__, reg);
		return ret < 0 ? ret : -EIO;
	}

	return 0;
}
EXPORT_SYMBOL(ov5640_write);
/*!
 * ov5640_read - read a ov5640 register
 *			   
 * @client: pointer to standard i2c client structure
 * @reg: the register address
 * @val: pointer to the read value
 *
 * Return 0 if successful, otherwise -EIO.
 */
int ov5640_read(struct i2c_client *client, u16 reg, u8 *val)
{
	int ret;
	/* We have 16-bit i2c addresses - care for endianness */
	unsigned char data[2] = { reg >> 8, reg & 0xff };

	if(client == NULL)
		return  -EIO;

	ret = i2c_master_send(client, data, 2);
	if (ret < 2) {
		dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
			__func__, reg);
		return ret < 0 ? ret : -EIO;
	}

	ret = i2c_master_recv(client, val, 1);
	if (ret < 1) {
		dev_err(&client->dev, "%s: i2c read error, reg: %x\n",
				__func__, reg);
		return ret < 0 ? ret : -EIO;
	}
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
	return 0;

}
EXPORT_SYMBOL(ov5640_read);

/*!
 * ov5640_write_array - write array to ov5640
 *			   
 * @client: pointer to standard i2c client structure
 * @regs: pointer to the sensor_register struct
 *
 * Return 0 if successful.
 */
int ov5640_write_array(struct i2c_client *client,
			      const struct sensor_register *regs)
{
	int i, ret = 0;

	for (i = 0; ret == 0 && regs[i].addr; i++){
		ret = ov5640_write(client, regs[i].addr, regs[i].value);
#if 1
		if(i%3)
		embest_debug("0x%x  0x%x \t",regs[i].addr, regs[i].value);
		else
		embest_debug("0x%x  0x%x \n",regs[i].addr, regs[i].value);
#endif
	}
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
	return ret;
}
EXPORT_SYMBOL(ov5640_write_array);

static int  module_s_mirror(struct i2c_client  *client, unsigned int mirror)
{
	int ret = 0;
    u8 reg_0x3821 = 0x00;
	
	ov5640_read(client, 0x3821, &reg_0x3821);
    if (mirror) {
        reg_0x3821 |= (0x3 << 1);
    } else {
        reg_0x3821 &= ~(0x3 << 1);
    }
	ov5640_write(client, 0x3821, reg_0x3821);
	
	return ret;
}

static int  module_s_flip(struct i2c_client  *client, unsigned int flip)
{
	int ret = 0;
	u8 reg_0x3820 = 0x40;
	
    ov5640_read(client, 0x3820, &reg_0x3820);
    if (flip) {
        reg_0x3820 |= (0x3 << 1);
    } else {
        reg_0x3820 &= ~(0x3 << 1);
    }
	ov5640_write(client, 0x3820, reg_0x3820);
	return ret;
}

static int ov5640_set_test_pattern(struct i2c_client  *client, int value)
{
	if (value == 1) {
        embest_debug("embest_debug: ov5640_set_test_pattern1=%d\n",value);
    } else if( value == 0 )
	{
        embest_debug("embest_debug: ov5640_set_test_pattern0=%d\n",value);
    }
	
	if ( value == 3)
	{
		embest_debug("embest_debug: ov5640_set_test_pattern3=%d\n",value);
		
	}
	return 0;
}

int ov5640_s_ctrl(struct v4l2_ctrl *ctrl)
{
	struct v4l2_subdev *sd =
		&container_of(ctrl->handler, struct ov5640, ctrl_handler)->sd;
	struct i2c_client  *client = v4l2_get_subdevdata(sd);

embest_debug("embest_debug: V4L2_CID_SCENE_MODE=%d\n",V4L2_CID_SCENE_MODE_ZGB);
	switch (ctrl->id) {
		case V4L2_CID_BRIGHTNESS:
			embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
/*			ov5640_write(client, 0x3a1e, ctrl->val - 0x20);
			ov5640_write(client, 0x3a1b, ctrl->val);*/
			switch (ctrl->val){
				case 0:
					ov5640_write_array(client, module_brightness_0_regs);
					break;
				case 1:
					ov5640_write_array(client, module_brightness_1_regs);
					break;
				case 2:
					ov5640_write_array(client, module_brightness_2_regs);
					break;
				case 3:
					ov5640_write_array(client, module_brightness_3_regs);
					break;
				case 4:
					ov5640_write_array(client, module_brightness_4_regs);
					break;
				case 5:
					ov5640_write_array(client, module_brightness_5_regs);
					break;
				case 6:
					ov5640_write_array(client, module_brightness_6_regs);
					break;
				case 7:
					ov5640_write_array(client, module_brightness_7_regs);
					break;
				case 8:
					ov5640_write_array(client, module_brightness_8_regs);
					break;
				default:
					return -EINVAL;
			}
			
			return 0;
		case V4L2_CID_CONTRAST:
			embest_debug("embest_debug: V4L2_CID_CONTRAST = %d\n",ctrl->val);
			switch (ctrl->val){
				case 3:
					embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
					ov5640_write_array(client, module_contrast_6_regs);
					return 0;
				case 2:
					ov5640_write_array(client, module_contrast_5_regs);
					return 0;
				case 1:
					ov5640_write_array(client, module_contrast_4_regs);
					return 0;
				case 0:
					embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
					ov5640_write_array(client, module_contrast_3_regs);
					return 0;
				case -1:
					ov5640_write_array(client, module_contrast_2_regs);
					return 0;
				case -2:
					ov5640_write_array(client, module_contrast_1_regs);
					return 0;
				case -3:
					embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
					ov5640_write_array(client, module_contrast_0_regs);
					return 0;
				default:
					return -EINVAL;
			}
		case V4L2_CID_SCENE_MODE_ZGB:
			embest_debug("embest_debug: V4L2_CID_SCENE_MODE = %d\n",ctrl->val);
			switch (ctrl->val){
				case V4L2_SCENE_MODE_NIGHT:
					embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
					ov5640_write(client, 0x3a00, 0x3c);
					return 0;
				case V4L2_SCENE_MODE_SUNSET:
					embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
					ov5640_write(client, 0x3a00, 0x38);
					return 0;
				default:
					return -ERANGE;
			}
		case V4L2_CID_EXPOSURE:
			switch(ctrl->val){
				case 4:
					ov5640_write_array(client, module_exp_comp_pos4_regs);
					break;
				case 3:
					ov5640_write_array(client,module_exp_comp_pos3_regs);
					break;
				case 2:
					ov5640_write_array(client,module_exp_comp_pos2_regs);
					break;
				case 1:
					ov5640_write_array(client,module_exp_comp_pos1_regs);
					break;
				case 0:
					ov5640_write_array(client,module_exp_comp_zero_regs);
					break;
				case -1:
					ov5640_write_array(client,module_exp_comp_neg1_regs);
					break;
				case -2:
					ov5640_write_array(client,module_exp_comp_neg2_regs);
					break;
				case -3:
					ov5640_write_array(client,module_exp_comp_neg3_regs);
					break;
				case -4:
					ov5640_write_array(client,module_exp_comp_neg4_regs);
					break;
				default:
					break;		
			}
			return 0;
		case V4L2_CID_MIRROR_ZGB:
			return module_s_mirror(client,ctrl->val);
		case V4L2_CID_FLIP_ZGB:
			return module_s_flip(client,ctrl->val);
		case V4L2_SPECIAL_EFFECTS_ZGB:
			embest_debug("embest_debug: V4L2_SPECIAL_EFFECTS_ZGB = %d\n",ctrl->val);
			switch(ctrl->val){
				case Normal_Effect:	//off
					ov5640_write_array(client, module_Normal_Effect_regs);
					return 0;
				case Blueish_Effect: //cool light
					ov5640_write_array(client,module_Blueish_Effect_regs);
					return 0;
				case Redish_Effect: //warm
					ov5640_write_array(client,module_Redish_Effect_regs);
					return 0;
				case BandW_Effect: //black and white
					ov5640_write_array(client,module_BandW_Effect_regs);
					return 0;
				case Sepia_Effect: //Sepia
					ov5640_write_array(client,module_Sepia_Effect_regs);
					return 0;
				case Negative_Effect: //Negative
					ov5640_write_array(client,module_Negative_Effect_regs);
					return 0;
				case Greenish_Effect: //Greenish
					ov5640_write_array(client,module_Greenish_Effect_regs);
					break;
				case Overexposure_Effect: //Overexposure
					embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
					ov5640_write_array(client,Overexposure_Effect_regs);
					return 0;
				case Solarize_Effect: //Solarize
					embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
					ov5640_write_array(client,module_Solarize_Effect_regs);
					return 0;
				default:
					return -EINVAL;		
			}
			return 0;
		case V4L2_CID_TEST_PATTERN:
			return ov5640_set_test_pattern(client, ctrl->val);
		case V4L2_CID_WHITE_BALANCE_ZGB:
			embest_debug("embest_debug: V4L2_SPECIAL_EFFECTS_ZGB = %d\n",ctrl->val);
			switch(ctrl->val){
				case WhiteBalance_Auto:	
					ov5640_write_array(client, module_WhiteBalance_Auto_regs);
					return 0;
				case WhiteBalance_Sunny: 
					ov5640_write_array(client,module_WhiteBalance_Sunny_regs);
					return 0;
				case WhiteBalance_Office: 
					ov5640_write_array(client,module_WhiteBalance_Office_regs);
					return 0;
				case WhiteBalance_Cloudy:
					ov5640_write_array(client,module_WhiteBalance_Cloudy_regs);
					return 0;
				case WhiteBalance_Home:
					ov5640_write_array(client,module_WhiteBalance_Home_regs);
					return 0;
				default:
					return -EINVAL;		
			}
			return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL(ov5640_s_ctrl);

/*!
 * ov5640_get_default_format - set the default ov5640 format
 *			   
 * @format: pointer to standard v4l2_mbus_framefmt structure
 */
void ov5640_get_default_format(struct v4l2_mbus_framefmt *format)
{
	format->width = ov5640_framesizes[2].width;
	format->height = ov5640_framesizes[2].height;
	format->colorspace = V4L2_COLORSPACE_SRGB;
	format->code = ov5640_formats[0].code;
	format->field = V4L2_FIELD_NONE;
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
}
EXPORT_SYMBOL(ov5640_get_default_format);

/*!
 * ov5640_enum_mbus_code - set the ov5640 format code
 *			   
 * @sd: pointer to standard v4l2_subdev structure
 * @cfg: pointer to standard v4l2_subdev_pad_config structure
 * @code: pointer to standard v4l2_subdev_mbus_code_enum structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
int ov5640_enum_mbus_code(struct v4l2_subdev *sd,
				 struct v4l2_subdev_pad_config *cfg,
				 struct v4l2_subdev_mbus_code_enum *code)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	if (code->index >= ARRAY_SIZE(ov5640_formats))
		return -EINVAL;

	code->code = ov5640_formats[code->index].code;
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);

	return 0;
}
EXPORT_SYMBOL(ov5640_enum_mbus_code);

/*!
 * ov5640_enum_frame_sizes - enum the bus support frame size
 *			   
 * @sd: pointer to standard v4l2_subdev structure
 * @cfg: pointer to standard v4l2_subdev_pad_config structure
 * @fse: pointer to standard v4l2_subdev_frame_size_enum structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
int ov5640_enum_frame_sizes(struct v4l2_subdev *sd,
				   struct v4l2_subdev_pad_config *cfg,
				   struct v4l2_subdev_frame_size_enum *fse)
{
	int i = ARRAY_SIZE(ov5640_formats);

	if (fse->index >= ARRAY_SIZE(ov5640_framesizes))
		return -EINVAL;

	while (--i)
		if (fse->code == ov5640_formats[i].code)
			break;

	fse->code = ov5640_formats[i].code;

	fse->min_width  = ov5640_framesizes[fse->index].width;
	fse->max_width  = fse->min_width;
	fse->max_height = ov5640_framesizes[fse->index].height;
	fse->min_height = fse->max_height;
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
	
	return 0;
}
EXPORT_SYMBOL(ov5640_enum_frame_sizes);

/*!
 * ov5640_get_fmt - get the ov5640 format code
 *			   width and height
 * @sd: pointer to standard V4L2 subdev structure
 * @cfg: pointer to standard v4l2_subdev_pad_config structure
 * @fmt: pointer to standard v4l2_subdev_format structure
 *
 * Return 0 if successful.
 */
int ov5640_get_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *ov5640 = to_ov5640(sd);
	struct v4l2_mbus_framefmt *mf;

	dev_dbg(&client->dev, "ov5640_get_fmt\n");

	if (fmt->which == V4L2_SUBDEV_FORMAT_TRY) {
		mf = v4l2_subdev_get_try_format(sd, cfg, 0);
		mutex_lock(&ov5640->lock);
		fmt->format = *mf;
		mutex_unlock(&ov5640->lock);
		return 0;
	}

	mutex_lock(&ov5640->lock);
	fmt->format = ov5640->format;
	mutex_unlock(&ov5640->lock);

	embest_debug("embest_debug: ov5640_get_fmt: %x %dx%d\n",
		ov5640->format.code, ov5640->format.width,
		ov5640->format.height);

	return 0;
}
EXPORT_SYMBOL(ov5640_get_fmt);

/*!
 * ov5640_try_frame_size - set the frame size if is matchable
 *			   
 * @mf: pointer to standard v4l2_mbus_framefmt structure
 * @size: pointer to pointer to standard ov5640_framesize structure
 */
void ov5640_try_frame_size(struct v4l2_mbus_framefmt *mf,
				    const struct ov5640_framesize **size)
{
	const struct ov5640_framesize *fsize = &ov5640_framesizes[2];
	const struct ov5640_framesize *match = NULL;
	int i = ARRAY_SIZE(ov5640_framesizes);
	unsigned int min_err = UINT_MAX;

	while (i--) {
		int err = abs(fsize->width - mf->width)
				+ abs(fsize->height - mf->height);
		if ((err < min_err) && (fsize->regs[0].addr)) {
			min_err = err;
			match = fsize;
		}
		fsize++;
	}

	if (!match)
		match = &ov5640_framesizes[2];

	mf->width  = match->width;
	mf->height = match->height;

	if (size)
		*size = match;
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
}
EXPORT_SYMBOL(ov5640_try_frame_size);

/*!
 * ov5640_set_fmt - set the ov5640 format code
 *			   width and height
 * @sd: pointer to standard V4L2 subdev structure
 * @cfg: pointer to standard v4l2_subdev_pad_config structure
 * @fmt: pointer to standard v4l2_subdev_format structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
int ov5640_set_fmt(struct v4l2_subdev *sd,
			  struct v4l2_subdev_pad_config *cfg,
			  struct v4l2_subdev_format *fmt)
{
	unsigned int index = ARRAY_SIZE(ov5640_formats);
	struct v4l2_mbus_framefmt *mf = &fmt->format;
	const struct ov5640_framesize *size = NULL;
	struct ov5640 *ov5640 = to_ov5640(sd);
	int ret = 0;
	
	/* set the size pointer struct if match */ 
	ov5640_try_frame_size(mf, &size);

	while (--index >= 0)
		if (ov5640_formats[index].code == mf->code)
			break;

	if (index < 0)
		return -EINVAL;

	mf->colorspace = V4L2_COLORSPACE_SRGB;
	mf->code = ov5640_formats[index].code;
	mf->field = V4L2_FIELD_NONE;

	mutex_lock(&ov5640->lock);

	if (ov5640->streaming) {
		mutex_unlock(&ov5640->lock);
		return -EBUSY;
	}

	ov5640->frame_size = size;
	ov5640->format = fmt->format;
	ov5640->format_ctrl_regs = ov5640_formats[index].format_ctrl_regs;

	mutex_unlock(&ov5640->lock);

	/* set frame size regs */
	ov5640_write_array(ov5640->client, ov5640->frame_size->regs);
	/* set format size regs */
	ov5640_write_array(ov5640->client, ov5640->format_ctrl_regs);
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
	return ret;
}
EXPORT_SYMBOL(ov5640_set_fmt);

/*!
 * ov5640_get_framerate - get the ov5640 framerate
 *			   
 * @sd: pointer to standard V4L2 subdev structure
 * @a: pointer to standard V4L2 streamparm structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
int ov5640_get_framerate(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct ov5640 *ov5640 = to_ov5640(sd);

	if( ov5640->frame_rate > 30 )
	{
		pr_err("[%s][%d]Invaild framerate.\n",__FUNCTION__,__LINE__);
		return -EINVAL;
	}
	a->parm.capture.timeperframe.denominator = ((u32)ov5640->frame_rate) & 0x0000ffff;
	embest_debug("embest_debug: %s(%d) frame_rate = %d\n",__FUNCTION__,__LINE__,a->parm.capture.timeperframe.denominator);
	return 0;

}
EXPORT_SYMBOL(ov5640_get_framerate);

/*!
 * ov5640_set_framerate - set the ov5640 framerate
 *			   
 * @sd: pointer to standard V4L2 subdev structure
 * @a: pointer to standard V4L2 streamparm structure
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
int ov5640_set_framerate(struct v4l2_subdev *sd, struct v4l2_streamparm *a)
{
	struct ov5640 *ov5640 = to_ov5640(sd);
	struct i2c_client *client = ov5640->client;
	u8 reg_read_PLL_ctrl1;

	ov5640->frame_rate = (unsigned char)(a->parm.capture.timeperframe.denominator);
	if( ov5640->frame_rate > 30 )
	{
		pr_err("[%s][%d]Invaild framerate.\n",__FUNCTION__,__LINE__);
		return -EINVAL;
	}

	if(ov5640->frame_rate > 15)
	{
		if(ov5640->frame_size->width == 1024)
		{
			ov5640_write(client, 0x3035, 0x21);
			ov5640_write(client, 0x3036, 0x69);
		}else if(ov5640->frame_size->width == 1280){
			ov5640_write(client, 0x3035, 0x21);
			ov5640_write(client, 0x3036, 0x69);
		}else{
			ov5640_write(client, 0x3035, 0x11);
			ov5640_write(client, 0x3036, 0x46);
		}
	}
	else{
		if(ov5640->frame_size->width == 1024)
		{
			ov5640_write(client, 0x3035, 0x21);
			ov5640_write(client, 0x3036, 0x46);
		}else if(ov5640->frame_size->width == 1280){
			ov5640_write(client, 0x3035, 0x41);
			ov5640_write(client, 0x3036, 0x69);
		}else{
			ov5640_write(client, 0x3035, 0x21);
			ov5640_write(client, 0x3036, 0x46);
		}
	}
	ov5640_read(client, 0x3035, &reg_read_PLL_ctrl1);
	embest_debug("embest_debug: %s(%d) ov5640_set_framerate=%dfps reg_read_PLL_ctrl1=0x%x \n",__FUNCTION__,__LINE__,ov5640->frame_rate,reg_read_PLL_ctrl1);

	return 0;
}
EXPORT_SYMBOL(ov5640_set_framerate);

/*!
 * ov5640_s_stream - start and stop the capture streaming
 *			   
 * @sd: pointer to standard V4L2 subdev structure
 * @on: 1 is start streaming,0 is stop streaming.
 *
 * Return 0 if successful, otherwise -EINVAL.
 */
int ov5640_start_stop_stream(struct v4l2_subdev *sd, int on)
{
	struct ov5640 *ov5640 = to_ov5640(sd);
	int ret = 0;

	embest_debug("embest_debug: %s(%d) ov5640->streaming = %d\n",__FUNCTION__,__LINE__,on);

	mutex_lock(&ov5640->lock);

	on = !!on;

	if (!on) {
		/* Stop Streaming Sequence */
		ov5640->streaming = on;
		ret = ov5640_write(ov5640->client,0x3007, 0x00);
		goto unlock;
	}

	ov5640->streaming = on;
	ret = ov5640_write(ov5640->client,0x3007, 0xff);

unlock:
	mutex_unlock(&ov5640->lock);
	return ret;
}
EXPORT_SYMBOL(ov5640_start_stop_stream);

/*!
 * ov5640_reset - reset the device
 */
void ov5640_reset(void)
{
	/* camera reset */
	gpio_set_value(rst_gpio, 0);

	/* camera power down */
	msleep(5);
	gpio_set_value(pwn_gpio, 0);
	msleep(5);

	/* camera reset on */
	gpio_set_value(rst_gpio, 1);
	msleep(5);
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
}
EXPORT_SYMBOL(ov5640_reset);

/*!
 * ov5640_init - init the device and read the ID registers 
 *			   to find the device
 * @sd: pointer to standard V4L2 subdev structure
 *
 * Return 0 if successful, otherwise negative.
 */
int ov5640_init(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct ov5640 *ov5640 = to_ov5640(sd);
	int ret;
	u8 id_high, id_low;
	u16 id;

	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
	/* enable the clk */
	ret = soc_camera_power_on(&client->dev,&ov5640->ssdd_dt, ov5640->clk);
		if (ret < 0)
			return ret;
		
	/* hardware reset */
	ov5640_reset();
	
	/* read the ov5640 id */
	ret = ov5640_read(client, REG_CHIP_ID_HIGH, &id_high);
	if (ret < 0)
		goto done;
	id = id_high << 8;
	ret = ov5640_read(client, REG_CHIP_ID_LOW, &id_low);
	if (ret < 0)
		goto done;
	id |= id_low;

	dev_info(&client->dev, "Chip ID 0x%04x detected\n", id);

	if (id != OV5640_ID) {
		ret = -ENODEV;
		printk("camera ov5640 is not found\n");
		goto done;
	}

	printk("camera ov5640 is found\n");

	/* sysclk from pad */
	ov5640_write(client,0x3103, 0x11);

	/* software reset */
	ov5640_write(client,0x3008, 0x82);

	/* delay at least 5ms */
	msleep(10);

	/* set the defualt registers */
	ret = ov5640_write_array(client, ov5640_ini_vga);

	/* disable the device, wait app to enable */
	ov5640_write(client,0x3007, 0x00);

    /* {0x503d, 0x80, 0, 0},*/ //az: test bar8
	//ov5640_write(client,0x503d, 0x80);

	msleep(10);

done:
	return ret;

}
EXPORT_SYMBOL(ov5640_init);

/*!
 * ov5640_get_pdata - get the platform data. 
 *			  
 * @client: pointer to standard i2c_client structure
 *
 * Return the pointer to ov5640_platform_data.
 */
struct ov5640_platform_data *
ov5640_get_pdata(struct i2c_client *client)
{
	struct ov5640_platform_data *pdata;
	struct device_node *endpoint;
	int ret;

	if (!IS_ENABLED(CONFIG_OF) || !client->dev.of_node)
		return client->dev.platform_data;

	endpoint = of_graph_get_next_endpoint(client->dev.of_node, NULL);
	if (!endpoint)
		return NULL;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata)
		goto done;

	ret = of_property_read_u64(endpoint, "link-frequencies",
				   &pdata->link_frequency);
	if (ret) {
		dev_err(&client->dev, "link-frequencies property not found\n");
		pdata = NULL;
	}
	embest_debug("embest_debug: %s(%d)\n",__FUNCTION__,__LINE__);
done:
	of_node_put(endpoint);
	return pdata;
}
EXPORT_SYMBOL(ov5640_get_pdata);
MODULE_LICENSE("GPL v2");
