#ifndef EC11_IOCTL_H
#define EC11_IOCTL_H

#include <linux/ioctl.h>

/* event types delivered by read() */
#define EC11_EV_ROTATE		1	/* delta: +1 (CW) or -1 (CCW) */
#define EC11_EV_CLICK		2	/* short click, reported on release */
#define EC11_EV_LONG_PRESS	3	/* held for long_press_ms, reported while held */
#define EC11_EV_STATE		4	/* S1/S2/KEY level changed, see state */

/* bits of ec11_event.state (KEY: 1 = pressed, S1/S2: pin level) */
#define EC11_STATE_S1		(1 << 0)
#define EC11_STATE_S2		(1 << 1)
#define EC11_STATE_KEY		(1 << 2)

struct ec11_event {
	int type;
	int delta;
	int count;	/* accumulated count at the time of the event */
	int state;	/* S1/S2/KEY state at the time of the event */
};

#define EC11_IOCTL_MAGIC 'e'

#define EC11_IOCTL_GET_COUNT	_IOR(EC11_IOCTL_MAGIC, 1, int)
#define EC11_IOCTL_RESET_COUNT	_IO(EC11_IOCTL_MAGIC, 2)

#endif
