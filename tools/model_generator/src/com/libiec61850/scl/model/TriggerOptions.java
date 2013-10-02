package com.libiec61850.scl.model;

/*
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

import org.w3c.dom.Node;

import com.libiec61850.scl.ParserUtils;
import com.libiec61850.scl.SclParserException;

public class TriggerOptions {
	
	private boolean dchg = false;
	private boolean qchg = false;
	private boolean dupd = false;
	private boolean period = false;
	private boolean gi = false;
	
	public TriggerOptions(Node trgOpsNode) throws SclParserException {
		
		Boolean dchgVal = ParserUtils.parseBooleanAttribute(trgOpsNode, "dchg");
		if (dchgVal != null)
			this.dchg = dchgVal;
		
		Boolean qchgVal = ParserUtils.parseBooleanAttribute(trgOpsNode, "qchg");
		if (qchgVal != null)
			this.qchg = qchgVal;
		
		Boolean dupdVal = ParserUtils.parseBooleanAttribute(trgOpsNode, "dupd");
		if (dupdVal != null)
			this.dupd = dupdVal;
		
		Boolean periodVal = ParserUtils.parseBooleanAttribute(trgOpsNode, "period");
		if (periodVal != null)
			this.period = periodVal;
		
		Boolean giVal = ParserUtils.parseBooleanAttribute(trgOpsNode, "gi");
		if (giVal != null)
			this.gi = giVal;
	}

	public boolean isDchg() {
		return dchg;
	}

	public boolean isQchg() {
		return qchg;
	}

	public boolean isDupd() {
		return dupd;
	}

	public boolean isPeriod() {
		return period;
	}

	public boolean isGi() {
		return gi;
	}
	
}
