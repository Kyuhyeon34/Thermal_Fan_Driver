#ifndef FAN_STATE_H
#define FAN_STATE_H

/*
 * Fan mode state machine. It has no hardware access, the outside world is
 * reached through struct fan_ops, so the motor (PWM) and temperature parts
 * can be plugged in later without touching this file.
 *
 *   WAIT --click--> (saved mode)     long press (2 s) in AUTO/MANUAL --> WAIT
 *   MANUAL <--click--> AUTO          the mode before the long press is saved
 */

enum fan_mode {
	WAIT = 0,
	AUTO,
	MANUAL
};

#define DUTY_MAX 100

/* global variables of the project */
extern int TEMP;		/* current temperature [deg C], written by the sensor side */
extern int DIAL_CNT;		/* knob position in steps (0..max), the MANUAL set point */
extern int PWM_DUTY;		/* output duty [%] 0..100 */
extern enum fan_mode fan_mode;

struct fan_ops {
	void (*set_pwm)(int duty);			/* motor side: apply the duty */
	void (*set_led)(int on);			/* mode LED, on in AUTO only */
	void (*on_mode)(enum fan_mode from, enum fan_mode to);
	void (*on_duty)(int duty);
};

void fan_init(const struct fan_ops *ops, int duty_step);

/* inputs */
void fan_on_rotate(int delta);		/* +1: clockwise, -1: counter clockwise */
void fan_on_click(void);
void fan_on_long_press(void);
void fan_set_temp(int temp);		/* call from the sensor side, AUTO uses it */

const char *fan_mode_name(enum fan_mode m);

#endif
