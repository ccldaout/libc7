/*
 * c7timer.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <c7lldef.h>
#include <c7memory.h>
#include <c7thread.h>
#include <c7timer.h>
#include <c7app.h>


typedef struct _alarm_t {
    c7_ll_link_t __ll;
    c7_alarm_t id;
    int64_t tv_us;
    void (*on_alarm)(void *__arg);
    void *__arg;
} _alarm_t;

struct c7_timer_t_ {
    pthread_mutex_t mutex;
    c7_alarm_t nextid;
    c7_ll_base_t waits;		/* list of _alarm_t */
    c7_ll_base_t frees;
};

c7_timer_t c7_timer_init(void)
{
    c7_timer_t timer;
    if ((timer = c7_malloc(sizeof(*timer))) != NULL) {
	c7_thread_mutex_init(&timer->mutex, NULL);
	c7_ll_init(&timer->waits);
	c7_ll_init(&timer->frees);
    }
    return timer;
}

c7_alarm_t c7_timer_alarm_on(c7_timer_t timer,
			  int64_t tv_us,
			  void (*on_alarm)(void *__arg),
			  void *__arg)
{
    _alarm_t *alarm, *cur;

    c7_thread_lock(&timer->mutex);

    if (C7_LL_IS_EMPTY(&timer->frees)) {
	if ((alarm = c7_malloc(sizeof(*alarm))) == NULL) {
	    c7_thread_unlock(&timer->mutex);
	    return C7_TIMER_INV_ALARM;
	}
    } else {
	alarm = C7_LL_HEAD(&timer->frees);
	C7_LL_UNLINK(alarm);
    }
    
    if (timer->nextid == C7_TIMER_INV_ALARM)
	timer->nextid++;
    alarm->id = timer->nextid++;
    alarm->tv_us = tv_us;
    alarm->on_alarm = on_alarm;
    alarm->__arg = __arg;

    C7_LL_FOREACH(&timer->waits, cur) {
	if (alarm->tv_us < cur->tv_us)
	    break;
    }
    if (cur != NULL)
	C7_LL_PUTPREV(cur, alarm);
    else
	C7_LL_PUTTAIL(&timer->waits, alarm);

    c7_thread_unlock(&timer->mutex);

    return alarm->id;
}

void c7_timer_alarm_off(c7_timer_t timer, c7_alarm_t alarm_id)
{
    _alarm_t *alarm;
    c7_thread_lock(&timer->mutex);
    C7_LL_FOREACH(&timer->waits, alarm) {
	if (alarm->id == alarm_id)
	    break;
    }
    if (alarm) {
	C7_LL_UNLINK(alarm);
	C7_LL_PUTTAIL(&timer->frees, alarm);
    }
    c7_thread_unlock(&timer->mutex);
}

void c7_timer_call(c7_timer_t timer)
{
    _alarm_t *alarm, wake;
    int64_t tv_us = c7_time_us();

    c7_thread_lock(&timer->mutex);

    C7_LL_FOREACH(&timer->waits, alarm) {
	if (tv_us < alarm->tv_us)
	    break;
	wake = *alarm;

	C7_LL_UNLINK(alarm);
	alarm->on_alarm = NULL;
	alarm->__arg = NULL;
	C7_LL_PUTTAIL(&timer->frees, alarm);

	c7_thread_unlock(&timer->mutex);
	wake.on_alarm(wake.__arg);
	c7_thread_lock(&timer->mutex);

	tv_us = c7_time_us();
    }

    c7_thread_unlock(&timer->mutex);
}

int c7_timer_get_delay_us(c7_timer_t timer)
{
    int delay_us;

    c7_thread_lock(&timer->mutex);

    if (C7_LL_IS_EMPTY(&timer->waits))
	delay_us = -1;
    else {
	_alarm_t *alarm = C7_LL_HEAD(&timer->waits);
	int64_t now_us = c7_time_us();
	if (alarm->tv_us <= now_us)
	    delay_us = 0;
	else
	    delay_us = alarm->tv_us - now_us;
    }

    c7_thread_unlock(&timer->mutex);
    return delay_us;
}

int c7_timer_get_delay_ms(c7_timer_t timer)
{
    int delay_ms;

    c7_thread_lock(&timer->mutex);

    if (C7_LL_IS_EMPTY(&timer->waits))
	delay_ms = -1;
    else {
	_alarm_t *alarm = C7_LL_HEAD(&timer->waits);
	int64_t now_us = c7_time_us();
	if (alarm->tv_us <= now_us)
	    delay_ms = 0;
	else {
	    delay_ms = (int)((alarm->tv_us - now_us)/1000);
	    alarm->tv_us = now_us + delay_ms * 1000;
	}
    }

    c7_thread_unlock(&timer->mutex);
    return delay_ms;
}

void c7_timer_free(c7_timer_t timer)
{
    _alarm_t *alarm;
    (void)pthread_mutex_destroy(&timer->mutex);
    C7_LL_FOREACH(&timer->waits, alarm) {
	(void)memset(alarm, 0, sizeof(*alarm));
	free(alarm);
    }
    C7_LL_FOREACH(&timer->frees, alarm) {
	(void)memset(alarm, 0, sizeof(*alarm));
	free(alarm);
    }
    (void)memset(timer, 0, sizeof(*timer));
    free(timer);
}
