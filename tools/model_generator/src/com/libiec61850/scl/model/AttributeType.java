package com.libiec61850.scl.model;

/*
 *  AttributeType.java
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

import com.libiec61850.scl.SclParserException;

public enum AttributeType {
	BOOLEAN,/* int */
	INT8,   /* int8_t */
	INT16,  /* int16_t */
	INT32,  /* int32_t */
	INT64,  /* int64_t */
	INT128,
	INT8U,  /* uint8_t */
	INT16U, /* uint16_t */
	INT24U, /* uint32_t */
	INT32U, /* uint32_t */
	FLOAT32, /* float */
	FLOAT64, /* double */
	ENUMERATED,
	OCTET_STRING_64,
	OCTET_STRING_6,
	OCTET_STRING_8,
	VISIBLE_STRING_32,
	VISIBLE_STRING_64,
	VISIBLE_STRING_65,
	VISIBLE_STRING_129,
	VISIBLE_STRING_255,
	UNICODE_STRING_255,
	TIMESTAMP,
	QUALITY,
	CHECK,
	CODEDENUM,
	GENERIC_BITSTRING,
	CONSTRUCTED,
	ENTRY_TIME,
	PHYCOMADDR;

	public static AttributeType createFromSclString(String typeString) throws SclParserException {
		if (typeString.equals("BOOLEAN"))
			return BOOLEAN;
		else if(typeString.equals("INT8"))
			return INT8;
		else if(typeString.equals("INT16"))
			return INT16;
		else if(typeString.equals("INT32"))
			return INT32;
		else if(typeString.equals("INT64"))
			return INT64;
		else if(typeString.equals("INT128"))
			return INT128;
		else if(typeString.equals("INT8U"))
			return INT8U;
		else if(typeString.equals("INT16U"))
			return INT16U;
		else if(typeString.equals("INT24U"))
			return INT24U;
		else if(typeString.equals("INT32U"))
			return INT32U;
		else if(typeString.equals("FLOAT32"))
			return FLOAT32;
		else if(typeString.equals("FLOAT64"))
			return FLOAT64;
		else if(typeString.equals("Enum"))
			return ENUMERATED;
		else if(typeString.equals("Dbpos")) //TODO check if this is always correct
		    return ENUMERATED;
		else if(typeString.equals("Check")) //TODO check if this is always correct
            return CHECK;
		else if(typeString.equals("Tcmd"))
			return ENUMERATED;
		else if(typeString.equals("Octet64"))
		    return OCTET_STRING_64;
		else if(typeString.equals("Quality"))
			return QUALITY;
		else if(typeString.equals("Timestamp"))
			return TIMESTAMP;
		else if(typeString.equals("VisString32"))
			return VISIBLE_STRING_32;
		else if(typeString.equals("VisString64"))
			return VISIBLE_STRING_64;
		else if(typeString.equals("VisString65"))
			return VISIBLE_STRING_65;
		else if(typeString.equals("VisString129"))
			return VISIBLE_STRING_129;
		else if(typeString.equals("ObjRef"))
			return VISIBLE_STRING_129;
		else if(typeString.equals("VisString255"))
			return VISIBLE_STRING_255;
		else if(typeString.equals("Unicode255"))
			return UNICODE_STRING_255;
		else if (typeString.equals("OptFlds"))
			return GENERIC_BITSTRING;
		else if (typeString.equals("TrgOps"))
			return GENERIC_BITSTRING;
		else if (typeString.equals("EntryID"))
			return OCTET_STRING_8;
		else if (typeString.equals("EntryTime"))
			return ENTRY_TIME;
		else if (typeString.equals("PhyComAddr"))
			return PHYCOMADDR;
		else if(typeString.equals("Struct"))
			return CONSTRUCTED;
		else
			throw new SclParserException("unsupported attribute type " + typeString);
		
	}
}
