/*
 * static_model.c
 *
 * automatically generated from C:/Users/sungh/Desktop/IEDEX/libiec61850-1.6/examples/server_example_setpoint/setpoint_tests.cid
 */
#include "static_model.h"

static void initializeValues();

extern DataSet iedModelds_GenericIO_LLN0_SetpointEvents;

extern DataSetEntry iedModelds_GenericIO_LLN0_SetpointEvents_fcda0;
extern DataSetEntry iedModelds_GenericIO_LLN0_SetpointEvents_fcda1;
extern DataSetEntry iedModelds_GenericIO_LLN0_SetpointEvents_fcda2;
extern DataSetEntry iedModelds_GenericIO_LLN0_SetpointEvents_fcda3;

DataSetEntry iedModelds_GenericIO_LLN0_SetpointEvents_fcda0 = {
  "GenericIO",
  false,
  "GGIO1$MX$AnOut1$mxVal$f",
  -1,
  NULL,
  NULL,
  &iedModelds_GenericIO_LLN0_SetpointEvents_fcda1
};

DataSetEntry iedModelds_GenericIO_LLN0_SetpointEvents_fcda1 = {
  "GenericIO",
  false,
  "GGIO1$MX$AnOut1Fb$mag$f",
  -1,
  NULL,
  NULL,
  &iedModelds_GenericIO_LLN0_SetpointEvents_fcda2
};

DataSetEntry iedModelds_GenericIO_LLN0_SetpointEvents_fcda2 = {
  "GenericIO",
  false,
  "GGIO1$MX$AnOut2$mxVal$f",
  -1,
  NULL,
  NULL,
  &iedModelds_GenericIO_LLN0_SetpointEvents_fcda3
};

DataSetEntry iedModelds_GenericIO_LLN0_SetpointEvents_fcda3 = {
  "GenericIO",
  false,
  "GGIO1$MX$AnOut2Fb$mag$f",
  -1,
  NULL,
  NULL,
  NULL
};

DataSet iedModelds_GenericIO_LLN0_SetpointEvents = {
  "GenericIO",
  "LLN0$SetpointEvents",
  4,
  &iedModelds_GenericIO_LLN0_SetpointEvents_fcda0,
  NULL
};

LogicalDevice iedModel_GenericIO = {
  LogicalDeviceModelType,
  "GenericIO",
  (ModelNode*) &iedModel,
  NULL,
  (ModelNode*) &iedModel_GenericIO_LLN0,
  NULL
};

LogicalNode iedModel_GenericIO_LLN0 = {
  LogicalNodeModelType,
  "LLN0",
  (ModelNode*) &iedModel_GenericIO,
  (ModelNode*) &iedModel_GenericIO_LPHD1,
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod
};

DataObject iedModel_GenericIO_LLN0_Mod = {
  DataObjectModelType,
  "Mod",
  (ModelNode*) &iedModel_GenericIO_LLN0,
  (ModelNode*) &iedModel_GenericIO_LLN0_Beh,
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod_stVal,
  0,
  -1
};
DataAttribute iedModel_GenericIO_LLN0_Mod_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod,
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod_q,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_Mod_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod,
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod_t,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_Mod_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod,
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod_ctlModel,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_Mod_ctlModel = {
  DataAttributeModelType,
  "ctlModel",
  (ModelNode*) &iedModel_GenericIO_LLN0_Mod,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CF,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_LLN0_Beh = {
  DataObjectModelType,
  "Beh",
  (ModelNode*) &iedModel_GenericIO_LLN0,
  (ModelNode*) &iedModel_GenericIO_LLN0_Health,
  (ModelNode*) &iedModel_GenericIO_LLN0_Beh_stVal,
  0,
  -1
};
DataAttribute iedModel_GenericIO_LLN0_Beh_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_GenericIO_LLN0_Beh,
  (ModelNode*) &iedModel_GenericIO_LLN0_Beh_q,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_Beh_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_LLN0_Beh,
  (ModelNode*) &iedModel_GenericIO_LLN0_Beh_t,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_Beh_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_LLN0_Beh,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_LLN0_Health = {
  DataObjectModelType,
  "Health",
  (ModelNode*) &iedModel_GenericIO_LLN0,
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt,
  (ModelNode*) &iedModel_GenericIO_LLN0_Health_stVal,
  0,
  -1
};
DataAttribute iedModel_GenericIO_LLN0_Health_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_GenericIO_LLN0_Health,
  (ModelNode*) &iedModel_GenericIO_LLN0_Health_q,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_Health_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_LLN0_Health,
  (ModelNode*) &iedModel_GenericIO_LLN0_Health_t,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_Health_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_LLN0_Health,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_LLN0_NamPlt = {
  DataObjectModelType,
  "NamPlt",
  (ModelNode*) &iedModel_GenericIO_LLN0,
  NULL,
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt_vendor,
  0,
  -1
};
DataAttribute iedModel_GenericIO_LLN0_NamPlt_vendor = {
  DataAttributeModelType,
  "vendor",
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt,
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt_swRev,
  NULL,
  0,
  -1,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_NamPlt_swRev = {
  DataAttributeModelType,
  "swRev",
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt,
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt_d,
  NULL,
  0,
  -1,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_NamPlt_d = {
  DataAttributeModelType,
  "d",
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt,
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt_configRev,
  NULL,
  0,
  -1,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_NamPlt_configRev = {
  DataAttributeModelType,
  "configRev",
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt,
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt_ldNs,
  NULL,
  0,
  -1,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LLN0_NamPlt_ldNs = {
  DataAttributeModelType,
  "ldNs",
  (ModelNode*) &iedModel_GenericIO_LLN0_NamPlt,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_EX,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
LogicalNode iedModel_GenericIO_LPHD1 = {
  LogicalNodeModelType,
  "LPHD1",
  (ModelNode*) &iedModel_GenericIO,
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyNam
};

DataObject iedModel_GenericIO_LPHD1_PhyNam = {
  DataObjectModelType,
  "PhyNam",
  (ModelNode*) &iedModel_GenericIO_LPHD1,
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyHealth,
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyNam_vendor,
  0,
  -1
};
DataAttribute iedModel_GenericIO_LPHD1_PhyNam_vendor = {
  DataAttributeModelType,
  "vendor",
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyNam,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_LPHD1_PhyHealth = {
  DataObjectModelType,
  "PhyHealth",
  (ModelNode*) &iedModel_GenericIO_LPHD1,
  (ModelNode*) &iedModel_GenericIO_LPHD1_Proxy,
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyHealth_stVal,
  0,
  -1
};
DataAttribute iedModel_GenericIO_LPHD1_PhyHealth_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyHealth,
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyHealth_q,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LPHD1_PhyHealth_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyHealth,
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyHealth_t,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LPHD1_PhyHealth_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_LPHD1_PhyHealth,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_LPHD1_Proxy = {
  DataObjectModelType,
  "Proxy",
  (ModelNode*) &iedModel_GenericIO_LPHD1,
  NULL,
  (ModelNode*) &iedModel_GenericIO_LPHD1_Proxy_stVal,
  0,
  -1
};
DataAttribute iedModel_GenericIO_LPHD1_Proxy_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_GenericIO_LPHD1_Proxy,
  (ModelNode*) &iedModel_GenericIO_LPHD1_Proxy_q,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_BOOLEAN,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LPHD1_Proxy_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_LPHD1_Proxy,
  (ModelNode*) &iedModel_GenericIO_LPHD1_Proxy_t,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_LPHD1_Proxy_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_LPHD1_Proxy,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
LogicalNode iedModel_GenericIO_GGIO1 = {
  LogicalNodeModelType,
  "GGIO1",
  (ModelNode*) &iedModel_GenericIO,
  NULL,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod
};

DataObject iedModel_GenericIO_GGIO1_Mod = {
  DataObjectModelType,
  "Mod",
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Beh,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod_stVal,
  0,
  -1
};
DataAttribute iedModel_GenericIO_GGIO1_Mod_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod_q,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_Mod_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod_t,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_Mod_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod_ctlModel,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_Mod_ctlModel = {
  DataAttributeModelType,
  "ctlModel",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Mod,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CF,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_GGIO1_Beh = {
  DataObjectModelType,
  "Beh",
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Health,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Beh_stVal,
  0,
  -1
};
DataAttribute iedModel_GenericIO_GGIO1_Beh_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Beh,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Beh_q,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_Beh_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Beh,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Beh_t,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_Beh_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Beh,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_GGIO1_Health = {
  DataObjectModelType,
  "Health",
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_NamPlt,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Health_stVal,
  0,
  -1
};
DataAttribute iedModel_GenericIO_GGIO1_Health_stVal = {
  DataAttributeModelType,
  "stVal",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Health,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Health_q,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_INT32,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_Health_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Health,
  (ModelNode*) &iedModel_GenericIO_GGIO1_Health_t,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_Health_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_GGIO1_Health,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_ST,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_GGIO1_NamPlt = {
  DataObjectModelType,
  "NamPlt",
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_NamPlt_vendor,
  0,
  -1
};
DataAttribute iedModel_GenericIO_GGIO1_NamPlt_vendor = {
  DataAttributeModelType,
  "vendor",
  (ModelNode*) &iedModel_GenericIO_GGIO1_NamPlt,
  (ModelNode*) &iedModel_GenericIO_GGIO1_NamPlt_swRev,
  NULL,
  0,
  -1,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_NamPlt_swRev = {
  DataAttributeModelType,
  "swRev",
  (ModelNode*) &iedModel_GenericIO_GGIO1_NamPlt,
  (ModelNode*) &iedModel_GenericIO_GGIO1_NamPlt_d,
  NULL,
  0,
  -1,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_NamPlt_d = {
  DataAttributeModelType,
  "d",
  (ModelNode*) &iedModel_GenericIO_GGIO1_NamPlt,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_DC,
  IEC61850_VISIBLE_STRING_255,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_GGIO1_AnOut1 = {
  DataObjectModelType,
  "AnOut1",
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper,
  0,
  -1
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper = {
  DataAttributeModelType,
  "Oper",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_mxVal,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_ctlVal,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_ctlVal = {
  DataAttributeModelType,
  "ctlVal",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_origin,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_ctlVal_f,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_ctlVal_f = {
  DataAttributeModelType,
  "f",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_ctlVal,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_FLOAT32,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_origin = {
  DataAttributeModelType,
  "origin",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_ctlNum,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_origin_orCat,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_origin_orCat = {
  DataAttributeModelType,
  "orCat",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_origin,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_origin_orIdent,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_origin_orIdent = {
  DataAttributeModelType,
  "orIdent",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_origin,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_OCTET_STRING_64,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_ctlNum = {
  DataAttributeModelType,
  "ctlNum",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_T,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_INT8U,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_T = {
  DataAttributeModelType,
  "T",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_Test,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_Test = {
  DataAttributeModelType,
  "Test",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper_Check,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_BOOLEAN,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_Oper_Check = {
  DataAttributeModelType,
  "Check",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_Oper,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_CHECK,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_mxVal = {
  DataAttributeModelType,
  "mxVal",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_q,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_mxVal_f,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_CONSTRUCTED,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_mxVal_f = {
  DataAttributeModelType,
  "f",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_mxVal,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_FLOAT32,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_t,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1_ctlModel,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1_ctlModel = {
  DataAttributeModelType,
  "ctlModel",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CF,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_GGIO1_AnOut1Fb = {
  DataObjectModelType,
  "AnOut1Fb",
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb_mag,
  0,
  -1
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1Fb_mag = {
  DataAttributeModelType,
  "mag",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb_q,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb_mag_f,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_CONSTRUCTED,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1Fb_mag_f = {
  DataAttributeModelType,
  "f",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb_mag,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_FLOAT32,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1Fb_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb_t,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut1Fb_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut1Fb,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_GGIO1_AnOut2 = {
  DataObjectModelType,
  "AnOut2",
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper,
  0,
  -1
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper = {
  DataAttributeModelType,
  "Oper",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_mxVal,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_ctlVal,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_ctlVal = {
  DataAttributeModelType,
  "ctlVal",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_origin,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_ctlVal_f,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_ctlVal_f = {
  DataAttributeModelType,
  "f",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_ctlVal,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_FLOAT32,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_origin = {
  DataAttributeModelType,
  "origin",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_ctlNum,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_origin_orCat,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_CONSTRUCTED,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_origin_orCat = {
  DataAttributeModelType,
  "orCat",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_origin,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_origin_orIdent,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_origin_orIdent = {
  DataAttributeModelType,
  "orIdent",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_origin,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_OCTET_STRING_64,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_ctlNum = {
  DataAttributeModelType,
  "ctlNum",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_T,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_INT8U,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_T = {
  DataAttributeModelType,
  "T",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_Test,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_Test = {
  DataAttributeModelType,
  "Test",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper_Check,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_BOOLEAN,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_Oper_Check = {
  DataAttributeModelType,
  "Check",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_Oper,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CO,
  IEC61850_CHECK,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_mxVal = {
  DataAttributeModelType,
  "mxVal",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_q,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_mxVal_f,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_CONSTRUCTED,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_mxVal_f = {
  DataAttributeModelType,
  "f",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_mxVal,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_FLOAT32,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_t,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2_ctlModel,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2_ctlModel = {
  DataAttributeModelType,
  "ctlModel",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_CF,
  IEC61850_ENUMERATED,
  0,
  NULL,
  0
};
DataObject iedModel_GenericIO_GGIO1_AnOut2Fb = {
  DataObjectModelType,
  "AnOut2Fb",
  (ModelNode*) &iedModel_GenericIO_GGIO1,
  NULL,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb_mag,
  0,
  -1
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2Fb_mag = {
  DataAttributeModelType,
  "mag",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb_q,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb_mag_f,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_CONSTRUCTED,
  0 + TRG_OPT_DATA_CHANGED + TRG_OPT_DATA_UPDATE + TRG_OPT_QUALITY_CHANGED,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2Fb_mag_f = {
  DataAttributeModelType,
  "f",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb_mag,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_FLOAT32,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2Fb_q = {
  DataAttributeModelType,
  "q",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb,
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb_t,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_QUALITY,
  0,
  NULL,
  0
};
DataAttribute iedModel_GenericIO_GGIO1_AnOut2Fb_t = {
  DataAttributeModelType,
  "t",
  (ModelNode*) &iedModel_GenericIO_GGIO1_AnOut2Fb,
  NULL,
  NULL,
  0,
  -1,
  IEC61850_FC_MX,
  IEC61850_TIMESTAMP,
  0,
  NULL,
  0
};
extern ReportControlBlock iedModel_GenericIO_LLN0_report0;
extern ReportControlBlock iedModel_GenericIO_LLN0_report1;

ReportControlBlock iedModel_GenericIO_LLN0_report0 = {
  &iedModel_GenericIO_LLN0,
  "SetpointRCB01",
  "SetpointEvents",
  false,
  "SetpointEvents",
  1,
  17,
  239,
  0,
  1000,
  {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
  &iedModel_GenericIO_LLN0_report1
};
ReportControlBlock iedModel_GenericIO_LLN0_report1 = {
  &iedModel_GenericIO_LLN0,
  "SetpointRCB02",
  "SetpointEvents",
  false,
  "SetpointEvents",
  1,
  17,
  239,
  0,
  1000,
  {0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0},
  NULL
};









IedModel iedModel = {
  "setpointIO",
  &iedModel_GenericIO,
  &iedModelds_GenericIO_LLN0_SetpointEvents,
  &iedModel_GenericIO_LLN0_report0,
  NULL,
  NULL,
  NULL,
  NULL,
  NULL,
  initializeValues
};

static void
initializeValues()
{

iedModel_GenericIO_LLN0_Mod_ctlModel.mmsValue = MmsValue_newIntegerFromInt32(0);

iedModel_GenericIO_GGIO1_Mod_ctlModel.mmsValue = MmsValue_newIntegerFromInt32(0);

iedModel_GenericIO_GGIO1_AnOut1_ctlModel.mmsValue = MmsValue_newIntegerFromInt32(3);

iedModel_GenericIO_GGIO1_AnOut2_ctlModel.mmsValue = MmsValue_newIntegerFromInt32(3);
}
