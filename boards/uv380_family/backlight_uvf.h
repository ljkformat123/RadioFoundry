// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// UV380 family backlight PWM timing (display_uvf.c, docs/spec/hardware-dm1701.md 9.4). Pure, so the host tests check it.
#ifndef BACKLIGHT_UVF_H
#define BACKLIGHT_UVF_H

#include <stdbool.h>
#include <stdint.h>

// The backlight pin has no timer channel. TIM1's update sets it and its CC1 match clears it, each through a one-word
// DMA write to the port's BSRR: two writes per cycle at any duty. Two rates (hardware-dm1701.md 9.4):
// - During TX or open receive audio, 40 kHz keeps the backlight ripple out of the signal. At
//   101 Hz it was an audible buzz in FM transmit, at 10 kHz sidebands about 33 dB down in the adjacent channel (#330).
// - Otherwise 1 kHz: the backlight driver is slow (it stays lit 38 us after the pin falls), so at 40 kHz it barely
//   dimmed, and at 2 kHz the low levels still came out as very short pulses the stage does not follow: Brightness 10 %
//   and the idle dim step looked black, only the steady levels lit the panel. 1200 Hz is the lowest rate the 16-bit
//   timer can count (60000 of its 65535), so a 10 % level is a 54 us pulse, long enough for the stage, and the ripple
//   is far above visible flicker.
#define UVF_BL_TIMER_HZ		72000000u
#define UVF_BL_PWM_HZ		40000u		// during TX or open receive audio
#define UVF_BL_PERIOD		(UVF_BL_TIMER_HZ / UVF_BL_PWM_HZ)	// timer counts per cycle (1800)
#define UVF_BL_IDLE_HZ		1200u		// otherwise (the 16-bit timer caps the period at 60000 counts here)
#define UVF_BL_IDLE_PERIOD	(UVF_BL_TIMER_HZ / UVF_BL_IDLE_HZ)	// (60000)

// The backlight stage keeps the LEDs lit after the pin falls: about 22 us at full current, then a fall of about 34 us,
// 38 us of full light in all, at any rate (hardware-dm1701.md 9.4: RF harmonic nulls and the 3.3 V rail agree). That
// tail is why 40 kHz can't dim (the period is 25 us). The compare is shortened by it, so the pulse plus the tail is the
// brightness asked for.
#define UVF_BL_TAIL_US		38u
#define UVF_BL_TAIL		(UVF_BL_TIMER_HZ / 1000000u * UVF_BL_TAIL_US)	// timer counts (2736)

// The shortest pulse the stage follows: below it the LEDs stay dark even though the pin was driven (a pulse of a count
// or two, 14..28 ns, lit nothing at all, so every low level and the idle dim step looked like a dark screen). 40 us is
// under the 62 us a 20 % level at 2 kHz gave, which did light the panel.
#define UVF_BL_MIN_US		40u
#define UVF_BL_MIN		(UVF_BL_TIMER_HZ / 1000000u * UVF_BL_MIN_US)	// timer counts (2880)

static inline uint16_t uvf_bl_period(bool tx, bool rx_audio)
{
	return tx || rx_audio ? UVF_BL_PERIOD : UVF_BL_IDLE_PERIOD;
}

// CC1 value for a brightness at a period (UVF_BL_PERIOD or UVF_BL_IDLE_PERIOD): the count at which the pin clears, so
// that the pulse plus the tail is the brightness's share of the period, and never shorter than UVF_BL_MIN. 0 = off,
// period = fully on (the caller drives the pin steady for both).
//
// At the fast rate the tail is longer than the period, so no level below full fits it: dimming can't be expressed
// there at all, and a lit level drives the pin steady instead. That is the #759 fix: the panel went dark in DMR
// receive below Brightness 100 % because every level collapsed to a pulse the stage ignores.
static inline uint16_t uvf_bl_compare(uint8_t percent, uint16_t period)
{
	if (percent == 0) {
		return 0;
	}
	if (percent >= 100) {
		return period;
	}
	if (period <= UVF_BL_TAIL + UVF_BL_MIN) {
		return period;	// the cycle is too short to hold a pulse the stage follows: keep it lit
	}
	uint32_t lit = (uint32_t)percent * period / 100u;
	if (lit <= UVF_BL_TAIL + UVF_BL_MIN) {
		return (uint16_t)UVF_BL_MIN;	// the darkest level the stage holds
	}
	return (uint16_t)(lit - UVF_BL_TAIL);
}

#endif
