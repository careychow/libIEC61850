/*
 *  mms_type_spec.h
 *
 *  MmsTypeSpecfication objects are used to describe simple and complex MMS types.
 *  Complex types are arrays or structures of simple and complex types.
 *  They also represent MMS NamedVariables.
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

#ifndef MMS_TYPE_SPEC_H_
#define MMS_TYPE_SPEC_H_

#include "mms_common.h"
#include "mms_types.h"
#include "linked_list.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Delete MmsTypeSpecification object (recursive).
 *
 * \param self the MmsVariableSpecification object
 */
void
MmsVariableSpecification_destroy(MmsVariableSpecification* self);

MmsValue*
MmsVariableSpecification_getChildValue(MmsVariableSpecification* self, MmsValue* value, char* childId);

MmsVariableSpecification*
MmsVariableSpecification_getNamedVariableRecursive(MmsVariableSpecification* variable, char* nameId);

MmsType
MmsVariableSpecification_getType(MmsVariableSpecification* self);

char*
MmsVariableSpecification_getName(MmsVariableSpecification* self);

LinkedList /* <char*> */
MmsVariableSpecification_getStructureElements(MmsVariableSpecification* self);

/**
 * \brief returns the number of elements if the type is a complex type (structure, array) or the
 * bit size of integers, unsigned integers, floats, bit strings, visible and MMS strings and octet strings.
 *
 * \param self the MmsVariableSpecification object
 * \return the number of elements or -1 if not applicable
 */
int
MmsVariableSpecification_getSize(MmsVariableSpecification* self);

MmsVariableSpecification*
MmsVariableSpecification_getChildSpecificationByIndex(MmsVariableSpecification* self, int index);

MmsVariableSpecification*
MmsVariableSpecification_getArrayElementSpecification(MmsVariableSpecification* self);

int
MmsVariableSpecification_getExponentWidth(MmsVariableSpecification* self);

#ifdef __cplusplus
}
#endif

#endif /* MMS_TYPE_SPEC_H_ */
