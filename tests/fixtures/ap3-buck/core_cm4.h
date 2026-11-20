/* Host stand-ins for the CMSIS core register qualifiers and field helpers. */
#pragma once
#include <stdint.h>
#define __IM volatile const
#define __OM volatile
#define __IOM volatile
#define _VAL2FLD(field, val) (((uint32_t)(val) << field##_Pos) & field##_Msk)
#define _FLD2VAL(field, val) (((val) & field##_Msk) >> field##_Pos)
