package com.libiec61850.tools;

/*
 *  StaticModelGenerator.java
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

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.PrintStream;
import java.util.LinkedList;
import java.util.List;

import com.libiec61850.scl.SclParser;
import com.libiec61850.scl.SclParserException;
import com.libiec61850.scl.communication.Communication;
import com.libiec61850.scl.communication.ConnectedAP;
import com.libiec61850.scl.communication.GSE;
import com.libiec61850.scl.communication.GSEAddress;
import com.libiec61850.scl.communication.SubNetwork;
import com.libiec61850.scl.model.AccessPoint;
import com.libiec61850.scl.model.DataAttribute;
import com.libiec61850.scl.model.DataModelValue;
import com.libiec61850.scl.model.DataObject;
import com.libiec61850.scl.model.DataSet;
import com.libiec61850.scl.model.FunctionalConstraintData;
import com.libiec61850.scl.model.GSEControl;
import com.libiec61850.scl.model.IED;
import com.libiec61850.scl.model.LogicalDevice;
import com.libiec61850.scl.model.LogicalNode;
import com.libiec61850.scl.model.ReportControlBlock;
import com.libiec61850.scl.model.Server;

public class StaticModelGenerator {

    private List<String> variablesList;
    private PrintStream cOut;
    private PrintStream hOut;
    
    private StringBuffer initializerBuffer;
    
    private StringBuffer reportControlBlocks;
    private List<String> rcbVariableNames;
    
    private StringBuffer gseControlBlocks;
    private List<String> gseVariableNames;
    
    
    private Communication communication;
    
    private ConnectedAP connectedAP;
    
    public StaticModelGenerator(InputStream stream, String icdFile, PrintStream cOut, PrintStream hOut) throws SclParserException {
        this.cOut = cOut;
        this.hOut = hOut;
        this.initializerBuffer = new StringBuffer();
        this.reportControlBlocks = new StringBuffer();
        this.rcbVariableNames = new LinkedList<String>();
        this.gseControlBlocks = new StringBuffer();
        this.gseVariableNames = new LinkedList<String>();
            
        SclParser sclParser = new SclParser(stream);

        IED ied = sclParser.getIed();
        
        if (ied == null) {
        	System.out.println("No data model present in SCL file! Exit.");
        }
    
        AccessPoint accessPoint = ied.getAccessPoint();
        
        communication = sclParser.getCommunication();

        if (communication != null) {
            List<SubNetwork> subNetworks = communication.getSubNetworks();
            
            for (SubNetwork subNetwork : subNetworks) {
                List<ConnectedAP> connectedAPs = subNetwork.getConnectedAPs();
                        
                for (ConnectedAP connectedAP : connectedAPs) {
                    if (connectedAP.getIedName().equals(ied.getName())) {
                    
                        if (connectedAP.getApName().equals(accessPoint.getName())) {
                            
                            System.out.println("Found connectedAP " + accessPoint.getName() + 
                                    " for IED " + ied.getName());
                            
                            this.connectedAP = connectedAP;
                            
                        }
                    }
                }
                        
            }
        }
        
        
        printCFileHeader(icdFile);
        
        printHeaderFileHeader(icdFile);

        variablesList = new LinkedList<String>();
        
        Server server = accessPoint.getServer();
        
        printForwardDeclarations(server);

        System.out.println("\n");

        printDeviceModelDefinitions(ied);
        
        printInitializerFunction();
        
        printVariablePointerDefines();
        
        printHeaderFileFooter();
    }
    
    private void printInitializerFunction() {
        cOut.println("\nstatic void\ninitializeValues()");
        cOut.println("{");
        cOut.print(this.initializerBuffer);
        cOut.println("}");
    }

    public static void main(String[] args) throws FileNotFoundException, SclParserException 
    {
        if (args.length < 1) {
            System.out.println("Usage: genmodel <ICD file>");
            System.exit(1);
        }

        String icdFile = args[0];
        
        String defaultCFileName = "static_model.c";
        String defaultHFileName = "static_model.h";
        
        PrintStream cOutStream = new PrintStream(new FileOutputStream(new File(defaultCFileName)));
        PrintStream hOutStream = new PrintStream(new FileOutputStream(new File(defaultHFileName)));

        InputStream stream = new FileInputStream(icdFile);

        new StaticModelGenerator(stream, icdFile, cOutStream, hOutStream);
    }

    private void printVariablePointerDefines() {
        hOut.println("\n\n");
        
        for (String variableName : variablesList) {
            String name = variableName.substring(8);
            hOut.println("#define IEDMODEL" + name + " (&" + variableName + ")");
        }
    }

    
    private String getLogicalDeviceName(IED ied, LogicalDevice logicalDevice) {
        String logicalDeviceName = logicalDevice.getLdName();

        if (logicalDeviceName == null)
            logicalDeviceName = ied.getName() + logicalDevice.getInst();
        
        return logicalDeviceName;
    }
    
    private void printDeviceModelDefinitions(IED ied) {

        
        
        printDataSets(ied);
           
        List<LogicalDevice> logicalDevices = ied.getAccessPoint().getServer().getLogicalDevices();
        
        
        
        for (int i = 0; i < logicalDevices.size(); i++) {

            LogicalDevice logicalDevice = logicalDevices.get(i);
            
            String ldName = "iedModel_" + logicalDevice.getInst();

            variablesList.add(ldName);
            
            String logicalDeviceName = getLogicalDeviceName(ied, logicalDevice);

            cOut.println("\nLogicalDevice " + ldName + " = {");
            cOut.println("    \"" + logicalDeviceName + "\",");
            
            if (i < (logicalDevices.size() - 1))
                cOut.println("    &iedModel_" + logicalDevices.get(i + 1).getInst() + ",");
            else
                cOut.println("    NULL,");
            
            String firstChildName = ldName + "_"
                    + logicalDevice.getLogicalNodes().get(0).getName();
            
            cOut.println("    &" + firstChildName);
            cOut.println("};\n");

            printLogicalNodeDefinitions(ldName, logicalDevice.getLogicalNodes());
        }
        
        printGSEDefinitions();
        printReportDefinitions();

        String firstLogicalDeviceName = logicalDevices.get(0).getInst();
        cOut.println("\nIedModel iedModel = {");
        cOut.println("    \"" + ied.getName() + "\"," );
        cOut.println("    &iedModel_" + firstLogicalDeviceName + "," );
        cOut.println("    datasets,");
        cOut.println("    reportControlBlocks,");
        cOut.println("    gseControlBlocks,");
        cOut.println("    initializeValues\n};");
    }

    private void printReportDefinitions() {
        reportControlBlocks.append("\n");
        reportControlBlocks.append("static ReportControlBlock* reportControlBlocks[] = {\n");
        
        for (String rcbVariableName : this.rcbVariableNames) {
            reportControlBlocks.append("    &" + rcbVariableName + ",\n");
        }
        
        reportControlBlocks.append("    NULL\n};\n");
        
        cOut.println(reportControlBlocks);
    }
    
    private void printGSEDefinitions() {
        gseControlBlocks.append("\n");
        gseControlBlocks.append("static GSEControlBlock* gseControlBlocks[] = {\n");
        
        for (String gseVariableName : this.gseVariableNames) {
            gseControlBlocks.append("    &" + gseVariableName + ",\n");
        }
        
        gseControlBlocks.append("    NULL\n};\n");
        
        cOut.println(gseControlBlocks);
    }

    private void printLogicalNodeDefinitions(String ldName,
            List<LogicalNode> logicalNodes) 
    {
        for (int i = 0; i < logicalNodes.size(); i++) {
            LogicalNode logicalNode = logicalNodes.get(i);
            
            String lnName = ldName + "_" + logicalNode.getName();
            
            variablesList.add(lnName);
            
            cOut.println("LogicalNode " + lnName + " = {");
            cOut.println("    LogicalNodeModelType,");
            cOut.println("    \"" + logicalNode.getName() + "\",");
            cOut.println("    &" + ldName + ",");
            if (i < (logicalNodes.size() - 1))
                cOut.println("    (ModelNode*) &" + ldName + "_" + logicalNodes.get(i + 1).getName() + ",");
            else
                cOut.println("    NULL,");
            
            String firstChildName = lnName + "_" + logicalNode.getDataObjects().get(0).getName();
            
            cOut.println("    (ModelNode*) &" + firstChildName + ",");
            
            cOut.println("};\n");
            
            printDataObjectDefinitions(lnName, logicalNode.getDataObjects(), null);
            
            printReportControlBlocks(lnName, logicalNode);
            
            printGSEControlBlocks(ldName, lnName, logicalNode);
        }
    }

    private void printDataObjectDefinitions(String lnName,
            List<DataObject> dataObjects, String dataAttributeSibling)
    {
        for (int i = 0; i < dataObjects.size(); i++) {
            DataObject dataObject = dataObjects.get(i);
            
            String doName = lnName + "_" + dataObject.getName();
            
            variablesList.add(doName);
            
            cOut.println("DataObject " + doName + " = {");
            cOut.println("    DataObjectModelType,");
            cOut.println("    \"" + dataObject.getName() + "\",");
            cOut.println("    (ModelNode*) &" + lnName + ",");
            
            if (i < (dataObjects.size() - 1))
                cOut.println("    (ModelNode*) &" + lnName + "_" + dataObjects.get(i + 1).getName() + ",");
            else if (dataAttributeSibling != null)
                cOut.println("    (ModelNode*) &" + dataAttributeSibling + ",");
            else
                cOut.println("    NULL,");
            
            String firstSubDataObjectName = null;
            String firstDataAttributeName = null;
            
            if ((dataObject.getSubDataObjects() != null) && (dataObject.getSubDataObjects().size() > 0))
                firstSubDataObjectName = doName + "_" + dataObject.getSubDataObjects().get(0).getName();
            
            if ((dataObject.getDataAttributes() != null) && (dataObject.getDataAttributes().size() > 0))
                firstDataAttributeName = doName + "_" + dataObject.getDataAttributes().get(0).getName();
                
            if (firstSubDataObjectName != null) {
                cOut.println("    (ModelNode*) &" + firstSubDataObjectName + ",");
            }
            else if (firstDataAttributeName != null){
                cOut.println("    (ModelNode*) &" + firstDataAttributeName + ",");
            }
            else {
                cOut.println("    NULL,");
            }
            
            cOut.println("    0,");
            cOut.println("    " + dataObject.getCount());
            
            cOut.println("};\n");
            
            if (dataObject.getSubDataObjects() != null)
                printDataObjectDefinitions(doName, dataObject.getSubDataObjects(), firstDataAttributeName);
            
            if (dataObject.getDataAttributes() != null)
                printDataAttributeDefinitions(doName, dataObject.getDataAttributes());
            
        }
    }

    private void printDataAttributeDefinitions(String doName,
            List<DataAttribute> dataAttributes) 
    {
        for (int i = 0; i < dataAttributes.size(); i++) {
            DataAttribute dataAttribute = dataAttributes.get(i);
            
            String daName = doName + "_" + dataAttribute.getName();
            
            variablesList.add(daName);
            
            cOut.println("DataAttribute " + daName + " = {");
            cOut.println("    DataAttributeModelType,");
            cOut.println("    \"" + dataAttribute.getName() + "\",");
            cOut.println("    (ModelNode*) &" + doName + ",");
            if (i < (dataAttributes.size() - 1))
                cOut.println("    (ModelNode*) &" + doName + "_" + dataAttributes.get(i + 1).getName() + ",");
            else
                cOut.println("    NULL,");
            if ((dataAttribute.getSubDataAttributes() != null) && (dataAttribute.getSubDataAttributes().size() > 0))
                cOut.println("    (ModelNode*) &" + daName + "_" + dataAttribute.getSubDataAttributes().get(0).getName() + ",");
            else
                cOut.println("    NULL,");
            
            cOut.println("    0,");
            cOut.println("    " + dataAttribute.getCount() + ",");
            cOut.println("    " + dataAttribute.getFc().toString() + ",");
            cOut.println("    " + dataAttribute.getType() + ",");
            cOut.println("    NULL");
            cOut.println("};\n");
            
            if (dataAttribute.getSubDataAttributes() != null)
                printDataAttributeDefinitions(daName, dataAttribute.getSubDataAttributes());
            
            if (dataAttribute.getValue() != null) {
                printValue(daName, dataAttribute);
            }
        }
        
    }

    private void printValue(String daName, DataAttribute dataAttribute) {
           DataModelValue value = dataAttribute.getValue();
        
           StringBuffer buffer = this.initializerBuffer;
           
           buffer.append("\n");
           buffer.append(daName);
           buffer.append(".mmsValue = ");
           
            switch (dataAttribute.getType()) {
            case ENUMERATED:
            case INT8:
            case INT16:
            case INT32:
            case INT64:
                buffer.append("MmsValue_newIntegerFromInt32(" + value.getIntValue() + ");");
                break;
            case INT8U:
            case INT16U:
            case INT24U:
            case INT32U:
                buffer.append("MmsValue_newUnsignedFromUint32(" + value.getLongValue() + ");");
                break;
            case BOOLEAN:
                buffer.append("MmsValue_newBoolean(" + value.getValue() + ");");
                break;
            case UNICODE_STRING_255:
            	buffer.append("MmsValue_newMmsString(\"" + value.getValue() + "\");");
            	break;
            case VISIBLE_STRING_32:
            case VISIBLE_STRING_64:
            case VISIBLE_STRING_129:
            case VISIBLE_STRING_255:
            case VISIBLE_STRING_65:
                buffer.append("MmsValue_newVisibleString(\"" + value.getValue() + "\");");
                break;
            case FLOAT32:
            	buffer.append("MmsValue_newFloat(" + value.getIntValue() + ");");
            	break;
            case FLOAT64:
            	buffer.append("MmsValue_newDouble(" + value.getIntValue() + ");");
            	break;
            default:
            	System.out.println("Unknown default value for "  + daName + " type: " + dataAttribute.getType());
                buffer.append("NULL;");
                break;
            }
            
           buffer.append("\n");
            
    }

    private void printForwardDeclarations(Server server) {
        
        cOut.println("extern IedModel iedModel;");
        cOut.println("static void initializeValues();");
        hOut.println("extern IedModel iedModel;");
        
        for (LogicalDevice logicalDevice : server.getLogicalDevices()) {
            String ldName = "iedModel_" + logicalDevice.getInst();

            cOut.println("extern LogicalDevice " + ldName + ";");
            hOut.println("extern LogicalDevice " + ldName + ";");

            for (LogicalNode logicalNode : logicalDevice.getLogicalNodes()) {
                String lnName = ldName + "_" + logicalNode.getName();

                cOut.println("extern LogicalNode   " + lnName + ";");
                hOut.println("extern LogicalNode   " + lnName + ";");

                printDataObjectForwardDeclarations(lnName,
                        logicalNode.getDataObjects());
            }

        }
    }

    private void printDataObjectForwardDeclarations(String prefix,
            List<DataObject> dataObjects) {
        for (DataObject dataObject : dataObjects) {
            String doName = prefix + "_" + dataObject.getName();

            cOut.println("extern DataObject    " + doName + ";");
            hOut.println("extern DataObject    " + doName + ";");

            if (dataObject.getSubDataObjects() != null) {
                printDataObjectForwardDeclarations(doName,
                        dataObject.getSubDataObjects());
            }

            printDataAttributeForwardDeclarations(doName,
                    dataObject.getDataAttributes());
        }
    }

    private void printDataAttributeForwardDeclarations(String doName,
            List<DataAttribute> dataAttributes) {
        for (DataAttribute dataAttribute : dataAttributes) {
            String daName = doName + "_" + dataAttribute.getName();

            cOut.println("extern DataAttribute " + daName + ";");
            hOut.println("extern DataAttribute " + daName + ";");

            if (dataAttribute.getSubDataAttributes() != null)
                printDataAttributeForwardDeclarations(daName,
                        dataAttribute.getSubDataAttributes());
        }
    }

    private void printCFileHeader(String filename) {

        cOut.println("/*");
        cOut.println(" * static_model.c");
        cOut.println(" *");
        cOut.println(" * automatically generated from " + filename);
        cOut.println(" */");
        cOut.println("#include <stdlib.h>");
        cOut.println("#include \"model.h\"");
        cOut.println();
    }
    
    private void printHeaderFileHeader(String filename) {
        hOut.println("/*");
        hOut.println(" * static_model.h");
        hOut.println(" *");
        hOut.println(" * automatically generated from " + filename);
        hOut.println(" */\n");
        hOut.println("#ifndef STATIC_MODEL_H_");
        hOut.println("#define STATIC_MODEL_H_\n");
        hOut.println("#include <stdlib.h>");
        hOut.println("#include \"model.h\"");
        hOut.println();
    }
    
    private void printHeaderFileFooter() {
        hOut.println();
        hOut.println("#endif /* STATIC_MODEL_H_ */\n");
    }

    private void printGSEControlBlocks(String ldName, String lnPrefix, LogicalNode logicalNode) {     
        List<GSEControl> gseControlBlocks = logicalNode.getGSEControlBlocks();
        
        String[] ldNameComponents = ldName.split("_");
        
        String logicalDeviceName = ldNameComponents[1];
        
        int gseControlNumber = 0;
        
        for (GSEControl gseControlBlock : gseControlBlocks) {
            
            GSEAddress gseAddress = lookupGSEAddress(logicalDeviceName, gseControlBlock.getName());
            
            String gseString = "";
            
            String phyComAddrName = "";
            
            if (gseAddress != null) {
                phyComAddrName = lnPrefix + "_gse" + gseControlNumber + "_address";
                
                gseString += "static PhyComAddress " + phyComAddrName + " = {\n";
                gseString += "  " + gseAddress.getVlanPriority() + ",\n";
                gseString += "  " + gseAddress.getVlanId() + ",\n";
                gseString += "  " + gseAddress.getAppId() + ",\n";
                gseString += "  {";
                
                for (int i = 0; i < 6; i++) {
                    gseString += "0x" + Integer.toHexString(gseAddress.getMacAddress()[i]);
                    if (i == 5)
                        gseString += "}\n";
                    else
                        gseString += ", ";
                }
                
                gseString += "};\n\n";
            }
            
            String gseVariableName = lnPrefix + "_gse" + gseControlNumber;
            
            gseString += "static GSEControlBlock " + gseVariableName + " = {";
            gseString += "&" + lnPrefix + ", ";
            
            gseString += "\"" + gseControlBlock.getName() + "\", ";
            
            if (gseControlBlock.getAppID() == null)
                gseString += "NULL, ";
            else
                gseString += "\"" + gseControlBlock.getAppID() + "\", ";
            
            if (gseControlBlock.getDataSet() != null)
                gseString += "\"" + gseControlBlock.getDataSet() + "\", ";
            else
                gseString += "NULL, ";
            
            gseString += gseControlBlock.getConfRev() + ", ";
            
            if (gseControlBlock.isFixedOffs())
                gseString += "true,";
            else
                gseString += "false,";
            
            if (gseAddress != null) 
                gseString += "&" + phyComAddrName;
            else
                gseString += "NULL";
            
            gseString += "};\n";
            
            this.gseControlBlocks.append(gseString);
      
            this.gseVariableNames.add(gseVariableName);
            
            gseControlNumber++;
        }
    }
    
    private GSEAddress lookupGSEAddress(String logicalDeviceName, String name) {
        
        for (GSE gse : connectedAP.getGses()) {
            if (gse.getLdInst().equals(logicalDeviceName)) {
                if (gse.getCbName().equals(name)) {
                    System.out.println("Found GSE address definition for " + name);
                    return gse.getAddress();
                }
            }
        }
        
        return null;
    }

    private void printReportControlBlocks(String lnPrefix, LogicalNode logicalNode) 
    {
        List<ReportControlBlock> reportControlBlocks = logicalNode.getReportControlBlocks();
        
        int reportNumber = 0;

        for (ReportControlBlock rcb : reportControlBlocks) {
            String rcbVariableName = lnPrefix + "_report" + reportNumber;
            
            String rcbString = "static ReportControlBlock " + rcbVariableName
                    + " = {";

            rcbString += "&" + lnPrefix + ", ";
            
            rcbString += "\"" + rcb.getName() + "\", ";
            
            if (rcb.getRptID() == null)
                rcbString += "NULL, ";
            else
                rcbString += "\"" + rcb.getRptID() + "\", ";

            if (rcb.isBuffered())
                rcbString += "true, ";
            else
                rcbString += "false, ";

            if (rcb.getDataSet() != null)
                rcbString += "\"" + rcb.getDataSet() + "\", ";
            else
                rcbString += "NULL, ";
            
            if (rcb.getConfRef() != null)
                rcbString += rcb.getConfRef().toString() + ", ";
            else
                rcbString += "0, ";
            
            int triggerOps = 0;
            
            if (rcb.getTriggerOptions() != null) {
                if (rcb.getTriggerOptions().isDchg())
                    triggerOps += 2;
                if (rcb.getTriggerOptions().isQchg())
                    triggerOps += 4;
                if (rcb.getTriggerOptions().isDupd())
                    triggerOps += 8;
                if (rcb.getTriggerOptions().isPeriod())
                    triggerOps += 16;
                if (rcb.getTriggerOptions().isGi())
                    triggerOps += 32;
            }
            else
                triggerOps = 32;
            
            rcbString += triggerOps + ", ";
            
            int options = 0;
            
            if (rcb.getOptionFields() != null) {
                if (rcb.getOptionFields().isSeqNum())
                    options += 1;
                if (rcb.getOptionFields().isTimeStamp())
                    options += 2;
                if (rcb.getOptionFields().isReasonCode())
                    options += 4;
                if (rcb.getOptionFields().isDataSet())
                    options += 8;
                if (rcb.getOptionFields().isDataRef())
                    options += 16;
                if (rcb.getOptionFields().isBufOvfl())
                    options += 32;
                if (rcb.getOptionFields().isEntryID())
                    options += 64;
                if (rcb.getOptionFields().isConfigRef())
                    options += 128;
            }
            else
                options = 32;
            
            rcbString += options + ", ";
            
            rcbString += rcb.getBufferTime() + ", ";
            
            if (rcb.getIntegrityPeriod() != null)
                rcbString += rcb.getIntegrityPeriod().toString();
            else
                rcbString += "0";
            
            rcbString += "};\n";

            this.reportControlBlocks.append(rcbString);
            
            this.rcbVariableNames.add(rcbVariableName);
            
            reportNumber++;
        }

    }
    
    private static String toMmsString(String iecString) {
        return iecString.replace('.', '$');
    }

    private void printDataSets(IED ied) {
        
        List<LogicalDevice> logicalDevices = 
                ied.getAccessPoint().getServer().getLogicalDevices();
        
        String dataSetNames = "";
        
        /* print data sets */
        for (LogicalDevice logicalDevice : logicalDevices) {
            
            for (LogicalNode logicalNode : logicalDevice.getLogicalNodes()) {
                
                List<DataSet> dataSets = logicalNode.getDataSets();
                
                for (DataSet dataSet : dataSets) {
                    
                    String dataSetVariableName = "ds_" +
                            logicalDevice.getInst() + "_" +
                            logicalNode.getName() + "_" +
                            dataSet.getName();
                    
                    int fcdaCount = 0;
                
                    String dataSetElements = "";
                    
                    for (FunctionalConstraintData fcda : dataSet.getFcda()) {
                        String dataSetEntryName = dataSetVariableName + "_fcda" + fcdaCount;
                        
                        cOut.println("static DataSetEntry " + dataSetEntryName + " = {");                        
                        cOut.println("  \"" + ied.getName() + fcda.getLdInstance() + "\",");
                        
                        String mmsVariableName = "";
                        
                        if (fcda.getPrefix() != null)
                            mmsVariableName += fcda.getPrefix();
                        
                        mmsVariableName += fcda.getLnClass();
                        
                        if (fcda.getLnInstance() != null)
                            mmsVariableName += fcda.getLnInstance();
                        
                        mmsVariableName += "$" + fcda.getFc().toString();
                        
                        mmsVariableName += "$" + toMmsString(fcda.getDoName());
                        
                        if (fcda.getDaName() != null)
                            mmsVariableName += "$" + toMmsString(fcda.getDaName());
                        
                        //TODO implement index processing!
                        
                        cOut.println("  \"" + mmsVariableName + "\",");
                        cOut.println("  -1,");
                        cOut.println("  NULL,");
                        cOut.println("  NULL");
                        cOut.println("};\n");
                        
                        fcdaCount++;
                        
                        if (fcdaCount != dataSet.getFcda().size())
                            dataSetElements += "  &" + dataSetEntryName + ",\n";
                        else
                            dataSetElements += "  &" + dataSetEntryName;
                    }
                    
                    cOut.println("static DataSetEntry* " + dataSetVariableName + "_elements[" +
                            fcdaCount + "] = {");
                    cOut.println(dataSetElements);
                    cOut.println("};\n");
                    
                    cOut.println("static DataSet " + dataSetVariableName + " = {");
                    
                    String lnVariableName = "iedModel_" + logicalDevice.getInst() +  "_" + logicalNode.getName();
                    
                    //cOut.println("  &" + lnVariableName + ",");
                    cOut.println("  \"" + ied.getName() + logicalDevice.getInst() + "\",");
                    cOut.println("  \"" + logicalNode.getName() + "$" + dataSet.getName() + "\",");
                    cOut.println("  " + dataSet.getFcda().size() + ",");
                    cOut.println("  " + dataSetVariableName + "_elements");
                    cOut.println("};");
                    
                    dataSetNames += "\n  &" + dataSetVariableName + ",";
                    
                }
            }
        }
        
        cOut.print("\nstatic DataSet* datasets[] = {");
        cOut.println(dataSetNames);
        cOut.println("  NULL\n};");
        
    }

}
