/*
 * c7init.c
 *
 * Copyright (c) 2019 ccldaout@gmail.com
 *
 * This software is released under the MIT License.
 * http://opensource.org/licenses/mit-license.php
 */
#include "_config.h"

#include "_private.h"
#include <pthread.h>

int C7_UNUSED_INT;

__attribute__((constructor))
void __c7_init(void)
{
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    static c7_bool_t __initialized;

    (void)pthread_mutex_lock(&lock);
    c7_bool_t initialized = __initialized;
    __initialized = C7_TRUE;
    (void)pthread_mutex_unlock(&lock);

    if (!initialized) {
	__c7_app_init();
	__c7_status_init();
	__c7_dconf_init();
	__c7_memory_init();
	__c7_coroutine_init();
	__c7_proc_init();
	__c7_signal_init();
    }
}
