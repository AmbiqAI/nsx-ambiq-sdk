#pragma once

#include <stdint.h>
#define __IM volatile const
#define __OM volatile
#define __IOM volatile
#define _VAL2FLD(field, value) (((uint32_t)(value) << field##_Pos) & field##_Msk)
#define _FLD2VAL(field, value) (((value) & field##_Msk) >> field##_Pos)
