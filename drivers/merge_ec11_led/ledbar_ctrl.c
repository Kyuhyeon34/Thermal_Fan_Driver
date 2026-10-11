/*
 * ledbar_ctrl.c - 팀 전역변수 기반 LED 바 제어 (유저 공간)
 *
 * /dev/ledbar 드라이버(ledbar.ko)에 ioctl 로 칸 수를 보낸다.
 * 칸 수가 바뀔 때만 ioctl 을 호출하므로 루프에서 자주 불러도 부담 없음.
 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <sys/ioctl.h>

#include "ledbar_ioctl.h"
#include "fan_globals.h"
#include "ledbar_ctrl.h"

/* ---- 조정 가능한 값 ---- */
#define TEMP_1ST_LEVEL_C	21.0f	/* 이 온도부터 1칸 (21.x→1, 27.x→7, 28~→8) */
#define TEMP_HYST_C		0.2f	/* 경계 깜빡임 방지 폭, 0 이면 끔 */
#define POWER_ON_WAVE		1	/* WAIT→켜짐 때 웨이브 효과 (0 이면 끔) */

static int fd = -1;
static int shown_level = -1;		/* 지금 LED 에 표시 중인 칸 수 */
static int auto_level = -1;		/* AUTO 히스테리시스용 직전 값 */
static enum fan_mode prev_mode = WAIT;

static int clamp(int v, int lo, int hi)
{
	return v < lo ? lo : v > hi ? hi : v;
}

static void bar_set_level(int level)
{
	__s32 v = level;

	if (ioctl(fd, LEDBAR_SET_LEVEL, &v) < 0)
		perror("ledbar: LEDBAR_SET_LEVEL");
}

static void bar_wave(void)
{
	struct ledbar_anim a = { .anim = LEDBAR_ANIM_WAVE, .period_ms = 0 };

	if (ioctl(fd, LEDBAR_SET_ANIM, &a) < 0)
		perror("ledbar: LEDBAR_SET_ANIM");
}

int ledbar_temp_to_level(float temp, int prev)
{
	int lv = (int)floorf(temp - TEMP_1ST_LEVEL_C) + 1;

	lv = clamp(lv, 0, LEDBAR_MAX_LEDS);

	/* 경계 근처(±TEMP_HYST_C)에서는 직전 칸 수 유지 */
	if (prev >= 0 && prev <= LEDBAR_MAX_LEDS && lv != prev) {
		float lower = TEMP_1ST_LEVEL_C + prev - 1;	/* prev 칸이 시작되는 온도 */
		float upper = TEMP_1ST_LEVEL_C + prev;		/* prev+1 칸이 시작되는 온도 */

		if (temp >= lower - TEMP_HYST_C && temp < upper + TEMP_HYST_C)
			return prev;
	}
	return lv;
}

int ledbar_ctrl_level(void)
{
	return shown_level;
}

int ledbar_ctrl_init(void)
{
	fd = open(LEDBAR_DEV_PATH, O_RDWR);
	if (fd < 0) {
		perror("ledbar: " LEDBAR_DEV_PATH " 열기 실패 (insmod 했는지 확인)");
		return -1;
	}

	prev_mode = WAIT;
	bar_set_level(0);
	shown_level = 0;
	return 0;
}

void ledbar_ctrl_update(void)
{
	enum fan_mode mode = fan_mode;
	int level;

	if (fd < 0)
		return;

	switch (mode) {
	case AUTO:
		auto_level = ledbar_temp_to_level(TEMP, auto_level);
		level = auto_level;
		break;
	case MANUAL:
		/* 범위를 벗어난 값이 들어와도 LED 는 0~8 로만 표시 */
		level = clamp(FAN_LEVEL, 0, LEDBAR_MAX_LEDS);
		break;
	case WAIT:
	default:
		level = 0;
		break;
	}

	if (POWER_ON_WAVE && prev_mode == WAIT && mode != WAIT) {
		/* 목표 칸 수를 먼저 정해두면 웨이브가 끝난 뒤 그 칸 수로 돌아온다 */
		bar_set_level(level);
		bar_wave();
	} else if (level != shown_level) {
		bar_set_level(level);
	}

	shown_level = level;
	prev_mode = mode;
}

void ledbar_ctrl_close(void)
{
	if (fd < 0)
		return;
	bar_set_level(0);
	close(fd);
	fd = -1;
	shown_level = -1;
}
