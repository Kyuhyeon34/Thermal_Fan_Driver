#include "fan_app.h"

/*
 * 성공: 0
 * 실패: 음수
 * 출력: milli-degree Celsius (m°C)
 */
int bmp180_get_temperature(int *temp_mc)
{
    FILE *fp;
    long value;

    if (temp_mc == NULL)
        return -1;

    fp = fopen(BMP180_TEMP_PATH, "r");
    if (fp == NULL) {
        perror("BMP180 fopen");
        return -1;
    }

    if (fscanf(fp, "%ld", &value) != 1) {
        fclose(fp);
        return -1;
    }

    fclose(fp);

    if (value < -40000 || value > 85000)
        return -1;

    *temp_mc = (int)value;

    return 0;
}