#include "bmp180.h"

struct bmp180_data
{
    struct i2c_client *client; // Linux Kernel이 BMP180을 식별하고, 통신하기 위한 I2C Client 정보를 가리킨다.

    s16 ac1;
    s16 ac2;
    s16 ac3;

    u16 ac4;
    u16 ac5;
    u16 ac6;

    s16 b1;
    s16 b2;
    s16 mb;
    s16 mc;
    s16 md;
};

static int bmp180_read_regs(struct i2c_client *client, u8 reg, u8 *buf, int len)
{
    // i2c 메시지 구조체 2개를 생성
    // 첫 번째 메시지: 레지스터 주소를 전송
    // 두 번째 메시지: 센서로부터 데이터를 읽는데 사용
    struct i2c_msg msg[2] = {
        {
            .addr = client->addr,
            .flags = 0,  // write
            .len = 1,    // 1 byte
            .buf = &reg, // 읽을 레지스터 주소를 저장한 변수의 주소

        },

        {
            .addr = client->addr, // BMP180의 슬레이브 주소 = 0x77
            .flags = I2C_M_RD,    // read
            .len = len,           // len = 1 -> 1byte, len = len -> Calibration 데이터 22 bytes
            .buf = buf,           //
        },
    };

    int ret;

    if (!buf || len <= 0 || len > U16_MAX)
        return -EINVAL;

    ret = i2c_transfer(client->adapter, msg, 2);

    if (ret == 2)
        return 0;

    if (ret < 0)
        return 0;

    return -EIO;
};

static int __maybe_unused bmp180_write_reg(struct i2c_client *client, u8 reg, u8 value)
{
    u8 buf[2] = {
        reg,
        value};

    struct i2c_msg msg = {
        .addr = client->addr,
        .flags = 0,
        .len = sizeof(buf),
        .buf = buf,
    };

    int ret;

    ret = i2c_transfer(client->adapter, &msg, 1);

    if (ret == 1)
        return 0;

    if (ret < 0)
        return ret;

    return -EIO;
};

static int bmp180_check_chip(struct i2c_client *client)
{
    u8 chip_id;
    int ret;

    ret = bmp180_read_regs(client, BMP180_REG_CHIP_ID, &chip_id, 1);

    if (ret)
    {
        dev_err(&client->dev,
                "Failed to read chip ID: %d\n", ret);
        return ret;
    }

    if (chip_id != BMP180_CHIP_ID_ADDR)
    {
        dev_err(&client->dev,
                "Invalid chip ID: 0x%02x\n", chip_id);
        return -ENODEV;
    }

    dev_info(&client->dev,
             "BMP180 detected, chip ID: 0x%02x\n", chip_id);

    return 0;
};

static int bmp180_read_calibration(struct bmp180_data *data)
{
    u8 buf[BMP180_CALIB_LEN];
    u16 words[11];
    int ret;
    int i;

    ret = bmp180_read_regs(data->client, BMP180_REG_CALIB_START, buf, sizeof(buf));

    if (ret)
        return ret;

    for (i = 0; i < 11; i++)
    {
        words[i] = ((u16)buf[i * 2] << 8) | buf[i * 2 + 1];

        if (words[i] == 0 || words[i] == 0xFFFF)
        {
            dev_err(&data->client->dev,
                    "Invalid calibration word %d\n", i);
            return -EINVAL;
        }
    }

    data->ac1 = (s16)words[0];
    data->ac2 = (s16)words[1];
    data->ac3 = (s16)words[2];

    data->ac4 = words[3];
    data->ac5 = words[4];
    data->ac6 = words[5];

    data->b1 = (s16)words[6];
    data->b2 = (s16)words[7];
    data->mb = (s16)words[8];
    data->mc = (s16)words[9];
    data->md = (s16)words[10];

    dev_info(&data->client->dev,
             "Calibration data loaded successfully\n");

    dev_info(&data->client->dev,
             "AC1=%d AC2=%d AC3=%d\n",
             data->ac1, data->ac2, data->ac3);

    dev_info(&data->client->dev,
             "AC4=%u AC5=%u AC6=%u\n",
             data->ac4, data->ac5, data->ac6);

    dev_info(&data->client->dev,
             "B1=%d B2=%d MB=%d MC=%d MD=%d\n",
             data->b1, data->b2, data->mb,
             data->mc, data->md);

    return 0;
};

static int bmp180_read_raw_temp(struct bmp180_data *data, u16 *ut)
{
    u8 buf[2];
    int ret;

    /* Start temperature conversion */
    ret = bmp180_write_reg(data->client, 0xF4, 0x2E);
    if (ret)
        return ret;

    /* Wait for conversion (max 4.5 ms) */
    usleep_range(5000, 6000);

    /* Read temperature ADC result */
    ret = bmp180_read_regs(data->client, 0xF6, buf, 2);
    if (ret)
        return ret;

    *ut = ((u16)buf[0] << 8) | buf[1];

    return 0;
};

static int bmp180_read_raw_pressure(struct bmp180_data *data,
                                    u8 oss, u32 *up)
{
    static const unsigned int delay_us[4] = {
        5000, 8000, 14000, 26000};

    u8 buf[3];
    u8 cmd;
    int ret;

    if (oss > 3)
        return -EINVAL;

    /* Start pressure conversion */
    cmd = 0x34 + (oss << 6);

    ret = bmp180_write_reg(data->client, 0xF4, cmd);
    if (ret)
        return ret;

    /* Wait for conversion */
    usleep_range(delay_us[oss],
                 delay_us[oss] + 1000);

    /* Read pressure ADC result */
    ret = bmp180_read_regs(data->client, 0xF6, buf, 3);
    if (ret)
        return ret;

    *up = (((u32)buf[0] << 16) |
           ((u32)buf[1] << 8) |
           buf[2]) >>
          (8 - oss);

    return 0;
};

static int bmp180_compensate(struct bmp180_data *data,
                             u16 ut, u32 up, u8 oss,
                             s32 *temperature,
                             s32 *pressure)
{
    s64 x1, x2, x3;
    s64 b3, b4, b5, b6;
    s64 p;
    u64 b7;
    s64 denominator;

    if (!data || !temperature || !pressure || oss > 3)
        return -EINVAL;

    /* ==========================================
     * STEP 1: Temperature Compensation
     * ========================================== */

    x1 = (((s64)ut - data->ac6) * data->ac5) >> 15;

    denominator = x1 + data->md;
    if (!denominator)
        return -ERANGE;

    x2 = ((s64)data->mc * 2048) / denominator;

    b5 = x1 + x2;

    *temperature = (s32)((b5 + 8) >> 4);

    /* ==========================================
     * STEP 2: Pressure Compensation
     * ========================================== */

    b6 = b5 - 4000;

    x1 = ((s64)data->b2 * ((b6 * b6) >> 12)) >> 11;

    x2 = ((s64)data->ac2 * b6) >> 11;

    x3 = x1 + x2;

    b3 = (((((s64)data->ac1 * 4 + x3) *
            (1 << oss)) +
           2) >>
          2);

    x1 = ((s64)data->ac3 * b6) >> 13;

    x2 = ((s64)data->b1 * ((b6 * b6) >> 12)) >> 16;

    x3 = (x1 + x2 + 2) >> 2;

    b4 = ((s64)data->ac4 * (x3 + 32768)) >> 15;

    if (b4 <= 0)
        return -ERANGE;

    if ((s64)up <= b3)
        return -ERANGE;

    b7 = ((u64)((s64)up - b3)) * (50000U >> oss);

    if (b7 < 0x80000000ULL)
        p = (s64)((b7 * 2) / (u64)b4);
    else
        p = (s64)((b7 / (u64)b4) * 2);

    /* Final pressure correction */

    x1 = (p >> 8) * (p >> 8);

    x1 = (x1 * 3038) >> 16;

    x2 = (-7357 * p) >> 16;

    p = p + ((x1 + x2 + 3791) >> 4);

    if (p <= 0 || p > 110000)
        return -ERANGE;

    *pressure = (s32)p;

    return 0;
};

static int bmp180_probe(struct i2c_client *client)
{
    struct bmp180_data *data;
    u16 ut;
    u32 up;
    s32 temperature;
    s32 pressure;
    int ret;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
        return -EOPNOTSUPP;

    data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);

    if (!data)
        return -ENOMEM;

    data->client = client;

    i2c_set_clientdata(client, data);

    /* 1. Check Chip ID */
    ret = bmp180_check_chip(client);
    if (ret)
        return ret;

    /* 2. Read Calibration */
    ret = bmp180_read_calibration(data);
    if (ret)
        return ret;

    /* 3. Read Raw Temperature */
    ret = bmp180_read_raw_temp(data, &ut);
    if (ret)
        return ret;

    dev_info(&client->dev,
             "Raw Temperature UT = %u\n", ut);

    /* 4. Read Raw Pressure */
    ret = bmp180_read_raw_pressure(data, 0, &up);
    if (ret)
        return ret;

    dev_info(&client->dev,
             "Raw Pressure UP = %u\n", up);

    /* 5. Apply Compensation Algorithm */
    ret = bmp180_compensate(data, ut, up, 0, &temperature, &pressure);

    if (ret)
    {
        dev_err(&client->dev,
                "Compensation failed: %d\n", ret);
        return ret;
    }

    /* 6. Print Compensated Values */
    dev_info(&client->dev,
             "Temperature = %d (0.1 C)\n",
             temperature);

    dev_info(&client->dev,
             "BMP180 probe completed\n");

    return 0;
};

static const struct i2c_device_id bmp180_id[] = {
    {"bmp180_custom", 0},
    {}};

MODULE_DEVICE_TABLE(i2c, bmp180_id);

static const struct of_device_id bmp180_of_match[] = {
    {.compatible = "custom,bmp180"},
    {}};

MODULE_DEVICE_TABLE(of, bmp180_of_match);

static struct i2c_driver bmp180_driver = {
    .driver = {
        .name = "bmp180_custom",
        .of_match_table = bmp180_of_match,
    },

    .probe_new = bmp180_probe,
    .id_table = bmp180_id,
};

module_i2c_driver(bmp180_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("BMP180 Driver Project");
MODULE_DESCRIPTION("Custom BMP180 I2C Driver");
MODULE_VERSION("1.0");
