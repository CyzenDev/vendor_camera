/*============================================================================

Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

============================================================================*/

/*============================================================================
 *                      INCLUDE FILES
 *===========================================================================*/
#include "chromatix_common.h"
#include "chromatix_3a.h"

static chromatix_VFE_common_type chromatix_s5k3l8_parms = {
#include "chromatix_s5k3l8_common.h"
};

/*============================================================================
 * FUNCTION    - load_chromatix -
 *
 * DESCRIPTION:
 *==========================================================================*/
void *load_chromatix(void)
{
  return &chromatix_s5k3l8_parms;
}
