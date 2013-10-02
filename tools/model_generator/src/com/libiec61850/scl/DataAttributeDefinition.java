package com.libiec61850.scl;

/*
 *  DataAttributeDefinition.java
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

import org.w3c.dom.Node;

import com.libiec61850.scl.model.AttributeType;
import com.libiec61850.scl.model.FunctionalConstraint;

public class DataAttributeDefinition {

	private String name = null;
	private String bType = null;
	private String type = null;
	private int count = 0;
	private FunctionalConstraint fc = null;
	private AttributeType attributeType = null;
	
	
	public DataAttributeDefinition(Node node) throws SclParserException {
		this.name = ParserUtils.parseAttribute(node, "name");
		this.bType = ParserUtils.parseAttribute(node, "bType");
		this.type = ParserUtils.parseAttribute(node, "type");
		String fcString = ParserUtils.parseAttribute(node, "fc");
		
		if (this.name == null)
			throw new SclParserException("attribute name is missing");
		
		if (fcString != null)
			this.fc = FunctionalConstraint.createFromString(fcString);

		if (this.bType == null)
			throw new SclParserException("attribute bType is missing");
		else {
			if (this.bType.equals("Tcmd"))
				this.type = "Tcmd";
			else if (this.bType.equals("Dbpos"))
				this.type = "Dbpos";
			else if (this.bType.equals("Check"))
				this.type = "Check";
			
			attributeType = AttributeType.createFromSclString(this.bType);
		}
		
		String countStr = ParserUtils.parseAttribute(node, "count");
		if (countStr != null)
			count = new Integer(countStr);
		
	}

	public String getName() {
		return name;
	}

	public String getbType() {
		return bType;
	}

	public String getType() {
		return type;
	}

	public FunctionalConstraint getFc() {
		return fc;
	}

	public AttributeType getAttributeType() {
		return attributeType;
	}

	public int getCount() {
		return count;
	}
	
}
