package com.libiec61850.scl;

/*
 *  SclParser.java
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

import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.util.Stack;

import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

import org.w3c.dom.Document;
import org.w3c.dom.Element;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;
import org.xml.sax.Locator;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;
import org.xml.sax.Attributes;

import com.libiec61850.scl.communication.Communication;
import com.libiec61850.scl.model.IED;
import com.libiec61850.scl.types.DataAttributeType;
import com.libiec61850.scl.types.DataObjectType;
import com.libiec61850.scl.types.EnumerationType;
import com.libiec61850.scl.types.LogicalNodeType;
import com.libiec61850.scl.types.TypeDeclarations;

public class SclParser {

	private IED ied;
	private Communication communication;
	private TypeDeclarations typeDeclarations;
	
	public SclParser(InputStream stream) throws SclParserException {
		Document doc = parseXmlDocument(stream);
		
		typeDeclarations = parseTypeDeclarations(doc);
		
		System.out.println("parse IED section ...");
		ied = parseIedSection(doc);
		
		System.out.println("parse communication section ...");
		communication = parseCommunicationSection(doc);
	}

	public IED getIed() {
		return ied;
	}

	public Communication getCommunication() {
        return communication;
    }
	
	
	public static Document parseXmlWithLineNumberInformation(InputStream xmlInputStream) throws IOException, SAXException {
	    final Document xmlDocument;
	    
	    SAXParser saxParser;
	    
	    try {
	        SAXParserFactory factory = SAXParserFactory.newInstance();
	        saxParser = factory.newSAXParser();
	        DocumentBuilderFactory docBuilderFactory = DocumentBuilderFactory.newInstance();
	        DocumentBuilder documentBuilder = docBuilderFactory.newDocumentBuilder();
	        xmlDocument = documentBuilder.newDocument();           
	    } catch(ParserConfigurationException e){
	        throw new RuntimeException(e);
	    }
	 
	    final Stack<Element> elementStack = new Stack<Element>();
	        
	    DefaultHandler handler = new DefaultHandler() {
	        
	    	private Locator documentLocator;
	    	
	    	private StringBuilder textNodeContent = new StringBuilder();
	 
	        @Override
	        public void setDocumentLocator(Locator locator) {
	            documentLocator = locator;
	        }
	        
	        @Override
	        public void startElement(String uri, String localName, String qName, Attributes attributes) throws SAXException {               
	            
	        	if (textNodeContent.length() > 0)
	        		createTextNode();            
	            
	            Element element = xmlDocument.createElement(qName);
	            	            
	            for(int i = 0; i < attributes.getLength(); i++) {
	                element.setAttribute(attributes.getQName(i), attributes.getValue(i));
	            }
	            	            
	            element.setUserData("START_LINE_NUMBER_ATTR", new Integer(documentLocator.getLineNumber()), null);
	            element.setUserData("START_COLUMN_NUMBER_ATTR", new Integer(documentLocator.getColumnNumber()), null);
	                       
	            elementStack.push(element);               
	        }
	        
	        @Override
	        public void endElement(String uri, String localName, String qName){
	        	
	        	if (textNodeContent.length() > 0)
	        		createTextNode();
	        	
	            Element element = elementStack.pop();
	            
	            element.setUserData("END_LINE_NUMBER_ATTR", new Integer(documentLocator.getLineNumber()), null);
	            element.setUserData("END_COLUMN_NUMBER_ATTR", new Integer(documentLocator.getColumnNumber()), null);
	       
	            
	            if (elementStack.isEmpty())
	                xmlDocument.appendChild(element);
	            else {
	                Element parentElement = elementStack.peek();
	                parentElement.appendChild(element);                   
	            }
	        }
	        
	        @Override
	        public void characters (char ch[], int start, int length) throws SAXException {
	        	textNodeContent.append(ch, start, length);
	        }
	        
	        private void createTextNode() {
                Element element = elementStack.peek();
                Node textNode = xmlDocument.createTextNode(textNodeContent.toString());
                element.appendChild(textNode);
                textNodeContent.delete(0, textNodeContent.length());    
	        }           
	    };
	    
	    
	    saxParser.parse(xmlInputStream, handler);
	    
	    return xmlDocument;
	}  

    private Document parseXmlDocument(InputStream stream)
			throws SclParserException 
	{
		Document xmlDocument = null;
		
		try {
			xmlDocument = parseXmlWithLineNumberInformation(stream);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		} catch (SAXException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		
		return xmlDocument;
	}
	
	
	private Communication parseCommunicationSection(Document doc) throws SclParserException {
	    
		
		Node comSection;
		
		try {
			comSection = getSectionNode("Communication", doc);
		} catch (SclParserException e) {
			return null;
		}
	    
	    Communication com = new Communication(comSection);
	    
	    return com;
	}
	
	private IED parseIedSection(Document doc) throws SclParserException {
		Node iedSection = getSectionNode("IED", doc);
		
		IED ied = new IED(iedSection, typeDeclarations);
		
		return ied;
	}

	
	private TypeDeclarations parseTypeDeclarations(Document doc) throws SclParserException {
		TypeDeclarations typeDeclarations = new TypeDeclarations();
		
		Node dataTypeTemplateSection = getSectionNode("DataTypeTemplates", doc);
		
		NodeList typeTemplates = dataTypeTemplateSection.getChildNodes();

		for (int i = 0; i < typeTemplates.getLength(); i++) {
			Node element = typeTemplates.item(i);

			String nodeName = element.getNodeName();

			if (nodeName.equals("LNodeType"))
				typeDeclarations.addType(new LogicalNodeType(element));
			else if (nodeName.equals("DOType"))
				typeDeclarations.addType(new DataObjectType(element));
			else if (nodeName.equals("DAType"))
				typeDeclarations.addType(new DataAttributeType(element));
			else if (nodeName.equals("EnumType"))
				typeDeclarations.addType(new EnumerationType(element));
		}

		
		return typeDeclarations;
	}

	private static Node getSectionNode(String sectionName, Document doc) throws SclParserException {
		NodeList typeTemplateSections = doc.getElementsByTagName(sectionName);
		
		if (typeTemplateSections.getLength() != 1)
			throw new SclParserException();
		
		Node typeTemplates = typeTemplateSections.item(0);
		
		return typeTemplates;
	}

	public static void main(String[] args) throws FileNotFoundException, SclParserException {
		String fileName = args[0];
		
		InputStream stream = new FileInputStream(fileName);
		
		SclParser sclParser = new SclParser(stream);
	}
	
}