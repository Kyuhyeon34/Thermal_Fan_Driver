/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * ledbar_ioctl.h - LED 바 드라이버 <-> 유저 앱 공용 헤더
 *
 * 커널 모듈(ledbar.c)과 유저 앱이 같이 include 한다.
 * 팀원(메인 앱 담당)에게 이 파일을 그대로 넘겨주면 된다.
 */
#ifndef _LEDBAR_IOCTL_H
#define _LEDBAR_IOCTL_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define LEDBAR_DEV_PATH		"/dev/ledbar"
#define LEDBAR_MAX_LEDS		8

/* 애니메이션 종류 */
#define LEDBAR_ANIM_NONE	0	/* 정지: 현재 패턴 그대로 표시 */
#define LEDBAR_ANIM_BLINK	1	/* 현재 패턴 깜빡임 (기본 500ms) */
#define LEDBAR_ANIM_WAVE	2	/* 1→8칸 채우고 다시 0칸, 한 번 돌고 원래 패턴 복귀 (기본 60ms) */
#define LEDBAR_ANIM_PROGRESS	3	/* 0칸부터 period마다 1칸씩 증가, 8칸에서 유지 (기본 375ms = 3초) */

struct ledbar_anim {
	__s32 anim;		/* LEDBAR_ANIM_* */
	__u32 period_ms;	/* 0 이면 애니메이션별 기본값 */
};

struct ledbar_state {
	__s32 level;		/* 마지막으로 설정한 레벨 0~8, 패턴을 직접 설정했으면 -1 */
	__u8  pattern;		/* 기본 표시 패턴 (bit0 = LED1 = 맨 아래) */
	__u8  shown;		/* 지금 실제로 켜져 있는 패턴 (애니메이션 반영) */
	__u8  nleds;		/* 연결된 LED 개수 */
	__u8  reserved;
	__s32 anim;		/* 현재 애니메이션 */
	__u32 period_ms;	/* 현재 애니메이션 주기 */
};

#define LEDBAR_IOC_MAGIC	'L'

/* 레벨 0~8: 아래부터 N칸 켜기 (WAVE/PROGRESS는 중단, BLINK는 유지) */
#define LEDBAR_SET_LEVEL	_IOW(LEDBAR_IOC_MAGIC, 0, __s32)
/* 비트마스크로 직접 켜기 (bit0 = LED1) */
#define LEDBAR_SET_PATTERN	_IOW(LEDBAR_IOC_MAGIC, 1, __u8)
/* 애니메이션 시작/정지 */
#define LEDBAR_SET_ANIM		_IOW(LEDBAR_IOC_MAGIC, 2, struct ledbar_anim)
/* 현재 상태 읽기 */
#define LEDBAR_GET_STATE	_IOR(LEDBAR_IOC_MAGIC, 3, struct ledbar_state)

#endif /* _LEDBAR_IOCTL_H */
