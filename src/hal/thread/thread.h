/*
 *  thread.h
 *
 *  Multi-threading abstraction layer
 *
 *  Copyright 2013 Michael Zillgith
 *
 *	This file is part of libIEC61850.
 *
 *	libIEC61850 is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	libIEC61850 is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *	See COPYING file for the complete license text.
 */

#ifndef THREAD_H_
#define THREAD_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif


/*! \addtogroup hal
   *
   *  @{
   */

/**
 * @defgroup HAL_THREAD Threading and synchronization API
 *
 * @{
 */

/** Opaque reference of a Thread instance */
typedef struct sThread* Thread;

/** Qpaque reference of a Semaphore instance */
typedef void* Semaphore;

/** Reference to a function that is called when starting the thread */
typedef void* (*ThreadExecutionFunction) (void*);

/**
 * \brief Create a new Thread instance
 *
 * \param function the entry point of the thread
 * \param parameter a parameter that is passed to the threads start function
 * \param autodestroy the thread is automatically destroyed if the ThreadExecutionFunction has finished.
 *
 * \return the newly created Thread instance
 */
Thread
Thread_create(ThreadExecutionFunction function, void* parameter, bool autodestroy);

/**
 * \brief Start a Thread.
 *
 * This function invokes the start function of the thread. The thread terminates when
 * the start function returns.
 *
 * \param thread the Thread instance to start
 */
void
Thread_start(Thread thread);

/**
 * \brief Destroy a Thread and free all related resources.
 *
 * \param thread the Thread instance to destroy
 */
void
Thread_destroy(Thread thread);

/**
 * \brief Suspend execution of the Thread for the specified number of milliseconds
 */
void
Thread_sleep(int millies);

Semaphore
Semaphore_create(int initialValue);

/* Wait until semaphore value is greater than zero. Then decrease the semaphore value. */
void
Semaphore_wait(Semaphore self);

void
Semaphore_post(Semaphore self);

void
Semaphore_destroy(Semaphore self);

/*! @} */

/*! @} */

#ifdef __cplusplus
}
#endif


#endif /* THREAD_H_ */
