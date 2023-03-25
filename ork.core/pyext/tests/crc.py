#!/usr/bin/env python3

import numpy as np
from orkengine.core import *

tokens = CrcStringProxy()

dict = {
    "GBU_CT_VN_RI_NI_MO": tokens.GBU_CT_VN_RI_NI_MO,
    "GBU_CV_EMI_RI_NI_MO": tokens.GBU_CV_EMI_RI_NI_MO,
    "GBU_DB_NM_NI_MO": tokens.GBU_DB_NM_NI_MO,
    "GBU_CT_NM_RI_NI_MO": tokens.GBU_CT_NM_RI_NI_MO,
    "GBU_CM_NM_RI_NI_MO": tokens.GBU_CM_NM_RI_NI_MO,
    "GBU_CT_NM_RI_IN_MO": tokens.GBU_CT_NM_RI_IN_MO,
}
for key in dict.keys():
    val = dict[key]
    print(key, val)
