/*
 * ledbar_ctrl.h - 전역변수(fan_mode, TEMP, FAN_LEVEL)를 보고 LED 바를 갱신하는 모듈
 *
 * 메인 앱 사용법:
 *     ledbar_ctrl_init();              // 시작할 때 한 번
 *     while (running) {
 *         ... 다른 팀원 코드가 전역변수 갱신 ...
 *         ledbar_ctrl_update();        // 루프마다 호출 (변한 게 있을 때만 실제로 LED 갱신)
 *     }
 *     ledbar_ctrl_close();             // 끝낼 때
 *
 * 표시 규칙:
 *     WAIT   : 전부 끔
 *     AUTO   : 21.0°C 미만 0칸, 21.x 1칸, 22.x 2칸 ... 27.x 7칸, 28°C 이상 8칸
 *     MANUAL : FAN_LEVEL(1~8) 그대로 표시 — 단수 계산은 엔코더/모터 담당 쪽에서
 *     WAIT → AUTO/MANUAL 으로 켜질 때 웨이브 효과 한 번
 */
#ifndef LEDBAR_CTRL_H
#define LEDBAR_CTRL_H

int  ledbar_ctrl_init(void);
void ledbar_ctrl_update(void);
void ledbar_ctrl_close(void);

/* 현재 LED 바에 표시 중인 칸 수 (0~8), 디버깅용 */
int  ledbar_ctrl_level(void);

/* 온도 → 칸 수 (prev: 직전 칸 수, 처음이거나 히스테리시스 안 쓰려면 -1) */
int  ledbar_temp_to_level(float temp, int prev);

#endif /* LEDBAR_CTRL_H */
