/*
 * c7timer.h
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#ifndef __C7_TIMER_H_LOADED__
#define __C7_TIMER_H_LOADED__
#if defined(__cplusplus)
extern "C" {
#endif
#include <c7config.h>


#include <c7types.h>


#define C7_TIMER_INV_ALARM	0

typedef struct c7_timer_t_ *c7_timer_t;
typedef uint64_t c7_alarm_t;

c7_timer_t c7_timer_init(void);
c7_alarm_t c7_timer_alarm_on(c7_timer_t timer,
			  int64_t tv_us,
			  void (*on_alarm)(void *__arg),
			  void *__arg);
void c7_timer_alarm_off(c7_timer_t timer, c7_alarm_t alarm);
void c7_timer_call(c7_timer_t timer);
int c7_timer_get_delay_us(c7_timer_t timer);
int c7_timer_get_delay_ms(c7_timer_t timer);
void c7_timer_free(c7_timer_t timer);


#if defined(__cplusplus)
}
#endif
#endif /* c7timer.h */
