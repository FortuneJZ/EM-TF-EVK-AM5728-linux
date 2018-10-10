
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include "tc358746.h"

#define I2C_RETRY_DELAY		5
#define I2C_RETRIES		    5
 
#define I2C_TEST_DO         1

struct tc358746_data {
    struct i2c_client *client;
	struct mutex lock;  // lock for sysfs operations
};

struct tc358746_data *tc358746_des;

static int tc358746_regw(struct i2c_client *client, u8 addr1, u8 addr2, u8 data1, u8 data2)
{
	int err;
	int tries = 0;

    u8 buf[4];
    struct i2c_msg msg[1];

    buf[0] = addr1;
    buf[1] = addr2;
    buf[2] = data1;
    buf[3] = data2;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 4;
	msg[0].buf = buf;

    printk(KERN_ALERT"%s: 0x%2x 0x%2x 0x%2x 0x%2x 0x%2x\n", __func__,
           msg[0].addr, 
           msg[0].buf[0],
           msg[0].buf[1],
           msg[0].buf[2],
           msg[0].buf[3]);

	do {
		err = i2c_transfer(client->adapter, msg, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		printk(KERN_ALERT"%s: err = %d\n", __func__, err);
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int tc358746_rst(void)
{
    printk(KERN_ALERT"%s\n", __func__);

#if 0
    // GPIO HW reset
	if (gpio_request(TC358746_RST_GPIO, "TC358746_RST")) {
        printk(KERN_ALERT"Can't request GPIO pin %d for TC358746_REST\n", TC358746_RST_GPIO);
    }
    else {
        printk(KERN_ALERT"Request GPIO pin %d 0\n", TC358746_RST_GPIO);
        gpio_direction_output(TC358746_RST_GPIO, 0);
        msleep_interruptible(1000);
        printk(KERN_ALERT"Request GPIO pin %d 1\n", TC358746_RST_GPIO);
        gpio_direction_output(TC358746_RST_GPIO, 1);
        gpio_free(TC358746_RST_GPIO);
        msleep_interruptible(1000);
    }
#else
    gpio_set_value(rst_gpio, 0);
    msleep_interruptible(1000);
    gpio_set_value(rst_gpio, 1);
    msleep_interruptible(1000);
#endif
    return 0;
}

static int tc358746_core_init(void)
{
    struct i2c_client *client = tc358746_des->client;

    printk(KERN_ALERT"%s\n", __func__);

    tc358746_regw(client, 0x00, 0x02, 0x00, 0x01);
    msleep_interruptible(1000);
    tc358746_regw(client, 0x00, 0x02, 0x00, 0x00);
	tc358746_regw(client, 0x00, 0x16, 0x10, 0x4F);
	tc358746_regw(client, 0x00, 0x18, 0x04, 0x03);
	msleep_interruptible(1000);
	tc358746_regw(client, 0x00, 0x18, 0x04, 0x13);
	tc358746_regw(client, 0x00, 0x20, 0x00, 0x11);
	tc358746_regw(client, 0x00, 0x60, 0x80, 0x12);
	tc358746_regw(client, 0x00, 0x06, 0x00, 0x32);
	tc358746_regw(client, 0x00, 0x08, 0x00, 0x61);
	tc358746_regw(client, 0x00, 0x04, 0x81, 0x45);

    return 0;
}

static ssize_t tc358746_sysfs_rst(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, 
                                  size_t count)
{
    printk(KERN_ALERT"%s\n", __func__);
    
    tc358746_rst();
    return count;
}

static ssize_t tc358746_sysfs_init(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, 
                                  size_t count)
{
    printk(KERN_ALERT"%s\n", __func__);
    tc358746_core_init();
    return count;
}

static ssize_t tc358746_sysfs_wr(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, 
                                  size_t count)
{
    int ret;
    int addr1, addr2, data1, data2;
    struct i2c_client *client = tc358746_des->client;

    printk(KERN_ALERT"%s %s", __func__, buf);

    ret = sscanf(buf, "%x %x %x %x", &addr1, &addr2, &data1, &data2);
    if (4 != ret) {
        printk(KERN_ALERT"Syntax error. Format reg value\n");
        goto exit;
    }

	tc358746_regw(client, addr1, addr2, data1, data2);
exit:
    return count;
}

static ssize_t tc358746_sysfs_ver(struct device *dev,
                                  struct device_attribute *attr,
                                  char *buf)
{
    return sprintf(buf, "v0.1\n");
}

static DEVICE_ATTR(rst, S_IWUSR, NULL, tc358746_sysfs_rst);
static DEVICE_ATTR(init, S_IWUSR, NULL, tc358746_sysfs_init);
static DEVICE_ATTR(wr, S_IWUSR, NULL, tc358746_sysfs_wr);
static DEVICE_ATTR(ver, S_IRUGO, tc358746_sysfs_ver, NULL);

static struct attribute *tc358746_attributes[] = {
    &dev_attr_rst.attr,
    &dev_attr_init.attr,
    &dev_attr_wr.attr,
    &dev_attr_ver.attr,
    NULL
};

static const struct attribute_group tc358746_attr_group = {
    .attrs = tc358746_attributes,
};

static void tc358746_default_init(void){
#if 0
0x0002, 0x0001  //reset 1
0x0002, 0x0000  // reset 0 , 拉高再拉低，复位然后回到工作状态
0x0016, 0x40A0  // PLL Control Register 0， 设置分频比，反正是时钟的配置，具体的意义不明白，但是必须要配置的。
0x0018, 0x0013  // PLL Control Register 0， PLL reset + PLL enable + Clock enable
0x0020, 0x0000 //  时钟设置 (ppi_clk = PLL_CLK DIV 8) + (MclkRef = PLL_CLK DIV 8) +（sys_clk = PLL_CLK DIV 8）
0x000c, 0x0201 //  MCLK 设置， Total MClk divider = (mclk_high + 1) + (mclk_low + 1) = （2*MclkRef Count +1 ）+ （2*MclkRef Count +1 ）
0x0006, 0x0062 // FiFo Control Register， 设置一个leve值，当前的是6+2, when reaches to this level,FiFo controller asserts FiFoRdy for Parallel portto 　　　　　　　　　　　　 // startoutput data
0x0008, 0x0020 // data format, 设置为RAW12
0x0060, 0x8009 //  MIPI PHY Time Delay： Delay = (9+1) x PPIRXCLK， TC TERM selection 需要设置成1，也就是这个15bit 为1，即8
0x0004, 0x0044 //  Parallel Port Enable + i2c地址每个字节传输完后递增
#endif
    struct i2c_client *client = tc358746_des->client;

    printk(KERN_ALERT"%s\n", __func__);
#if 1
    tc358746_regw(client, 0x00, 0x02, 0x00, 0x01);
    msleep_interruptible(100);
    tc358746_regw(client, 0x00, 0x02, 0x00, 0x00);
	tc358746_regw(client, 0x00, 0x16, 0x40, 0xA0);//0x104F
	tc358746_regw(client, 0x00, 0x18, 0x04, 0x03);
	msleep_interruptible(100);
	tc358746_regw(client, 0x00, 0x18, 0x04, 0x13);
	tc358746_regw(client, 0x00, 0x20, 0x00, 0x00);//0x11
	tc358746_regw(client, 0x00, 0x0c, 0x02, 0x01);//add
	tc358746_regw(client, 0x00, 0x06, 0x00, 0x62);//0x32
	tc358746_regw(client, 0x00, 0x08, 0x00, 0x01);//(DataFmt:TODO [7:4] 0->RAW8 3->RGB888 6->YUV422-8bit
	tc358746_regw(client, 0x00, 0x60, 0x80, 0x09);//0x8012
	tc358746_regw(client, 0x00, 0x04, 0x00, 0x45);//0x8145
#else
    tc358746_regw(client, 0x00, 0x02, 0x00, 0x01);
    msleep_interruptible(1000);
    tc358746_regw(client, 0x00, 0x02, 0x00, 0x00);
	tc358746_regw(client, 0x00, 0x16, 0x10, 0x4F);
	tc358746_regw(client, 0x00, 0x18, 0x04, 0x03);
	msleep_interruptible(1000);
	tc358746_regw(client, 0x00, 0x18, 0x04, 0x13);
	tc358746_regw(client, 0x00, 0x20, 0x00, 0x11);
	tc358746_regw(client, 0x00, 0x60, 0x80, 0x12);
	tc358746_regw(client, 0x00, 0x06, 0x00, 0x32);
	tc358746_regw(client, 0x00, 0x08, 0x00, 0x61);
	tc358746_regw(client, 0x00, 0x04, 0x81, 0x45);
#endif
}
static int tc358746_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	int err = 0;
    struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

    printk(KERN_ALERT"%s: client %p, addr 0x%x\n", __func__, client, client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		printk(KERN_INFO "i2c_check_functionality error\n");
		return -EIO;
	}

    struct device *dev = &client->dev;
    int retval = -1;
	/* request reset pin */
	rst_gpio = of_get_named_gpio(dev->of_node, "rst-gpios", 0);
	if (!gpio_is_valid(rst_gpio)) {
		dev_warn(dev, "no sensor reset pin available\n");
		rst_gpio = -1;
	} else {
		retval = devm_gpio_request_one(dev, rst_gpio, GPIOF_OUT_INIT_HIGH,
						"TC358746_RST");
		if (retval < 0) {
			dev_warn(dev, "Failed to set reset pin\n");
			return retval;
		}
	}

    tc358746_des->client = client;
    i2c_set_clientdata(client, tc358746_des);

    mutex_init(&tc358746_des->lock);

    err = sysfs_create_group(&client->dev.kobj, &tc358746_attr_group);
    if (err) {
        printk(KERN_ALERT"%s: sysfs_create_group error\n", __func__);
        return -ENXIO;
    }
    
    tc358746_rst();
    tc358746_default_init();
    return err;
}

static int tc358746_remove(struct i2c_client *client)
{
    sysfs_remove_group(&client->dev.kobj, &tc358746_attr_group);
    i2c_set_clientdata(client, NULL);

    printk(KERN_ALERT"%s: client %p, addr 0x%x\n", __func__, client, client->addr);
    return 0;
}

static int tc358746_suspend(struct i2c_client *client, pm_message_t state)
{
	return 0;
}

static int tc358746_resume(struct i2c_client *client)
{
	return 0;
}

static struct i2c_device_id tc358746_id[] = {
	{ TC358746_NAME, 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, tc358746_id);

static struct i2c_driver tc358746_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = TC358746_NAME,
	},
	.probe = tc358746_probe,
	.remove = tc358746_remove,
//    .suspend = tc358746_suspend,
//    .resume = tc358746_resume,
	.id_table = tc358746_id,
};


static int __init tc358746_init(void)
{
    int ret;
    
    printk(KERN_ALERT"%s\n", __func__);

    tc358746_des= kzalloc(sizeof(struct tc358746_data), GFP_KERNEL);
    if (tc358746_des == NULL) {
        printk(KERN_ALERT"%s: allocation error\n", __func__);
        ret = -ENOMEM;
        return ret;
    }

    memset(tc358746_des, 0x0, sizeof(struct tc358746_data));

    ret = i2c_add_driver(&tc358746_i2c_driver);
    if( ret < 0 ) {
        printk(KERN_ERR"%s i2c_add_driver error\n",__func__);
        kfree(tc358746_des);
        return ret;
    }

    return 0; 
}

static void __exit tc358746_exit(void)
{
    printk(KERN_ALERT"%s\n", __func__);

    if (tc358746_des) {
        i2c_del_driver(&tc358746_i2c_driver);
        kfree(tc358746_des);
        tc358746_des = NULL;
    }
}

module_init(tc358746_init);
module_exit(tc358746_exit);

MODULE_DESCRIPTION("Toshiba tc358746");
MODULE_LICENSE("GPL");

