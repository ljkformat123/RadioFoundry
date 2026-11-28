// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// UV380 family backlight PWM timing (display_uvf.c, docs/spec/hardware-dm1701.md 9.4). Pure, so the host tests check it.
#ifndef BACKLIGHT_UVF_H
#define BACKLIGHT_UVF_H

#include <stdbool.h>
#include <stdint.h>

// The backlight pin has no timer channel. TIM1's update sets it and its CC1 match clears it, each through a one-word
// DMA write to the port's BSRR: two writes per cycle at any duty (hardware-dm1701.md 9.4).
// - 2 kHz: a rate the backlight stage follows, so the panel dims as set (the stage's tail is taken off below), with no
//   visible flicker.
// - During TX or open receive audio from a signal the pin is held high instead. With a carrier, any rate the stage
//   follows modulates the synthesizer: a tone in FM transmit and receive audio, weak-signal DMR errors, and from 5 to
//   17 kHz sidebands in the adjacent channel. A beep alone has no carrier, so the PWM stays on for it. From about
//   18 kHz up the stage never turns off, so a dimmed level is fully lit anyway, and the shortest pulse (one count at
//   40 kHz) left the panel dark on some radios (#759). A steady pin is clean on every radio.
#define UVF_BL_TIMER_HZ		72000000u
#define UVF_BL_PWM_HZ		2000u
#define UVF_BL_PERIOD		(UVF_BL_TIMER_HZ / UVF_BL_PWM_HZ)	// timer counts per cycle (36000)

// The backlight stage keeps the LEDs lit after the pin falls: about 22 us at full current, then a fall of about 34 us,
// 38 us of full light in all, at any rate (hardware-dm1701.md 9.4: RF harmonic nulls and the 3.3 V rail agree). That
// tail lit Brightness 10 % at about 18 % before the compare was shortened by it.
#define UVF_BL_TAIL_US		38u
#define UVF_BL_TAIL		(UVF_BL_TIMER_HZ / 1000000u * UVF_BL_TAIL_US)	// timer counts (2736)

// The shortest pulse the stage follows: below it the LEDs stay dark even though the pin was driven (a pulse of a count
// or two, 14..28 ns, lit nothing at all; measured on the radio, 2026-11-20). 40 us is under the 62 us a 20 % level at
// 2 kHz gives, which did light the panel
#define UVF_BL_MIN_US		40u
#define UVF_BL_MIN		(UVF_BL_TIMER_HZ / 1000000u * UVF_BL_MIN_US)	// timer counts (2880)

// CC1 value for a brightness: the count at which the pin clears, so that the pulse plus the tail is the brightness's
// share of the period. A level whose pulse would be shorter than UVF_BL_MIN keeps that pulse instead: lit stays lit,
// brighter than the level asked for, rather than a dark screen (local change, BG5FLU). quiet (transmitting, or receive
// audio open): any lit level is fully on. 0 = off, UVF_BL_PERIOD = fully on (the caller drives the pin steady for both)
static inline uint16_t uvf_bl_compare(uint8_t percent, bool quiet)
{
	if (percent == 0) {
		return 0;
	}
	if (percent >= 100 || quiet) {
		return UVF_BL_PERIOD;
	}
	uint32_t lit = (uint32_t)percent * UVF_BL_PERIOD / 100u;
	if (lit <= UVF_BL_TAIL + UVF_BL_MIN) {
		return (uint16_t)UVF_BL_MIN;
	}
	return (uint16_t)(lit - UVF_BL_TAIL);
}

#endif
