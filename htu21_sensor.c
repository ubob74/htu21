#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/delay.h>

struct htu21_data {
	struct i2c_client *client;
};

static int htu21_soft_reset(struct i2c_client *client)
{
	int ret;
	struct i2c_adapter *adap = client->adapter;
	struct i2c_msg msg[1];
	unsigned char data[0];

	msg->addr = client->addr;
	msg->flags = 0;
	msg->len = 1;
	msg->buf = data;

	/* Send reset command */
	data[0] = 0xFE;

	ret = i2c_transfer(adap, msg, 1);

	usleep_range(15000, 16000);
	return ret;
}

static int htu21_read_serial(struct i2c_client *client, u64 *sn)
{
	int err;
	struct i2c_adapter *adap = client->adapter;
	unsigned char snd_data[2];
	unsigned char rcv_data[8];
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 2,
			.buf = &snd_data[0],
		},
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = 8,
			.buf = &rcv_data[0],
		},
	};

	/* First memory access */
	snd_data[0] = 0xFA;
	snd_data[1] = 0x0F;

	err = i2c_transfer(adap, msg, 2);
	pr_err("err=%d\n", err);

	pr_err("%X %X %X\n", rcv_data[0], rcv_data[2], rcv_data[4]);

	/* TODO Second memory access */

	return err;
}

static int htu21_read_temp(struct i2c_client *client, char *buf)
{
	int err;
	struct i2c_adapter *adap = client->adapter;
	unsigned char snd_data[0];
	unsigned char rcv_data[3];
	u16 temp;
	struct i2c_msg msg[2] = {
		{
			.addr = client->addr,
			.flags = client->flags,
			.len = 1,
			.buf = &snd_data[0],
		},
		{
			.addr = client->addr,
			.flags = client->flags | I2C_M_RD,
			.len = 3,
			.buf = &rcv_data[0],
		},
	};

	snd_data[0] = 0xE3;

	err = i2c_transfer(adap, msg, 2);
	if (err < 0)
		return -1;

	pr_err("%x %x %x\n", rcv_data[0], rcv_data[1], rcv_data[2]);

	temp = (rcv_data[0] << 8) | rcv_data[1];
	return sprintf(buf, "%d\n", temp);
}

static ssize_t htu21_temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct htu21_data *htu21_data = (struct htu21_data *)dev_get_drvdata(dev);
	return htu21_read_temp(htu21_data->client, buf);
}

static ssize_t htu21_hum_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", "humidity");
}

static DEVICE_ATTR(htu21_temp, S_IRUGO, htu21_temp_show, NULL);
static DEVICE_ATTR(htu21_hum, S_IRUGO, htu21_hum_show, NULL);

static struct attribute *htu21_attrs[] = {
	&dev_attr_htu21_temp.attr,
	&dev_attr_htu21_hum.attr,
	NULL,
};

static const struct attribute_group htu21_attr_group = {
	.attrs = htu21_attrs,
};

static int htu21_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int err;
	u64 snum;
	struct htu21_data *htu21_data;

	htu21_soft_reset(client);

	err = htu21_read_serial(client, &snum);
	if (err < 0)
		pr_err("read serial error err=%d\n", err);

	err = sysfs_create_group(&client->dev.kobj, &htu21_attr_group);
	if (err)
		return -1;

	htu21_data = devm_kzalloc(&client->dev, sizeof(*htu21_data), GFP_KERNEL);
	if (!htu21_data)
		goto err_alloc;

	htu21_data->client = client;
	dev_set_drvdata(&client->dev, htu21_data);

	return 0;

err_alloc:
	sysfs_remove_group(&client->dev.kobj, &htu21_attr_group);
	return -ENOMEM;
}

static int htu21_remove(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &htu21_attr_group);
	dev_set_drvdata(&client->dev, NULL);
	return 0;
}

static const struct of_device_id htu21_match[] = {
	{ .compatible = "meas,htu21", },
	{}
};
MODULE_DEVICE_TABLE(of, htu21_match);

static struct i2c_driver htu21_driver = {
	.probe  = htu21_probe,
	.remove = htu21_remove,
	.driver = {
		.name       = "i2c-htu",
		.of_match_table = of_match_ptr(htu21_match),
	},
};

module_i2c_driver(htu21_driver);

MODULE_LICENSE("GPL");
