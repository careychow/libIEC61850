/*
 *  cdc.c
 *
 *  Helper functions for the dynamic creation of Common Data Classes (CDCs)
 *
 *  Copyright 2014 Michael Zillgith
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

#include "dynamic_model.h"
#include "cdc.h"

/************************************************
 * Constructed Attribute Classes
 ***********************************************/

DataAttribute*
CAC_AnalogueValue_create(const char* name, ModelNode* parent, FunctionalConstraint fc, uint8_t triggerOptions,
        bool isIntegerNotFloat)
{
    DataAttribute* analogeValue = DataAttribute_create(name, parent, CONSTRUCTED, fc, triggerOptions, 0, 0);

    if (isIntegerNotFloat)
        DataAttribute_create("i", (ModelNode*) analogeValue, INT32, fc, triggerOptions, 0, 0);
    else
        DataAttribute_create("f", (ModelNode*) analogeValue, FLOAT32, fc, triggerOptions, 0, 0);

    return analogeValue;
}

DataAttribute*
CAC_ValWithTrans_create(const char* name, ModelNode* parent, FunctionalConstraint fc, uint8_t triggerOptions, bool hasTransientIndicator)
{
    DataAttribute* valWithTrans = DataAttribute_create(name, parent, CONSTRUCTED, fc, triggerOptions, 0, 0);

    DataAttribute_create("posVal", (ModelNode*) valWithTrans, INT8, fc, triggerOptions, 0, 0);

    if (hasTransientIndicator)
        DataAttribute_create("transInd", (ModelNode*) valWithTrans, BOOLEAN, fc, triggerOptions, 0, 0);

    return valWithTrans;
}

/**
 * CDC_OPTION_AC_CLC_O
 */
DataAttribute*
CAC_Vector_create(const char* name, ModelNode* parent, uint32_t options, FunctionalConstraint fc, uint8_t triggerOptions)
{
    DataAttribute* vector = DataAttribute_create(name, parent, CONSTRUCTED, fc, triggerOptions, 0, 0);

    CAC_AnalogueValue_create("mag", (ModelNode*) vector, fc, triggerOptions, false);

    if (options & CDC_OPTION_AC_CLC_O)
        CAC_AnalogueValue_create("ang", (ModelNode*) vector, fc, triggerOptions, false);

    return vector;
}

DataAttribute*
CAC_Point_create(const char* name, ModelNode* parent, FunctionalConstraint fc, uint8_t triggerOptions, bool hasZVal)
{
    DataAttribute* point = DataAttribute_create(name, parent, CONSTRUCTED, fc, triggerOptions, 0, 0);

    DataAttribute_create("xVal", (ModelNode*) point, FLOAT32, fc, triggerOptions, 0, 0);
    DataAttribute_create("yVal", (ModelNode*) point, FLOAT32, fc, triggerOptions, 0, 0);

    if (hasZVal)
        DataAttribute_create("zVal", (ModelNode*) point, FLOAT32, fc, triggerOptions, 0, 0);

    return point;
}

DataAttribute*
CAC_ScaledValueConfig_create(const char* name, ModelNode* parent)
{
    DataAttribute* scaling = DataAttribute_create(name, parent, CONSTRUCTED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    DataAttribute_create("scaleFactor", (ModelNode*) scaling, FLOAT32, CF, TRG_OPT_DATA_CHANGED, 0, 0);
    DataAttribute_create("offset", (ModelNode*) scaling, FLOAT32, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    return scaling;
}

DataAttribute*
CAC_Unit_create(const char* name, ModelNode* parent, bool hasMagnitude)
{
    DataAttribute* unit = DataAttribute_create(name, parent, CONSTRUCTED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    DataAttribute_create("SIUnit", (ModelNode*) unit, ENUMERATED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    if (hasMagnitude)
        DataAttribute_create("multiplier", (ModelNode*) unit, ENUMERATED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    return unit;
}

/************************************************
 *  Control parameters
 ************************************************/

static void
addOriginator(char* name, ModelNode* parent, FunctionalConstraint fc)
{
    DataAttribute* origin = DataAttribute_create(name, parent, CONSTRUCTED, fc, 0 ,0 ,0);

    DataAttribute_create("orCat", (ModelNode*) origin, ENUMERATED, fc, 0, 0, 0);
    DataAttribute_create("orIdent", (ModelNode*) origin, OCTET_STRING_64, fc, 0, 0, 0);

}

static void
addGenericOperateElements(DataAttribute* oper, DataAttributeType type, bool isTimeActivated, bool hasCheck)
{
    DataAttribute_create("ctlVal", (ModelNode*) oper, type, CO, 0, 0, 0);

    if (isTimeActivated)
        DataAttribute_create("operTm", (ModelNode*) oper, TIMESTAMP, CO, 0, 0, 0);

    addOriginator("origin", (ModelNode*) oper, CO);

    DataAttribute_create("ctlNum", (ModelNode*) oper, INT8U, CO, 0, 0, 0);
    DataAttribute_create("T", (ModelNode*) oper, TIMESTAMP, CO, 0, 0, 0);
    DataAttribute_create("Test", (ModelNode*) oper, BOOLEAN, CO, 0, 0, 0);

    if (hasCheck)
        DataAttribute_create("Check", (ModelNode*) oper, CHECK, CO, 0, 0, 0);
}

static void
addCommonOperateElements(DataAttribute* oper, bool isTimeActivated, bool hasCheck)
{
    if (isTimeActivated)
        DataAttribute_create("operTm", (ModelNode*) oper, TIMESTAMP, CO, 0, 0, 0);

    addOriginator("origin", (ModelNode*) oper, CO);

    DataAttribute_create("ctlNum", (ModelNode*) oper, INT8U, CO, 0, 0, 0);
    DataAttribute_create("T", (ModelNode*) oper, TIMESTAMP, CO, 0, 0, 0);
    DataAttribute_create("Test", (ModelNode*) oper, BOOLEAN, CO, 0, 0, 0);

    if (hasCheck)
        DataAttribute_create("Check", (ModelNode*) oper, CHECK, CO, 0, 0, 0);
}

static DataAttribute*
CDA_Oper(ModelNode* parent, DataAttributeType type, bool isTImeActivated)
{
    DataAttribute* oper =  DataAttribute_create("Oper", parent, CONSTRUCTED, CO, 0, 0, 0);

    addGenericOperateElements(oper, type, isTImeActivated, true);

    return oper;
}

static DataAttribute*
CDA_SBOw(ModelNode* parent, DataAttributeType type, bool isTImeActivated)
{
    DataAttribute* oper =  DataAttribute_create("SBOw", parent, CONSTRUCTED, CO, 0, 0, 0);

    addGenericOperateElements(oper, type, isTImeActivated, true);

    return oper;
}

static DataAttribute*
CDA_Cancel(ModelNode* parent, DataAttributeType type, bool isTImeActivated)
{
    DataAttribute* oper =  DataAttribute_create("Cancel", parent, CONSTRUCTED, CO, 0, 0, 0);

    addGenericOperateElements(oper, type, isTImeActivated, false);

    return oper;
}



/************************************************
 * Common Data Classes - helper functions
 ***********************************************/

static void
CDC_addTimeQuality(DataObject* dataObject, FunctionalConstraint fc)
{
    DataAttribute_create("q", (ModelNode*) dataObject, QUALITY, fc, TRG_OPT_QUALITY_CHANGED, 0, 0);
    DataAttribute_create("t", (ModelNode*) dataObject, TIMESTAMP, fc, 0, 0, 0);
}

static void
CDC_addStatusToDataObject(DataObject* dataObject, DataAttributeType statusType)
{
    DataAttribute_create("stVal", (ModelNode*) dataObject, statusType, ST, TRG_OPT_DATA_CHANGED | TRG_OPT_DATA_UPDATE, 0, 0);
    CDC_addTimeQuality(dataObject, ST);
}

static void
CDC_addOptionPicsSubst(DataObject* dataObject, DataAttributeType type)
{
    DataAttribute_create("subEna", (ModelNode*) dataObject, BOOLEAN, SV, 0, 0, 0);
    DataAttribute_create("subVal", (ModelNode*) dataObject, type, SV, 0, 0, 0);
    DataAttribute_create("subQ", (ModelNode*) dataObject, QUALITY, SV, 0, 0, 0);
    DataAttribute_create("subID", (ModelNode*) dataObject, VISIBLE_STRING_64, SV, 0, 0, 0);
}

static void
CDC_addOptionPicsSubstValWithTrans(DataObject* dataObject, bool hasTransientIndicator)
{
    DataAttribute_create("subEna", (ModelNode*) dataObject, BOOLEAN, SV, 0, 0, 0);

    CAC_ValWithTrans_create("subVal", (ModelNode*) dataObject, SV, 0, hasTransientIndicator);

    DataAttribute_create("subQ", (ModelNode*) dataObject, QUALITY, SV, 0, 0, 0);
    DataAttribute_create("subID", (ModelNode*) dataObject, VISIBLE_STRING_64, SV, 0, 0, 0);
}

/* Add optional attributes for extension (name spaces) and textual descriptions */
static void
CDC_addStandardOptions(DataObject* dataObject, uint32_t options)
{
    /* Standard options ? */
    if (options & CDC_OPTION_DESC)
        DataAttribute_create("d",(ModelNode*)  dataObject, VISIBLE_STRING_255, DC, 0, 0, 0);

    if (options & CDC_OPTION_DESC_UNICODE)
        DataAttribute_create("dU", (ModelNode*) dataObject, UNICODE_STRING_255, DC, 0, 0, 0);

    if (options & CDC_OPTION_AC_DLNDA) {
        DataAttribute_create("cdcNs", (ModelNode*) dataObject, VISIBLE_STRING_255, EX, 0, 0, 0);
        DataAttribute_create("cdcName", (ModelNode*) dataObject, VISIBLE_STRING_255, EX, 0, 0, 0);
    }

    if (options & CDC_OPTION_AC_DLN)
        DataAttribute_create("dataNs", (ModelNode*) dataObject, VISIBLE_STRING_255, EX, 0, 0, 0);
}

/************************************************
 * Common Data Classes - constructors
 ***********************************************/

DataObject*
CDC_SPS_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newSPS = DataObject_create(dataObjectName, parent, 0);

    CDC_addStatusToDataObject(newSPS, BOOLEAN);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubst(newSPS, BOOLEAN);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newSPS, BOOLEAN, BL, 0, 0, 0);

    CDC_addStandardOptions(newSPS, options);

    return newSPS;
}

DataObject*
CDC_DPS_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newDPS = DataObject_create(dataObjectName, parent, 0);

    CDC_addStatusToDataObject(newDPS, CODEDENUM);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubst(newDPS, CODEDENUM);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newDPS, BOOLEAN, BL, 0, 0, 0);

    CDC_addStandardOptions(newDPS, options);

    return newDPS;
}

DataObject*
CDC_INS_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newINS = DataObject_create(dataObjectName, parent, 0);

    CDC_addStatusToDataObject(newINS, INT32);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubst(newINS, INT32);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newINS, BOOLEAN, BL, 0, 0, 0);

    CDC_addStandardOptions(newINS, options);

    return newINS;
}


DataObject*
CDC_ENS_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newENS = DataObject_create(dataObjectName, parent, 0);

    CDC_addStatusToDataObject(newENS, ENUMERATED);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubst(newENS, ENUMERATED);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newENS, BOOLEAN, BL, 0, 0, 0);

    CDC_addStandardOptions(newENS, options);

    return newENS;
}

DataObject*
CDC_BCR_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newBCR = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("actVal", (ModelNode*) newBCR, INT64, ST, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_FROZEN_VALUE) {
        DataAttribute_create("frVal", (ModelNode*) newBCR, INT64, ST, TRG_OPT_DATA_UPDATE, 0, 0);
        DataAttribute_create("frTm", (ModelNode*) newBCR, TIMESTAMP, ST, 0, 0, 0);
    }

    CDC_addTimeQuality(newBCR, ST);

    if (options & CDC_OPTION_UNIT)
        DataAttribute_create("units", (ModelNode*) newBCR, ENUMERATED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    DataAttribute_create("pulsQty", (ModelNode*) newBCR, FLOAT32, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_FROZEN_VALUE) {
        DataAttribute_create("frEna", (ModelNode*) newBCR, BOOLEAN, CF, TRG_OPT_DATA_CHANGED, 0, 0);
        DataAttribute_create("strTm", (ModelNode*) newBCR, TIMESTAMP, CF, TRG_OPT_DATA_CHANGED, 0, 0);
        DataAttribute_create("frPd", (ModelNode*) newBCR, INT32, CF, TRG_OPT_DATA_CHANGED, 0, 0);
        DataAttribute_create("frRs", (ModelNode*) newBCR, BOOLEAN, CF, TRG_OPT_DATA_CHANGED, 0, 0);
    }

    CDC_addStandardOptions(newBCR, options);

    return newBCR;
}

DataObject*
CDC_SEC_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newSEC = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("cnt", (ModelNode*) newSEC, INT32U, ST, TRG_OPT_DATA_CHANGED, 0, 0);
    DataAttribute_create("sev", (ModelNode*) newSEC, ENUMERATED, ST, 0, 0, 0);
    DataAttribute_create("t", (ModelNode*) newSEC, TIMESTAMP, ST, 0, 0, 0);

    if (options & CDC_OPTION_ADDR)
        DataAttribute_create("addr", (ModelNode*) newSEC, OCTET_STRING_64, ST, 0, 0, 0);

    if (options & CDC_OPTION_ADDINFO)
        DataAttribute_create("addInfo", (ModelNode*) newSEC, VISIBLE_STRING_64, ST, 0, 0, 0);

    CDC_addStandardOptions(newSEC, options);

    return newSEC;
}




/**
 * CDC_OPTION_INST_MAG
 * CDC_OPTION_RANGE
 * CDC_OPTION_PICS_SUBST
 */
DataObject*
CDC_MV_create(const char* dataObjectName, ModelNode* parent, uint32_t options, bool isIntegerNotFloat)
{
    DataObject* newMV = DataObject_create(dataObjectName, parent, 0);

    if (options & CDC_OPTION_INST_MAG)
        CAC_AnalogueValue_create("instMag", (ModelNode*) newMV, MX, 0, isIntegerNotFloat);

    CAC_AnalogueValue_create("mag", (ModelNode*) newMV, MX, TRG_OPT_DATA_CHANGED | TRG_OPT_DATA_UPDATE, isIntegerNotFloat);

    if (options & CDC_OPTION_RANGE)
        DataAttribute_create("range", (ModelNode*) newMV, ENUMERATED, MX, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addTimeQuality(newMV, MX);

//    if (options & CDC_OPTION_PICS_SUBST)
//        CDC_addOptionPicsSubst(newMV, )

    CDC_addStandardOptions(newMV, options);

    return newMV;
}

/**
 * CDC_OPTION_INST_MAG
 * CDC_OPTION_RANGE
 */
DataObject*
CDC_CMV_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newMV = DataObject_create(dataObjectName, parent, 0);

    if (options & CDC_OPTION_INST_MAG)
        CAC_Vector_create("instCVal", (ModelNode*) newMV, options, MX, 0);

    CAC_Vector_create("cVal", (ModelNode*) newMV, options, MX, TRG_OPT_DATA_CHANGED | TRG_OPT_DATA_UPDATE);

    if (options & CDC_OPTION_RANGE)
        DataAttribute_create("range", (ModelNode*) newMV, ENUMERATED, MX, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_RANGE_ANG)
           DataAttribute_create("rangeAng", (ModelNode*) newMV, ENUMERATED, MX, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addTimeQuality(newMV, MX);

//    if (options & CDC_OPTION_PICS_SUBST)
//        CDC_addOptionPicsSubst(newMV, )

    CDC_addStandardOptions(newMV, options);

    return newMV;
}


/**
 * CDC_OPTION_UNIT
 * CDC_OPTION_AC_SCAV
 * CDC_OPTION_MIN
 * CDC_OPTION_MAX
 */
DataObject*
CDC_SAV_create(const char* dataObjectName, ModelNode* parent, uint32_t options, bool isIntegerNotFloat)
{
    DataObject* newSAV = DataObject_create(dataObjectName, parent, 0);

    CAC_AnalogueValue_create("instMag", (ModelNode*) newSAV, MX, 0, isIntegerNotFloat);

    CDC_addTimeQuality(newSAV, MX);

    if (options & CDC_OPTION_UNIT)
        CAC_Unit_create("units", (ModelNode*) newSAV, options & CDC_OPTION_UNIT_MULTIPLIER);

    if (options & CDC_OPTION_AC_SCAV)
        CAC_ScaledValueConfig_create("sVC", (ModelNode*) newSAV);

    if (options & CDC_OPTION_MIN)
        CAC_AnalogueValue_create("min", (ModelNode*) newSAV, CF, TRG_OPT_DATA_CHANGED, isIntegerNotFloat);

    if (options & CDC_OPTION_MAX)
        CAC_AnalogueValue_create("max", (ModelNode*) newSAV, CF, TRG_OPT_DATA_CHANGED, isIntegerNotFloat);

    CDC_addStandardOptions(newSAV, options);

    return newSAV;
}

DataObject*
CDC_HST_create(const char* dataObjectName, ModelNode* parent, uint32_t options, uint16_t maxPts)
{
    DataObject* newHST = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("hstVal", (ModelNode*) newHST, INT32, ST, TRG_OPT_DATA_CHANGED | TRG_OPT_DATA_UPDATE, maxPts, 0);

    CDC_addTimeQuality(newHST, ST);

    DataAttribute_create("numPts", (ModelNode*) newHST, INT16U, CF, 0, 0, 0);

    //TODO add mandatory attribute "hstRangeC"

    CAC_Unit_create("units", (ModelNode*) newHST, options & CDC_OPTION_UNIT_MULTIPLIER);

    DataAttribute_create("maxPts", (ModelNode*) newHST, INT16U, CF, 0, 0, 0);

    CDC_addStandardOptions(newHST, options);

    return newHST;
}


static void
addControls(DataObject* parent, DataAttributeType type, uint32_t controlOptions)
{
    DataAttribute* ctlModel =
            DataAttribute_create("ctlModel", (ModelNode*) parent, ENUMERATED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    int controlModel = controlOptions & 0x07;

    ctlModel->mmsValue = MmsValue_newIntegerFromInt16(controlModel);

    if (controlModel > 0) {

        if (controlModel == CDC_CTL_MODEL_SBO_NORMAL)
            DataAttribute_create("SBO", (ModelNode*) parent, VISIBLE_STRING_129, CO, 0, 0, 0);

        bool isTimeActivated = false;

        if (controlOptions & CDC_CTL_MODEL_IS_TIME_ACTIVATED)
            isTimeActivated = true;

        if (controlModel == CDC_CTL_MODEL_SBO_ENHANCED)
            CDA_SBOw((ModelNode*) parent, type, isTimeActivated);

        CDA_Oper((ModelNode*) parent, type, isTimeActivated);

        if (controlOptions & CDC_CTL_MODEL_HAS_CANCEL)
            CDA_Cancel((ModelNode*) parent, type, isTimeActivated);

    }
}

static void
addOriginatorAndCtlNumOptions(ModelNode* parent, uint32_t controlOptions)
{
    if (controlOptions & CDC_CTL_OPTION_ORIGIN)
        addOriginator("origin", parent, ST);

    if (controlOptions & CDC_CTL_OPTION_CTL_NUM)
        DataAttribute_create("ctlNum", parent, INT8U, ST, 0, 0, 0);
}

/**
 *
 * CDC_OPTION_IS_TIME_ACTICATED
 *
 * substitution options
 * CDC_OPTION_BLK_ENA
 * standard description and namespace options
 *
 */
DataObject*
CDC_SPC_create(const char* dataObjectName, ModelNode* parent, uint32_t options, uint32_t controlOptions)
{
    DataObject* newSPC = DataObject_create(dataObjectName, parent, 0);

    addOriginatorAndCtlNumOptions((ModelNode*) newSPC, controlOptions);

    CDC_addStatusToDataObject(newSPC, BOOLEAN);

    addControls(newSPC, BOOLEAN, controlOptions);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubst(newSPC, BOOLEAN);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newSPC, BOOLEAN, BL, 0, 0, 0);

    CDC_addStandardOptions(newSPC, options);

    return newSPC;
}

/**
 *
 * CDC_OPTION_IS_TIME_ACTICATED
 *
 * substitution options
 * CDC_OPTION_BLK_ENA
 * standard description and namespace options
 *
 */
DataObject*
CDC_DPC_create(const char* dataObjectName, ModelNode* parent, uint32_t options, uint32_t controlOptions)
{
    DataObject* newDPC = DataObject_create(dataObjectName, parent, 0);

    addOriginatorAndCtlNumOptions((ModelNode*) newDPC, controlOptions);

    CDC_addStatusToDataObject(newDPC, CODEDENUM);

    addControls(newDPC, BOOLEAN, controlOptions);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubst(newDPC, CODEDENUM);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newDPC, BOOLEAN, BL, 0, 0, 0);

    CDC_addStandardOptions(newDPC, options);

    return newDPC;
}

static void
addAnalogControls(DataObject* parent, uint32_t controlOptions, bool isIntegerNotFloat)
{
    DataAttribute* ctlModel =
            DataAttribute_create("ctlModel", (ModelNode*) parent, ENUMERATED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    int controlModel = controlOptions & 0x07;

    ctlModel->mmsValue = MmsValue_newIntegerFromInt16(controlModel);

    if (controlModel != CDC_CTL_MODEL_NONE) {

        if (controlModel == CDC_CTL_MODEL_SBO_NORMAL)
            DataAttribute_create("SBO", (ModelNode*) parent, VISIBLE_STRING_129, CO, 0, 0, 0);

        bool isTimeActivated = false;

        if (controlOptions & CDC_CTL_MODEL_IS_TIME_ACTIVATED)
            isTimeActivated = true;

        if (controlModel == CDC_CTL_MODEL_SBO_ENHANCED) {
            DataAttribute* sBOw =  DataAttribute_create("SBOw", (ModelNode*) parent, CONSTRUCTED, CO, 0, 0, 0);

            CAC_AnalogueValue_create("ctlVal", (ModelNode*) sBOw, CO, 0, isIntegerNotFloat);

            addCommonOperateElements(sBOw, isTimeActivated, true);
        }

        DataAttribute* oper =  DataAttribute_create("Oper", (ModelNode*) parent, CONSTRUCTED, CO, 0, 0, 0);

        CAC_AnalogueValue_create("ctlVal", (ModelNode*) oper, CO, 0, isIntegerNotFloat);

        addCommonOperateElements(oper, isTimeActivated, true);

        if (controlOptions & CDC_CTL_MODEL_HAS_CANCEL) {
            DataAttribute* cancel =  DataAttribute_create("SBOw", (ModelNode*) parent, CONSTRUCTED, CO, 0, 0, 0);

            CAC_AnalogueValue_create("ctlVal", (ModelNode*) cancel, CO, 0, isIntegerNotFloat);

            addCommonOperateElements(cancel, isTimeActivated, true);
        }

    }
}

DataObject*
CDC_APC_create(const char* dataObjectName, ModelNode* parent, uint32_t options, uint32_t controlOptions, bool isIntegerNotFloat)
{
    DataObject* newAPC = DataObject_create(dataObjectName, parent, 0);

    if (controlOptions & CDC_CTL_OPTION_ORIGIN)
        addOriginator("origin", (ModelNode*) newAPC, MX);

    if (controlOptions & CDC_CTL_OPTION_CTL_NUM)
        DataAttribute_create("ctlNum", (ModelNode*) newAPC, INT8U, MX, 0, 0, 0);

    CAC_AnalogueValue_create("mxVal", (ModelNode*) newAPC, MX, TRG_OPT_DATA_CHANGED, isIntegerNotFloat);

    CDC_addTimeQuality(newAPC, MX);

    if (controlOptions & CDC_CTL_OPTION_ST_SELD)
        DataAttribute_create("stSeld", (ModelNode*) newAPC, BOOLEAN, MX, TRG_OPT_DATA_CHANGED, 0, 0);

    if (controlOptions & CDC_CTL_OPTION_OP_RCVD)
        DataAttribute_create("opRcvd", (ModelNode*) newAPC, BOOLEAN, OR, TRG_OPT_DATA_CHANGED, 0, 0);

    if (controlOptions & CDC_CTL_OPTION_OP_OK)
        DataAttribute_create("opOk", (ModelNode*) newAPC, BOOLEAN, OR, TRG_OPT_DATA_CHANGED, 0, 0);

    if (controlOptions & CDC_CTL_OPTION_T_OP_OK)
        DataAttribute_create("tOpOk", (ModelNode*) newAPC, BOOLEAN, OR, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_PICS_SUBST) {
        DataAttribute_create("subEna", (ModelNode*) newAPC, BOOLEAN, SV, 0, 0, 0);
        CAC_AnalogueValue_create("subVal", (ModelNode*) newAPC, SV, 0, isIntegerNotFloat);
        DataAttribute_create("subQ", (ModelNode*) newAPC, QUALITY, SV, 0, 0, 0);
        DataAttribute_create("subID", (ModelNode*) newAPC, VISIBLE_STRING_64, SV, 0, 0, 0);
    }

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newAPC, BOOLEAN, BL, 0, 0, 0);

    addAnalogControls(newAPC, controlOptions, isIntegerNotFloat);

    CDC_addStandardOptions(newAPC, options);

    return newAPC;
}


DataObject*
CDC_INC_create(const char* dataObjectName, ModelNode* parent, uint32_t options, uint32_t controlOptions)
{
    DataObject* newINC = DataObject_create(dataObjectName, parent, 0);

    addOriginatorAndCtlNumOptions((ModelNode*) newINC, controlOptions);

    CDC_addStatusToDataObject(newINC, INT32);

    addControls(newINC, INT32, controlOptions);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubst(newINC, INT32);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newINC, BOOLEAN, BL, 0, 0, 0);

    if (options & CDC_OPTION_MIN)
        DataAttribute_create("minVal", (ModelNode*) newINC, INT32, CF, 0, 0, 0);

    if (options & CDC_OPTION_MAX)
        DataAttribute_create("maxVal", (ModelNode*) newINC, INT32, CF, 0, 0, 0);

    if (options & CDC_OPTION_STEP_SIZE)
        DataAttribute_create("stepSize", (ModelNode*) newINC, INT32U, CF, 0, 0, 0);

    CDC_addStandardOptions(newINC, options);

    return newINC;
}

DataObject*
CDC_ENC_create(const char* dataObjectName, ModelNode* parent, uint32_t options, uint32_t controlOptions)
{
    DataObject* newENC = DataObject_create(dataObjectName, parent, 0);

    addOriginatorAndCtlNumOptions((ModelNode*) newENC, controlOptions);

    CDC_addStatusToDataObject(newENC, ENUMERATED);

    addControls(newENC, ENUMERATED, controlOptions);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubst(newENC, ENUMERATED);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newENC, BOOLEAN, BL, 0, 0, 0);

    CDC_addStandardOptions(newENC, options);

    return newENC;
}

DataObject*
CDC_BSC_create(const char* dataObjectName, ModelNode* parent, uint32_t options, uint32_t controlOptions, bool hasTransientIndicator)
{
    DataObject* newBSC = DataObject_create(dataObjectName, parent, 0);

    addOriginatorAndCtlNumOptions((ModelNode*) newBSC, controlOptions);

    CAC_ValWithTrans_create("valWTr", (ModelNode*) newBSC, ST, TRG_OPT_DATA_CHANGED, hasTransientIndicator);
    CDC_addTimeQuality(newBSC, ST);

    DataAttribute_create("persistent", (ModelNode*) newBSC, BOOLEAN, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    addControls(newBSC, CODEDENUM, controlOptions);

    if (options & CDC_OPTION_PICS_SUBST)
        CDC_addOptionPicsSubstValWithTrans(newBSC, hasTransientIndicator);

    if (options & CDC_OPTION_BLK_ENA)
        DataAttribute_create("blkEna", (ModelNode*) newBSC, BOOLEAN, BL, 0, 0, 0);

    CDC_addStandardOptions(newBSC, options);

    return newBSC;
}

DataObject*
CDC_LPL_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newLPL = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("vendor", (ModelNode*) newLPL, VISIBLE_STRING_255, DC, 0, 0, 0);
    DataAttribute_create("swRev", (ModelNode*) newLPL, VISIBLE_STRING_255, DC, 0, 0, 0);

    CDC_addStandardOptions(newLPL, options);

    return newLPL;
}

/* Directional protection activation information (ACD) */
DataObject*
CDC_ACD_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newACD = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("general", (ModelNode*) newACD, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);
    DataAttribute_create("dirGeneral", (ModelNode*) newACD, ENUMERATED, ST, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_PHASE_A) {
        DataAttribute_create("phsA", (ModelNode*) newACD, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);
        DataAttribute_create("dirPhsA", (ModelNode*) newACD, ENUMERATED, ST, TRG_OPT_DATA_CHANGED, 0, 0);
    }

    if (options & CDC_OPTION_PHASE_B) {
        DataAttribute_create("phsB", (ModelNode*) newACD, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);
        DataAttribute_create("dirPhsB", (ModelNode*) newACD, ENUMERATED, ST, TRG_OPT_DATA_CHANGED, 0, 0);
    }

    if (options & CDC_OPTION_PHASE_C) {
        DataAttribute_create("phsC", (ModelNode*) newACD, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);
        DataAttribute_create("dirPhsC", (ModelNode*) newACD, ENUMERATED, ST, TRG_OPT_DATA_CHANGED, 0, 0);
    }

    if (options & CDC_OPTION_PHASE_NEUT) {
        DataAttribute_create("neut", (ModelNode*) newACD, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);
        DataAttribute_create("dirNeut", (ModelNode*) newACD, ENUMERATED, ST, TRG_OPT_DATA_CHANGED, 0, 0);
    }

    CDC_addTimeQuality(newACD, ST);

    CDC_addStandardOptions(newACD, options);

    return newACD;
}

DataObject*
CDC_ACT_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newACT = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("general", (ModelNode*) newACT, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_PHASE_A)
        DataAttribute_create("phsA", (ModelNode*) newACT, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_PHASE_B)
        DataAttribute_create("phsB", (ModelNode*) newACT, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_PHASE_C)
        DataAttribute_create("phsC", (ModelNode*) newACT, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_PHASE_NEUT)
        DataAttribute_create("neut", (ModelNode*) newACT, BOOLEAN, ST, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addTimeQuality(newACT, ST);

    CDC_addStandardOptions(newACT, options);

    return newACT;
}

DataObject*
CDC_WYE_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newWYE = DataObject_create(dataObjectName, parent, 0);

    //TODO check if some options should be masked
    //TODO take care for GC_1
    CDC_CMV_create("phsA", (ModelNode*) newWYE, options);
    CDC_CMV_create("phsB", (ModelNode*) newWYE, options);
    CDC_CMV_create("phsC", (ModelNode*) newWYE, options);
    CDC_CMV_create("neut", (ModelNode*) newWYE, options);
    CDC_CMV_create("net", (ModelNode*) newWYE, options);
    CDC_CMV_create("res", (ModelNode*) newWYE, options);

    if (options & CDC_OPTION_ANGLE_REF)
        DataAttribute_create("angRef", (ModelNode*) newWYE, ENUMERATED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addStandardOptions(newWYE, options);

    return newWYE;
}


DataObject*
CDC_DEL_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newDEL = DataObject_create(dataObjectName, parent, 0);

    //TODO check if some options should be masked
    CDC_CMV_create("phsAB", (ModelNode*) newDEL, options);
    CDC_CMV_create("phsBC", (ModelNode*) newDEL, options);
    CDC_CMV_create("phsCA", (ModelNode*) newDEL, options);

    if (options & CDC_OPTION_ANGLE_REF)
        DataAttribute_create("angRef", (ModelNode*) newDEL, ENUMERATED, CF, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addStandardOptions(newDEL, options);

    return newDEL;
}


DataObject*
CDC_SPG_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newSPG = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("setVal", (ModelNode*) newSPG, BOOLEAN, SP, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addStandardOptions(newSPG, options);

    return newSPG;
}

DataObject*
CDC_ENG_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newENG = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("setVal", (ModelNode*) newENG, ENUMERATED, SP, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addStandardOptions(newENG, options);

    return newENG;
}

DataObject*
CDC_ING_create(const char* dataObjectName, ModelNode* parent, uint32_t options)
{
    DataObject* newING = DataObject_create(dataObjectName, parent, 0);

    DataAttribute_create("setVal", (ModelNode*) newING, INT32, SP, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_UNIT)
        CAC_Unit_create("units", (ModelNode*) newING, options & CDC_OPTION_UNIT_MULTIPLIER);

    if (options & CDC_OPTION_MIN)
        DataAttribute_create("minVal", (ModelNode*) newING, INT32, SP, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_MAX)
        DataAttribute_create("maxVal", (ModelNode*) newING, INT32, SP, TRG_OPT_DATA_CHANGED, 0, 0);

    if (options & CDC_OPTION_STEP_SIZE)
        DataAttribute_create("stepSize", (ModelNode*) newING, INT32U, SP, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addStandardOptions(newING, options);


    return newING;
}

DataObject*
CDC_ASG_create(const char* dataObjectName, ModelNode* parent, uint32_t options, bool isIntegerNotFloat)
{
    DataObject* newASG = DataObject_create(dataObjectName, parent, 0);

    CAC_AnalogueValue_create("setMag", (ModelNode*) newASG, SP, TRG_OPT_DATA_CHANGED, isIntegerNotFloat);

    if (options & CDC_OPTION_UNIT)
        CAC_Unit_create("units", (ModelNode*) newASG, options & CDC_OPTION_UNIT_MULTIPLIER);

    if (options & CDC_OPTION_AC_SCAV)
        CAC_ScaledValueConfig_create("sVC", (ModelNode*) newASG);

    if (options & CDC_OPTION_MIN)
        CAC_AnalogueValue_create("minVal", (ModelNode*) newASG, CF, TRG_OPT_DATA_CHANGED, isIntegerNotFloat);

    if (options & CDC_OPTION_MAX)
        CAC_AnalogueValue_create("maxVal", (ModelNode*) newASG, CF, TRG_OPT_DATA_CHANGED, isIntegerNotFloat);

    if (options & CDC_OPTION_STEP_SIZE)
        CAC_AnalogueValue_create("stepSize", (ModelNode*) newASG, CF, TRG_OPT_DATA_CHANGED, isIntegerNotFloat);

    CDC_addStandardOptions(newASG, options);

    return newASG;
}

/**********************************************************************************
 * Wind power specific CDCs - according to 61400-25-2:2006
 *********************************************************************************/

DataObject*
CDC_SPV_create(const char* dataObjectName, ModelNode* parent, uint32_t options, uint32_t controlOptions, uint32_t wpOptions, bool hasChaManRs)
{
    DataObject* newSPV = DataObject_create(dataObjectName, parent, 0);

    if (hasChaManRs)
        CDC_SPC_create("chaManRs", (ModelNode*) newSPV, 0, CDC_CTL_MODEL_DIRECT_NORMAL);

    CDC_APC_create("actVal", (ModelNode*) newSPV, 0, controlOptions, false);

    //TOOO add optional "oldVal" APC

    if (wpOptions & CDC_OPTION_61400_MIN_MX_VAL)
        CAC_AnalogueValue_create("minMxVal", (ModelNode*) newSPV, MX, 0, false);

    if (wpOptions & CDC_OPTION_61400_MAX_MX_VAL)
        CAC_AnalogueValue_create("maxMxVal", (ModelNode*) newSPV, MX, 0, false);

    if (wpOptions & CDC_OPTION_61400_TOT_AV_VAL)
        CAC_AnalogueValue_create("totAvVal", (ModelNode*) newSPV, MX, 0, false);

    if (wpOptions & CDC_OPTION_61400_SDV_VAL)
        CAC_AnalogueValue_create("sdvVal", (ModelNode*) newSPV, MX, 0, false);

    if (options & CDC_OPTION_UNIT)
        CAC_Unit_create("units", (ModelNode*) newSPV, options & CDC_OPTION_UNIT_MULTIPLIER);

    if (options & CDC_OPTION_MIN)
        CAC_AnalogueValue_create("minVal", (ModelNode*) newSPV, CF, TRG_OPT_DATA_CHANGED, false);

    if (options & CDC_OPTION_MAX)
        CAC_AnalogueValue_create("maxVal", (ModelNode*) newSPV, CF, TRG_OPT_DATA_CHANGED, false);

    if (wpOptions & CDC_OPTION_61400_SP_ACS)
        DataAttribute_create("spAcs", (ModelNode*) newSPV, CODEDENUM, CF, 0, 0, 0);

    if (wpOptions & CDC_OPTION_61400_CHA_PER_RS)
        DataAttribute_create("chaPerRs", (ModelNode*) newSPV, CODEDENUM, CF, 0, 0, 0);

    CDC_addStandardOptions(newSPV, options);

    return newSPV;
}

DataObject*
CDC_STV_create(const char* dataObjectName, ModelNode* parent,
        uint32_t options,
        uint32_t controlOptions,
        uint32_t wpOptions,
        bool hasOldStatus)
{
    DataObject* newSTV = DataObject_create(dataObjectName, parent, 0);

    CDC_INS_create("actSt", (ModelNode*) newSTV, 0);

    if (hasOldStatus)
        CDC_INS_create("oldSt", (ModelNode*) newSTV, 0);

    CDC_addStandardOptions(newSTV, options);

    return newSTV;
}

DataObject*
CDC_ALM_create(const char* dataObjectName, ModelNode* parent,
        uint32_t options,
        uint32_t controlOptions,
        uint32_t wpOptions,
        bool hasOldStatus)
{
    DataObject* newALM = DataObject_create(dataObjectName, parent, 0);

    CDC_SPC_create("almAck", (ModelNode*) newALM, 0, CDC_CTL_MODEL_DIRECT_NORMAL | CDC_CTL_OPTION_ORIGIN);

    CDC_INS_create("actSt", (ModelNode*) newALM, 0);

    if (hasOldStatus)
        CDC_INS_create("oldSt", (ModelNode*) newALM, 0);

    CDC_addStandardOptions(newALM, options);

    return newALM;
}

DataObject*
CDC_CMD_create(const char* dataObjectName, ModelNode* parent,
        uint32_t options,
        uint32_t controlOptions,
        uint32_t wpOptions,
        bool hasOldStatus,
        bool hasCmTm,
        bool hasCmCt)
{
    DataObject* newCMD = DataObject_create(dataObjectName, parent, 0);

    CDC_INC_create("actSt", (ModelNode*) newCMD, 0, controlOptions);

    if (hasOldStatus)
        CDC_INS_create("oldSt", (ModelNode*) newCMD, 0);

    if (wpOptions & CDC_OPTION_61400_CM_ACS)
        DataAttribute_create("cmAcs", (ModelNode*) newCMD, INT8U, CF, 0, 0, 0);

    CDC_addStandardOptions(newCMD, options);

    return newCMD;
}


/**
 * \brief create a new CDC instance of type CTE (Event counting)
 */
DataObject*
CDC_CTE_create(const char* dataObjectName, ModelNode* parent,
        uint32_t options,
        uint32_t controlOptions,
        uint32_t wpOptions,
        bool hasHisRs)
{
    DataObject* newCTE = DataObject_create(dataObjectName, parent, 0);

    CDC_SPC_create("manRs", (ModelNode*) newCTE, 0, CDC_CTL_MODEL_DIRECT_NORMAL | CDC_CTL_OPTION_ORIGIN);

    if (hasHisRs)
        CDC_INC_create("hisRs", (ModelNode*) newCTE, 0, CDC_CTL_MODEL_DIRECT_NORMAL | CDC_CTL_OPTION_ORIGIN);

    CDC_INS_create("actCtVal", (ModelNode*) newCTE, 0);

    CDC_INS_create("oldCtVal", (ModelNode*) newCTE, 0);

    if (wpOptions & CDC_OPTION_61400_TM_TOT)
        DataAttribute_create("ctTot", (ModelNode*) newCTE, INT32U, ST, 0, 0, 0);

    if (wpOptions & CDC_OPTION_61400_COUNTING_DAILY)
        DataAttribute_create("dly", (ModelNode*) newCTE, INT32U, ST, TRG_OPT_DATA_CHANGED, 32, 0);

    if (wpOptions & CDC_OPTION_61400_COUNTING_MONTHLY)
        DataAttribute_create("mly", (ModelNode*) newCTE, INT32U, ST, TRG_OPT_DATA_CHANGED, 13, 0);

    if (wpOptions & CDC_OPTION_61400_COUNTING_YEARLY)
        DataAttribute_create("mly", (ModelNode*) newCTE, INT32U, ST, TRG_OPT_DATA_CHANGED, 21, 0);

    if (wpOptions & CDC_OPTION_61400_COUNTING_TOTAL)
        DataAttribute_create("tot", (ModelNode*) newCTE, INT32U, ST, TRG_OPT_DATA_CHANGED, 0, 0);

    CDC_addStandardOptions(newCTE, options);

    return newCTE;
}


DataObject*
CDC_TMS_create(const char* dataObjectName, ModelNode* parent,
        uint32_t options,
        uint32_t controlOptions,
        uint32_t wpOptions,
        bool hasHisRs)
{
    DataObject* newTMS = DataObject_create(dataObjectName, parent, 0);

    CDC_SPC_create("manRs", (ModelNode*) newTMS, 0, CDC_CTL_MODEL_DIRECT_NORMAL | CDC_CTL_OPTION_ORIGIN);

    if (hasHisRs)
        CDC_INC_create("hisRs", (ModelNode*) newTMS, 0, CDC_CTL_MODEL_DIRECT_NORMAL | CDC_CTL_OPTION_ORIGIN);

    CDC_INS_create("actTmVal", (ModelNode*) newTMS, 0);

    CDC_INS_create("oldTmVal", (ModelNode*) newTMS, 0);

    if (wpOptions & CDC_OPTION_61400_TM_TOT)
        DataAttribute_create("tmTot", (ModelNode*) newTMS, INT32U, ST, 0, 0, 0);

    if (wpOptions & CDC_OPTION_61400_COUNTING_DAILY)
        DataAttribute_create("dly", (ModelNode*) newTMS, INT32U, ST, TRG_OPT_DATA_CHANGED, 32, 0);

    if (wpOptions & CDC_OPTION_61400_COUNTING_MONTHLY)
        DataAttribute_create("mly", (ModelNode*) newTMS, INT32U, ST, TRG_OPT_DATA_CHANGED, 13, 0);

    if (wpOptions & CDC_OPTION_61400_COUNTING_YEARLY)
        DataAttribute_create("mly", (ModelNode*) newTMS, INT32U, ST, TRG_OPT_DATA_CHANGED, 21, 0);

    if (wpOptions & CDC_OPTION_61400_COUNTING_TOTAL)
        DataAttribute_create("tot", (ModelNode*) newTMS, INT32U, ST, TRG_OPT_DATA_CHANGED, 0, 0);


    CDC_addStandardOptions(newTMS, options);

    return newTMS;
}

