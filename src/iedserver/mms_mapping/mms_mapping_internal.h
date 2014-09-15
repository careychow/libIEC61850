/*
 *  mms_mapping_internal.h
 *
 *  Copyright 2013 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

#ifndef MMS_MAPPING_INTERNAL_H_
#define MMS_MAPPING_INTERNAL_H_

#include "thread.h"
#include "linked_list.h"

struct sMmsMapping {
    IedModel* model;
    MmsDevice* mmsDevice;
    MmsServer mmsServer;
    LinkedList reportControls;
    LinkedList gseControls;
    LinkedList controlObjects;
    LinkedList observedObjects;
    LinkedList attributeAccessHandlers;

#if (CONFIG_MMS_THREADLESS_STACK != 1)
    bool reportThreadRunning;
    bool reportThreadFinished;
    Thread reportWorkerThread;
#endif

    IedServer iedServer;

    IedConnectionIndicationHandler connectionIndicationHandler;
    void* connectionIndicationHandlerParameter;
};

#endif /* MMS_MAPPING_INTERNAL_H_ */
