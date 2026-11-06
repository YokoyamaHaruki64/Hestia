/*=============================================================================

 File   : HMath.h
 Desc   : 数学ライブラリの共通ヘッダ。SIMD版と通常版の両方を含む。

 ------------------------------------------------------------------------------

 Date   : 2026/10/08
 Author : Yokoyama Haruki

===============================================================================*/

#ifndef _HMATH_H_
#define _HMATH_H_

#include "Math/MathCommon.h"

#include "Math/Vector2.h"
#include "Math/Vector3.h"
#include "Math/Vector4.h"
#include "Math/Quaternion.h"
#include "Math/Matrix3x3.h"
#include "Math/Matrix4x4.h"

#include "Math/Vector2_SIMD.h"
#include "Math/Vector3_SIMD.h"
#include "Math/Vector4_SIMD.h"
#include "Math/Quaternion_SIMD.h"
#include "Math/Matrix3x3_SIMD.h"
#include "Math/Matrix4x4_SIMD.h"

#include "Math/SimdInterop.h"

#endif // _HMATH_H_
