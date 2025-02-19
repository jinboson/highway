// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// 256-bit LASX vectors and operations.
// External include guard in highway.h - see comment there.

#include <lasxintrin.h>

#include "hwy/ops/loongarch_lsx-inl.h"
#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {
namespace detail {

// we don't have intrinsics to operate between 128-bit and 256-bit,
// so use union to emulate it.
typedef union My256 {
  __m256i i256;
  __m128i i128[2];
} U256;

template <typename T>
struct Raw256 {
  using type = __m256i;
};
template <>
struct Raw256<float> {
  using type = __m256;
};
template <>
struct Raw256<double> {
  using type = __m256d;
};

}  // namespace detail

template <typename T>
class Vec256 {
  using Raw = typename detail::Raw256<T>::type;

 public:
  using PrivateT = T;                                  // only for DFromV
  static constexpr size_t kPrivateN = 32 / sizeof(T);  // only for DFromV

  // Compound assignment. Only usable if there is a corresponding non-member
  // binary operator overload. For example, only f32 and f64 support division.
  HWY_INLINE Vec256& operator*=(const Vec256 other) {
    return *this = (*this * other);
  }
  HWY_INLINE Vec256& operator/=(const Vec256 other) {
    return *this = (*this / other);
  }
  HWY_INLINE Vec256& operator+=(const Vec256 other) {
    return *this = (*this + other);
  }
  HWY_INLINE Vec256& operator-=(const Vec256 other) {
    return *this = (*this - other);
  }
  HWY_INLINE Vec256& operator%=(const Vec256 other) {
    return *this = (*this % other);
  }
  HWY_INLINE Vec256& operator&=(const Vec256 other) {
    return *this = (*this & other);
  }
  HWY_INLINE Vec256& operator|=(const Vec256 other) {
    return *this = (*this | other);
  }
  HWY_INLINE Vec256& operator^=(const Vec256 other) {
    return *this = (*this ^ other);
  }

  Raw raw;
};

namespace detail {

template <typename T>
using RawMask256 = typename Raw256<T>::type;

}  // namespace detail

template <typename T>
struct Mask256 {
  using Raw = typename detail::RawMask256<T>;

  using PrivateT = T;                                  // only for DFromM
  static constexpr size_t kPrivateN = 32 / sizeof(T);  // only for DFromM

  Raw raw;
};

template <typename T>
using Full256 = Simd<T, 32 / sizeof(T), 0>;

// ------------------------------ Zero

// Cannot use VFromD here because it is defined in terms of Zero.
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_NOT_FLOAT_NOR_SPECIAL_D(D)>
HWY_API Vec256<TFromD<D>> Zero(D /* tag */) {
  return Vec256<TFromD<D>>{__lasx_xvreplgr2vr_d(0)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F32_D(D)>
HWY_API Vec256<float> Zero(D /* tag */) {
  return Vec256<float>{reinterpret_cast<__m256>(__lasx_xvreplgr2vr_d(0))};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API Vec256<double> Zero(D /* tag */) {
  return Vec256<double>{reinterpret_cast<__m256d>(__lasx_xvreplgr2vr_d(0))};
}

// ------------------------------ BitCast

namespace detail {

HWY_INLINE __m256i BitCastToInteger(__m256i v) { return v; }
HWY_INLINE __m256i BitCastToInteger(__m256 v) {
  return reinterpret_cast<__m256i>(v);
}
HWY_INLINE __m256i BitCastToInteger(__m256d v) {
  return reinterpret_cast<__m256i>(v);
}

template <typename T>
HWY_INLINE Vec256<uint8_t> BitCastToByte(Vec256<T> v) {
  return Vec256<uint8_t>{BitCastToInteger(v.raw)};
}

// Cannot rely on function overloading because return types differ.
template <typename T>
struct BitCastFromInteger256 {
  HWY_INLINE __m256i operator()(__m256i v) { return v; }
};
template <>
struct BitCastFromInteger256<float> {
  HWY_INLINE __m256 operator()(__m256i v) {
    return reinterpret_cast<__m256>(v);
  }
};
template <>
struct BitCastFromInteger256<double> {
  HWY_INLINE __m256d operator()(__m256i v) {
    return reinterpret_cast<__m256d>(v);
  }
};

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_INLINE VFromD<D> BitCastFromByte(D /* tag */, Vec256<uint8_t> v) {
  return VFromD<D>{BitCastFromInteger256<TFromD<D>>()(v.raw)};
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_D(D, 32), typename FromT>
HWY_API VFromD<D> BitCast(D d, Vec256<FromT> v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(v));
}

// ------------------------------ Set

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{__lasx_xvreplgr2vr_b(static_cast<char>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI16_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{__lasx_xvreplgr2vr_h(static_cast<short>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{__lasx_xvreplgr2vr_w(static_cast<int>(t))};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> Set(D /* tag */, TFromD<D> t) {
  return VFromD<D>{__lasx_xvreplgr2vr_d(static_cast<long long>(t))};  // NOLINT
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F32_D(D)>
HWY_API Vec256<float> Set(D /* tag */, float t) {
  return BitCast(D(), Vec256<int8_t>{__lasx_xvldrepl_w(&t, 0)});
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API Vec256<double> Set(D /* tag */, double t) {
  return BitCast(D(), Vec256<int8_t>{__lasx_xvldrepl_d(&t, 0)});
}

HWY_DIAGNOSTICS(push)
HWY_DIAGNOSTICS_OFF(disable : 4700, ignored "-Wuninitialized")
#if HWY_COMPILER_GCC_ACTUAL
HWY_DIAGNOSTICS_OFF(disable : 4701, ignored "-Wmaybe-uninitialized")
#endif

template <class D>
HWY_API VFromD<D> Undefined(D /*tag*/) {
#if HWY_HAS_BUILTIN(__builtin_nondeterministic_value)
  return VFromD<D>{__builtin_nondeterministic_value(Zero(D()).raw)};
#else
  VFromD<D> v;
  return v;
#endif
}

HWY_DIAGNOSTICS(pop)

// ------------------------------ ResizeBitCast

// 32-byte vector to 32-byte vector
template <class D, class FromV, HWY_IF_V_SIZE_GT_V(FromV, 16),
          HWY_IF_V_SIZE_D(D, HWY_MAX_LANES_V(FromV) * sizeof(TFromV<FromV>))>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  return BitCast(d, v);
}

// 32-byte vector to 16-byte vector
template <class D, class FromV, HWY_IF_V_SIZE_GT_V(FromV, 16),
          HWY_IF_V_SIZE_D(D, 16)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  const DFromV<decltype(v)> d_from;
  const Half<decltype(d_from)> dh_from;
  return BitCast(d, LowerHalf(dh_from, v));
}

// 32-byte vector to <= 8-byte vector
template <class D, class FromV, HWY_IF_V_SIZE_GT_V(FromV, 16),
          HWY_IF_V_SIZE_LE_D(D, 8)>
HWY_API VFromD<D> ResizeBitCast(D /*d*/, FromV v) {
  return VFromD<D>{ResizeBitCast(Full128<TFromD<D>>(), v).raw};
}

// <= 16-byte vector to 32-byte vector
template <class D, class FromV, HWY_IF_V_SIZE_LE_V(FromV, 16),
          HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ResizeBitCast(D d, FromV v) {
  detail::U256 u;
  u.i128[0] = ResizeBitCast(Full128<uint8_t>(), v).raw;
  return BitCast(d, Vec256<uint8_t>{u.i256});
}

// ------------------------------ Dup128VecFromValues

template <class D, HWY_IF_UI8_D(D), HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3, TFromD<D> t4,
                                      TFromD<D> t5, TFromD<D> t6, TFromD<D> t7,
                                      TFromD<D> t8, TFromD<D> t9, TFromD<D> t10,
                                      TFromD<D> t11, TFromD<D> t12,
                                      TFromD<D> t13, TFromD<D> t14,
                                      TFromD<D> t15) {
  alignas(32) char rawI8[32] = {
      static_cast<char>(t0),  static_cast<char>(t1),  static_cast<char>(t2),
      static_cast<char>(t3),  static_cast<char>(t4),  static_cast<char>(t5),
      static_cast<char>(t6),  static_cast<char>(t7),  static_cast<char>(t8),
      static_cast<char>(t9),  static_cast<char>(t10), static_cast<char>(t11),
      static_cast<char>(t12), static_cast<char>(t13), static_cast<char>(t14),
      static_cast<char>(t15), static_cast<char>(t0),  static_cast<char>(t1),
      static_cast<char>(t2),  static_cast<char>(t3),  static_cast<char>(t4),
      static_cast<char>(t5),  static_cast<char>(t6),  static_cast<char>(t7),
      static_cast<char>(t8),  static_cast<char>(t9),  static_cast<char>(t10),
      static_cast<char>(t11), static_cast<char>(t12), static_cast<char>(t13),
      static_cast<char>(t14), static_cast<char>(t15)};
  return VFromD<D>{__lasx_xvld(rawI8, 0)};
}

template <class D, HWY_IF_UI16_D(D), HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3, TFromD<D> t4,
                                      TFromD<D> t5, TFromD<D> t6,
                                      TFromD<D> t7) {
  alignas(32)
      int16_t rawI16[16] = {static_cast<int16_t>(t0), static_cast<int16_t>(t1),
                            static_cast<int16_t>(t2), static_cast<int16_t>(t3),
                            static_cast<int16_t>(t4), static_cast<int16_t>(t5),
                            static_cast<int16_t>(t6), static_cast<int16_t>(t7),
                            static_cast<int16_t>(t0), static_cast<int16_t>(t1),
                            static_cast<int16_t>(t2), static_cast<int16_t>(t3),
                            static_cast<int16_t>(t4), static_cast<int16_t>(t5),
                            static_cast<int16_t>(t6), static_cast<int16_t>(t7)};
  return VFromD<D>{__lasx_xvld(rawI16, 0)};
}

template <class D, HWY_IF_UI32_D(D), HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3) {
  alignas(32)
      int32_t rawI32[8] = {static_cast<int32_t>(t0), static_cast<int32_t>(t1),
                           static_cast<int32_t>(t2), static_cast<int32_t>(t3),
                           static_cast<int32_t>(t0), static_cast<int32_t>(t1),
                           static_cast<int32_t>(t2), static_cast<int32_t>(t3)};
  return VFromD<D>{__lasx_xvld(rawI32, 0)};
}

template <class D, HWY_IF_F32_D(D), HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1,
                                      TFromD<D> t2, TFromD<D> t3) {
  alignas(32) float rawF32[8] = {t0, t1, t2, t3, t0, t1, t2, t3};
  return BitCast(D(), Vec256<int8_t>{__lasx_xvld(rawF32, 0)});
}

template <class D, HWY_IF_UI64_D(D), HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1) {
  alignas(32) int64_t rawI64[4] = {t0, t1, t0, t1};
  return VFromD<D>{__lasx_xvld(rawI64, 0)};
}

template <class D, HWY_IF_F64_D(D), HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> Dup128VecFromValues(D /*d*/, TFromD<D> t0, TFromD<D> t1) {
  alignas(32) double rawF64[4] = {t0, t1, t0, t1};
  return BitCast(D(), Vec256<int8_t>{__lasx_xvld(rawF64, 0)});
}

// ------------------------------ And

template <typename T>
HWY_API Vec256<T> And(Vec256<T> a, Vec256<T> b) {
  const DFromV<decltype(a)> d;  // for float16_t
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvand_v(BitCast(du, a).raw,
                                                        BitCast(du, b).raw)});
}

// ------------------------------ AndNot

// Returns ~not_mask & mask.
template <typename T>
HWY_API Vec256<T> AndNot(Vec256<T> not_mask, Vec256<T> mask) {
  const DFromV<decltype(mask)> d;  // for float16_t
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvandn_v(
                        BitCast(du, not_mask).raw, BitCast(du, mask).raw)});
}

// ------------------------------ Or

template <typename T>
HWY_API Vec256<T> Or(Vec256<T> a, Vec256<T> b) {
  const DFromV<decltype(a)> d;  // for float16_t
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{
                        __lasx_xvor_v(BitCast(du, a).raw, BitCast(du, b).raw)});
}

// ------------------------------ Xor

template <typename T>
HWY_API Vec256<T> Xor(Vec256<T> a, Vec256<T> b) {
  const DFromV<decltype(a)> d;  // for float16_t
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvxor_v(BitCast(du, a).raw,
                                                        BitCast(du, b).raw)});
}

// ------------------------------ Not
template <typename T>
HWY_API Vec256<T> Not(const Vec256<T> v) {
  const DFromV<decltype(v)> d;
  using TU = MakeUnsigned<T>;
  return Xor(v, BitCast(d, Vec256<TU>{__lasx_vreplgr2vr_w(-1)}));
}

// ------------------------------ Xor3
template <typename T>
HWY_API Vec256<T> Xor3(Vec256<T> x1, Vec256<T> x2, Vec256<T> x3) {
  return Xor(x1, Xor(x2, x3));
}

// ------------------------------ Or3
template <typename T>
HWY_API Vec256<T> Or3(Vec256<T> o1, Vec256<T> o2, Vec256<T> o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd
template <typename T>
HWY_API Vec256<T> OrAnd(Vec256<T> o, Vec256<T> a1, Vec256<T> a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ IfVecThenElse
template <typename T>
HWY_API Vec256<T> IfVecThenElse(Vec256<T> mask, Vec256<T> yes, Vec256<T> no) {
  return IfThenElse(MaskFromVec(mask), yes, no);
}

// ------------------------------ Operator overloads (internal-only if float)

template <typename T>
HWY_API Vec256<T> operator&(const Vec256<T> a, const Vec256<T> b) {
  return And(a, b);
}

template <typename T>
HWY_API Vec256<T> operator|(const Vec256<T> a, const Vec256<T> b) {
  return Or(a, b);
}

template <typename T>
HWY_API Vec256<T> operator^(const Vec256<T> a, const Vec256<T> b) {
  return Xor(a, b);
}

// ------------------------------ PopulationCount

namespace detail {

template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<1> /* tag */, Vec256<T> v) {
  return Vec256<T>{__lasx_xvpcnt_b(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<2> /* tag */, Vec256<T> v) {
  return Vec256<T>{__lasx_xvpcnt_h(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<4> /* tag */, Vec256<T> v) {
  return Vec256<T>{__lasx_xvpcnt_w(v.raw)};
}
template <typename T>
HWY_INLINE Vec256<T> PopulationCount(hwy::SizeTag<8> /* tag */, Vec256<T> v) {
  return Vec256<T>{__lasx_xvpcnt_d(v.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Vec256<T> PopulationCount(Vec256<T> v) {
  return detail::PopulationCount(hwy::SizeTag<sizeof(T)>(), v);
}

// ------------------------------ Mask

// Mask and Vec are the same (true = FF..FF).
template <typename T>
HWY_API Mask256<T> MaskFromVec(const Vec256<T> v) {
  return Mask256<T>{v.raw};
}

template <typename T>
HWY_API Vec256<T> VecFromMask(const Mask256<T> v) {
  return Vec256<T>{v.raw};
}

// ------------------------------ IfThenElse

// mask ? yes : no
namespace detail {

// Templates for a particular size.
template <typename T>
HWY_INLINE Vec256<T> XvstlMask(hwy::SizeTag<1> /* tag */, Mask256<T> mask) {
  return Vec256<T>{__lasx_xvstli_b(mask.raw, 0)};
}
template <typename T>
HWY_INLINE Vec256<T> XvstlMask(hwy::SizeTag<2> /* tag */, Mask256<T> mask) {
  return Vec256<T>{__lasx_xvstli_h(mask.raw, 0)};
}
template <typename T>
HWY_INLINE Vec256<T> XvstlMask(hwy::SizeTag<4> /* tag */, Mask256<T> mask) {
  return Vec256<T>{__lasx_xvstli_w(mask.raw, 0)};
}
template <typename T>
HWY_INLINE Vec256<T> XvstlMask(hwy::SizeTag<8> /* tag */, Mask256<T> mask) {
  return Vec256<T>{__lasx_xvstli_d(mask.raw, 0)};
}

}  // namespace detail

template <typename T>
HWY_INLINE Vec256<T> XvstlMask(Mask256<T> mask) {
  return detail::XvstlMask(hwy::SizeTag<sizeof(T)>(), mask);
}

template <typename T, HWY_IF_NOT_FLOAT3264(T)>
HWY_API Vec256<T> IfThenElse(Mask256<T> mask, Vec256<T> yes, Vec256<T> no) {
  return Vec256<T>{__lasx_xvbitsel_v(no.raw, yes.raw, XvstlMask(mask).raw)};
}

template <typename T, HWY_IF_FLOAT3264(T)>
HWY_API Vec256<T> IfThenElse(Mask256<T> mask, Vec256<T> yes, Vec256<T> no) {
  const DFromV<decltype(yes)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvbitsel_v(
                        BitCast(du, no).raw, BitCast(du, yes).raw,
                        XvstlMask(BitCast(du, mask)).raw)});
}

// mask ? yes : 0
template <typename T>
HWY_API Vec256<T> IfThenElseZero(Mask256<T> mask, Vec256<T> yes) {
  const DFromV<decltype(yes)> d;
  return yes & VecFromMask(d, mask);
}

// mask ? 0 : no
template <typename T>
HWY_API Vec256<T> IfThenZeroElse(Mask256<T> mask, Vec256<T> no) {
  const DFromV<decltype(no)> d;
  return AndNot(VecFromMask(d, mask), no);
}

template <typename T>
HWY_API Vec256<T> ZeroIfNegative(Vec256<T> v) {
  static_assert(IsSigned<T>(), "Only for float");
  const DFromV<decltype(v)> d;
  const auto zero = Zero(d);
  return IfThenElse(MaskFromVec(v), zero, v);
}

// ------------------------------ Mask logical

template <typename T>
HWY_API Mask256<T> Not(const Mask256<T> m) {
  const Full256<T> d;
  return MaskFromVec(Not(VecFromMask(d, m)));
}

template <typename T>
HWY_API Mask256<T> And(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(And(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> AndNot(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> Or(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(Or(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> Xor(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(Xor(VecFromMask(d, a), VecFromMask(d, b)));
}

template <typename T>
HWY_API Mask256<T> ExclusiveNeither(const Mask256<T> a, Mask256<T> b) {
  const Full256<T> d;
  return MaskFromVec(AndNot(VecFromMask(d, a), Not(VecFromMask(d, b))));
}

// ================================================== COMPARE

// Comparisons fill a lane with 1-bits if the condition is true, else 0.

template <class DTo, HWY_IF_V_SIZE_D(DTo, 32), typename TFrom>
HWY_API MFromD<DTo> RebindMask(DTo d_to, Mask256<TFrom> m) {
  static_assert(sizeof(TFrom) == sizeof(TFromD<DTo>), "Must have same size");
  const Full256<TFrom> dfrom;
  return MaskFromVec(BitCast(d_to, VecFromMask(dfrom, m)));
}

template <typename T>
HWY_API Mask256<T> TestBit(const Vec256<T> v, const Vec256<T> bit) {
  static_assert(!hwy::IsFloat<T>(), "Only integer vectors supported");
  return (v & bit) == bit;
}

// ------------------------------ Equality

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Mask256<T> operator==(Vec256<T> a, Vec256<T> b) {
  return Mask256<T>{__lasx_xvseq_b(a.raw, b.raw)};
}

template <typename T, HWY_IF_UI16(T)>
HWY_API Mask256<T> operator==(Vec256<T> a, Vec256<T> b) {
  return Mask256<T>{__lasx_xvseq_h(a.raw, b.raw)};
}

template <typename T, HWY_IF_UI32(T)>
HWY_API Mask256<T> operator==(Vec256<T> a, Vec256<T> b) {
  return Mask256<T>{__lasx_xvseq_w(a.raw, b.raw)};
}

template <typename T, HWY_IF_UI64(T)>
HWY_API Mask256<T> operator==(Vec256<T> a, Vec256<T> b) {
  return Mask256<T>{__lasx_xvseq_d(a.raw, b.raw)};
}

HWY_API Mask256<float> operator==(Vec256<float> a, Vec256<float> b) {
  // return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_EQ_OQ)};
  return Mask256<float>{__lasx_xvfcmp_ceq_s(a.raw, b.raw)};
}

HWY_API Mask256<double> operator==(Vec256<double> a, Vec256<double> b) {
  // return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_EQ_OQ)};
  return Mask256<double>{__lasx_xvfcmp_ceq_d(a.raw, b.raw)};
}

// ------------------------------ Inequality

template <typename T, HWY_IF_NOT_FLOAT3264(T)>
HWY_API Mask256<T> operator!=(Vec256<T> a, Vec256<T> b) {
  return Not(a == b);
}
HWY_API Mask256<float> operator!=(Vec256<float> a, Vec256<float> b) {
  // return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_NEQ_OQ)};
  return Mask256<float>{__lasx_xvfcmp_cne_s(a.raw, b.raw)};
}
HWY_API Mask256<double> operator!=(Vec256<double> a, Vec256<double> b) {
  // return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_NEQ_OQ)};
  return Mask256<double>{__lasx_xvfcmp_cne_d(a.raw, b.raw)};
}

// ------------------------------ Strict inequality

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

HWY_API Mask256<int8_t> Gt(hwy::SignedTag /*tag*/, Vec256<int8_t> a,
                           Vec256<int8_t> b) {
  return Mask256<int8_t>{__lasx_xvslt_b(b.raw, a.raw)};
}
HWY_API Mask256<int16_t> Gt(hwy::SignedTag /*tag*/, Vec256<int16_t> a,
                            Vec256<int16_t> b) {
  return Mask256<int16_t>{__lasx_xvslt_h(b.raw, a.raw)};
}
HWY_API Mask256<int32_t> Gt(hwy::SignedTag /*tag*/, Vec256<int32_t> a,
                            Vec256<int32_t> b) {
  return Mask256<int32_t>{__lasx_xvslt_w(b.raw, a.raw)};
}
HWY_API Mask256<int64_t> Gt(hwy::SignedTag /*tag*/, Vec256<int64_t> a,
                            Vec256<int64_t> b) {
  return Mask256<int64_t>{__lasx_xvslt_d(b.raw, a.raw)};
}

HWY_API Mask256<uint8_t> Gt(hwy::UnsignedTag /*tag*/, Vec256<uint8_t> a,
                            Vec256<uint8_t> b) {
  return Mask256<uint8_t>{__lasx_xvslt_bu(b.raw, a.raw)};
}
HWY_API Mask256<uint16_t> Gt(hwy::UnsignedTag /*tag*/, Vec256<uint16_t> a,
                             Vec256<uint16_t> b) {
  return Mask256<uint16_t>{__lasx_xvslt_hu(b.raw, a.raw)};
}
HWY_API Mask256<uint32_t> Gt(hwy::UnsignedTag /*tag*/, Vec256<uint32_t> a,
                             Vec256<uint32_t> b) {
  return Mask256<uint32_t>{__lasx_xvslt_wu(b.raw, a.raw)};
}
HWY_API Mask256<uint64_t> Gt(hwy::UnsignedTag /*tag*/, Vec256<uint64_t> a,
                             Vec256<uint64_t> b) {
  return Mask256<uint64_t>{__lasx_xvslt_du(b.raw, a.raw)};
}

HWY_API Mask256<float> Gt(hwy::FloatTag /*tag*/, Vec256<float> a,
                          Vec256<float> b) {
  // return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_GT_OQ)};
  return Mask256<float>{__lasx_xvfcmp_clt_s(b.raw, a.raw)};
}
HWY_API Mask256<double> Gt(hwy::FloatTag /*tag*/, Vec256<double> a,
                           Vec256<double> b) {
  // return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_GT_OQ)};
  return Mask256<double>{__lasx_xvfcmp_clt_d(b.raw, a.raw)};
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> operator>(Vec256<T> a, Vec256<T> b) {
  return detail::Gt(hwy::TypeTag<T>(), a, b);
}

// ------------------------------ Weak inequality

namespace detail {

template <typename T>
HWY_INLINE Mask256<T> Ge(hwy::SignedTag tag, Vec256<T> a, Vec256<T> b) {
  return Not(Gt(tag, b, a));
}

template <typename T>
HWY_INLINE Mask256<T> Ge(hwy::UnsignedTag tag, Vec256<T> a, Vec256<T> b) {
  return Not(Gt(tag, b, a));
}

HWY_INLINE Mask256<float> Ge(hwy::FloatTag /*tag*/, Vec256<float> a,
                             Vec256<float> b) {
  // return Mask256<float>{_mm256_cmp_ps(a.raw, b.raw, _CMP_GE_OQ)};
  return Not(Gt(tag, b, a));
}
HWY_INLINE Mask256<double> Ge(hwy::FloatTag /*tag*/, Vec256<double> a,
                              Vec256<double> b) {
  // return Mask256<double>{_mm256_cmp_pd(a.raw, b.raw, _CMP_GE_OQ)};
  return Not(Gt(tag, b, a));
}

}  // namespace detail

template <typename T>
HWY_API Mask256<T> operator>=(Vec256<T> a, Vec256<T> b) {
  return detail::Ge(hwy::TypeTag<T>(), a, b);
}

// ------------------------------ Reversed comparisons

template <typename T>
HWY_API Mask256<T> operator<(const Vec256<T> a, const Vec256<T> b) {
  return b > a;
}

template <typename T>
HWY_API Mask256<T> operator<=(const Vec256<T> a, const Vec256<T> b) {
  return b >= a;
}

// ------------------------------ Min (Gt, IfThenElse)

// Unsigned
HWY_API Vec256<uint8_t> Min(const Vec256<uint8_t> a, const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{__lasx_xvmin_bu(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> Min(const Vec256<uint16_t> a,
                             const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvmin_hu(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> Min(const Vec256<uint32_t> a,
                             const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{__lasx_xvmin_wu(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> Min(const Vec256<uint64_t> a,
                             const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{__lasx_xvmin_du(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> Min(const Vec256<int8_t> a, const Vec256<int8_t> b) {
  return Vec256<int8_t>{__lasx_xvmin_b(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> Min(const Vec256<int16_t> a, const Vec256<int16_t> b) {
  return Vec256<int16_t>{__lasx_xvmin_h(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> Min(const Vec256<int32_t> a, const Vec256<int32_t> b) {
  return Vec256<int32_t>{__lasx_xvmin_w(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> Min(const Vec256<int64_t> a, const Vec256<int64_t> b) {
  return Vec256<int64_t>{__lasx_xvmin_d(a.raw, b.raw)};
}

// Float
HWY_API Vec256<float> Min(const Vec256<float> a, const Vec256<float> b) {
  return IfThenElse(a < b, a, b);
}
HWY_API Vec256<double> Min(const Vec256<double> a, const Vec256<double> b) {
  return IfThenElse(a < b, a, b);
}

// ------------------------------ Max (Gt, IfThenElse)

// Unsigned
HWY_API Vec256<uint8_t> Max(const Vec256<uint8_t> a, const Vec256<uint8_t> b) {
  return Vec256<uint8_t>{__lasx_xvmax_bu(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> Max(const Vec256<uint16_t> a,
                             const Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvmax_hu(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> Max(const Vec256<uint32_t> a,
                             const Vec256<uint32_t> b) {
  return Vec256<uint32_t>{__lasx_xvmax_wu(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> Max(const Vec256<uint64_t> a,
                             const Vec256<uint64_t> b) {
  return Vec256<uint64_t>{__lasx_xvmax_du(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> Max(const Vec256<int8_t> a, const Vec256<int8_t> b) {
  return Vec256<int8_t>{__lasx_xvmax_b(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> Max(const Vec256<int16_t> a, const Vec256<int16_t> b) {
  return Vec256<int16_t>{__lasx_xvmax_h(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> Max(const Vec256<int32_t> a, const Vec256<int32_t> b) {
  return Vec256<int32_t>{__lasx_xvmax_w(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> Max(const Vec256<int64_t> a, const Vec256<int64_t> b) {
  return Vec256<int64_t>{__lasx_xvmax_d(a.raw, b.raw)};
}

// Float
HWY_API Vec256<float> Max(const Vec256<float> a, const Vec256<float> b) {
  return IfThenElse(a > b, a, b);
}
HWY_API Vec256<double> Max(const Vec256<double> a, const Vec256<double> b) {
  return IfThenElse(a > b, a, b);
}

// ------------------------------ Iota

namespace detail {

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 1)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  alignas(32) char rawI8[32] = {
      static_cast<char>(0),  static_cast<char>(1),  static_cast<char>(2),
      static_cast<char>(3),  static_cast<char>(4),  static_cast<char>(5),
      static_cast<char>(6),  static_cast<char>(7),  static_cast<char>(8),
      static_cast<char>(9),  static_cast<char>(10), static_cast<char>(11),
      static_cast<char>(12), static_cast<char>(13), static_cast<char>(14),
      static_cast<char>(15), static_cast<char>(16), static_cast<char>(17),
      static_cast<char>(18), static_cast<char>(19), static_cast<char>(20),
      static_cast<char>(21), static_cast<char>(22), static_cast<char>(23),
      static_cast<char>(24), static_cast<char>(25), static_cast<char>(26),
      static_cast<char>(27), static_cast<char>(28), static_cast<char>(29),
      static_cast<char>(30), static_cast<char>(31)};
  return VFromD<D>{__lasx_xvld(rawI8, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI16_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  alignas(32)
      int16_t rawI16[16] = {static_cast<int16_t>(0),  static_cast<int16_t>(1),
                            static_cast<int16_t>(2),  static_cast<int16_t>(3),
                            static_cast<int16_t>(4),  static_cast<int16_t>(5),
                            static_cast<int16_t>(6),  static_cast<int16_t>(7),
                            static_cast<int16_t>(8),  static_cast<int16_t>(9),
                            static_cast<int16_t>(10), static_cast<int16_t>(11),
                            static_cast<int16_t>(12), static_cast<int16_t>(13),
                            static_cast<int16_t>(14), static_cast<int16_t>(15)};
  return VFromD<D>{__lasx_xvld(rawI16, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI32_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  alignas(32) int32_t rawI32[8] = {
      static_cast<int32_t>(0), static_cast<int32_t>(1), static_cast<int32_t>(2),
      static_cast<int32_t>(3), static_cast<int32_t>(4), static_cast<int32_t>(5),
      static_cast<int32_t>(6), static_cast<int32_t>(7)};
  return VFromD<D>{__lasx_xvld(rawI32, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI64_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  alignas(32)
      int64_t rawI64[4] = {static_cast<int64_t>(0), static_cast<int64_t>(1),
                           static_cast<int64_t>(2), static_cast<int64_t>(3)};
  return VFromD<D>{__lasx_xvld(rawI64, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F32_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  alignas(32) float rawF32[8] = {0.0f, 1.0f, 2.0f, 3.0f,
                                 4.0f, 5.0f, 6.0f, 7.0f};
  return BitCast(D(), Vec256<int8_t>{__lasx_xvld(rawF32, 0)});
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_INLINE VFromD<D> Iota0(D /*d*/) {
  alignas(32) float rawF64[4] = {0.0f, 1.0f, 2.0f, 3.0f};
  return BitCast(D(), Vec256<int8_t>{__lasx_xvld(rawF64, 0)});
}

}  // namespace detail

template <class D, HWY_IF_V_SIZE_D(D, 32), typename T2>
HWY_API VFromD<D> Iota(D d, const T2 first) {
  return detail::Iota0(d) + Set(d, ConvertScalarTo<TFromD<D>>(first));
}

// ------------------------------ FirstN (Iota, Lt)

template <class D, HWY_IF_V_SIZE_D(D, 32), class M = MFromD<D>>
HWY_API M FirstN(const D d, size_t n) {
  constexpr size_t kN = MaxLanes(d);
  n = HWY_MIN(n, kN);
  const RebindToSigned<decltype(d)> di;  // Signed comparisons are cheaper.
  using TI = TFromD<decltype(di)>;
  return RebindMask(d, detail::Iota0(di) < Set(di, static_cast<TI>(n)));
}

// ================================================== ARITHMETIC

// ------------------------------ Addition

// Unsigned
HWY_API Vec256<uint8_t> operator+(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return Vec256<uint8_t>{__lasx_xvadd_b(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> operator+(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvadd_h(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator+(Vec256<uint32_t> a, Vec256<uint32_t> b) {
  return Vec256<uint32_t>{__lasx_xvadd_w(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> operator+(Vec256<uint64_t> a, Vec256<uint64_t> b) {
  return Vec256<uint64_t>{__lasx_xvadd_d(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> operator+(Vec256<int8_t> a, Vec256<int8_t> b) {
  return Vec256<int8_t>{__lasx_xvadd_b(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> operator+(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{__lasx_xvadd_h(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator+(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Vec256<int32_t>{__lasx_xvadd_w(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> operator+(Vec256<int64_t> a, Vec256<int64_t> b) {
  return Vec256<int64_t>{__lasx_xvadd_d(a.raw, b.raw)};
}

HWY_API Vec256<float> operator+(Vec256<float> a, Vec256<float> b) {
  return Vec256<float>{__lasx_xvfadd_s(a.raw, b.raw)};
}
HWY_API Vec256<double> operator+(Vec256<double> a, Vec256<double> b) {
  return Vec256<double>{__lasx_xvfadd_d(a.raw, b.raw)};
}

// ------------------------------ Subtraction

// Unsigned
HWY_API Vec256<uint8_t> operator-(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return Vec256<uint8_t>{__lasx_xvsub_b(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> operator-(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvsub_h(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator-(Vec256<uint32_t> a, Vec256<uint32_t> b) {
  return Vec256<uint32_t>{__laxs_xvsub_w(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> operator-(Vec256<uint64_t> a, Vec256<uint64_t> b) {
  return Vec256<uint64_t>{__lasx_xvsub_d(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> operator-(Vec256<int8_t> a, Vec256<int8_t> b) {
  return Vec256<int8_t>{__lasx_xvsub_b(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> operator-(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{__lasx_xvsub_h(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator-(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Vec256<int32_t>{__lasx_xvsub_w(a.raw, b.raw)};
}
HWY_API Vec256<int64_t> operator-(Vec256<int64_t> a, Vec256<int64_t> b) {
  return Vec256<int64_t>{__lasx_xvsub_d(a.raw, b.raw)};
}

HWY_API Vec256<float> operator-(Vec256<float> a, Vec256<float> b) {
  return Vec256<float>{__lasx_xvfsub_s(a.raw, b.raw)};
}
HWY_API Vec256<double> operator-(Vec256<double> a, Vec256<double> b) {
  return Vec256<double>{__lasx_xvfsub_d(a.raw, b.raw)};
}

// ------------------------------ AddSub

template <typename T, HWY_IF_FLOAT3264(T)>
HWY_API Vec256<T> AddSub(Vec256<T> a, Vec256<T> b) {
  const DFromV<decltype(a)> d;
  RebindToUnsigned<decltype(d)> du;
  const auto sub = a - b;
  const auto add = a + b;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvbitsel_v(BitCast(du, add).raw, BitCast(du, sub).raw, Set(du, 0xffff).raw)};
}

// ------------------------------ SumsOf8
HWY_API Vec256<uint64_t> SumsOf8(Vec256<uint8_t> v) {
  v.raw = __lasx_xvhaddw.hu.bu(v.raw, v.raw);
  v.raw = __lasx_xvhaddw.wu.hu(v.raw, v.raw);
  return Vec256<uint64_t>{__lasx_xvhaddw.du.wu(v.raw, v.raw)};
}

HWY_API Vec256<uint64_t> SumsOf8AbsDiff(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return SumsOf8(Vec256<uint8_t>{__lasx_xvabsd_bu(a.raw, b.raw)});
}

// ------------------------------ SaturatedAdd

// Returns a + b clamped to the destination range.

// Unsigned
HWY_API Vec256<uint8_t> SaturatedAdd(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return Vec256<uint8_t>{__lasx_xvsadd_bu(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> SaturatedAdd(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvsadd_hu(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> SaturatedAdd(Vec256<int8_t> a, Vec256<int8_t> b) {
  return Vec256<int8_t>{__lasx_xvsadd_b(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> SaturatedAdd(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{__lasx_xvsadd_h(a.raw, b.raw)};
}

// ------------------------------ SaturatedSub

// Returns a - b clamped to the destination range.

// Unsigned
HWY_API Vec256<uint8_t> SaturatedSub(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return Vec256<uint8_t>{__lasx_xvsub_bu(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> SaturatedSub(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvssub_hu(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int8_t> SaturatedSub(Vec256<int8_t> a, Vec256<int8_t> b) {
  return Vec256<int8_t>{__lasx_xvsub_b(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> SaturatedSub(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{__lasx_xvsub_h(a.raw, b.raw)};
}

// ------------------------------ Average

// Returns (a + b + 1) / 2

// Unsigned
HWY_API Vec256<uint8_t> AverageRound(Vec256<uint8_t> a, Vec256<uint8_t> b) {
  return Vec256<uint8_t>{__lasx_xvavgr_bu(a.raw, b.raw)};
}
HWY_API Vec256<uint16_t> AverageRound(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvavgr_hu(a.raw, b.raw)};
}

// ------------------------------ Abs (Sub)

// Returns absolute value, except that LimitsMin() maps to LimitsMax() + 1.
HWY_API Vec256<int8_t> Abs(Vec256<int8_t> v) {
  return Vec256<int8_t>{__lasx_xvabsd_b(v.raw, __lasx_xvreplgr2vr_b(0))};
}
HWY_API Vec256<int16_t> Abs(const Vec256<int16_t> v) {
  return Vec256<int16_t>{__lasx_xvabsd_h(v.raw, __lasx_xvreplgr2vr_h(0))};
}
HWY_API Vec256<int32_t> Abs(const Vec256<int32_t> v) {
  return Vec256<int32_t>{__lasx_xvabsd_w(v.raw, __lasx_xvreplgr2vr_w(0))};
}

// ------------------------------ Integer multiplication

// Unsigned
HWY_API Vec256<uint16_t> operator*(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvmul_h(a.raw, b.raw)};
}
HWY_API Vec256<uint32_t> operator*(Vec256<uint32_t> a, Vec256<uint32_t> b) {
  return Vec256<uint32_t>{__lasx_xvmul_w(a.raw, b.raw)};
}

// Signed
HWY_API Vec256<int16_t> operator*(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{__lasx_xvmul_h(a.raw, b.raw)};
}
HWY_API Vec256<int32_t> operator*(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Vec256<int32_t>{__lasx_xvmul_w(a.raw, b.raw)};
}

// Returns the upper 16 bits of a * b in each lane.
HWY_API Vec256<uint16_t> MulHigh(Vec256<uint16_t> a, Vec256<uint16_t> b) {
  return Vec256<uint16_t>{__lasx_xvmuh_h(a.raw, b.raw)};
}
HWY_API Vec256<int16_t> MulHigh(Vec256<int16_t> a, Vec256<int16_t> b) {
  return Vec256<int16_t>{__lasx_xvmuh_h(a.raw, b.raw)};
}

// Multiplies even lanes (0, 2 ..) and places the double-wide result into
// even and the upper half into its odd neighbor lane.
HWY_API Vec256<int64_t> MulEven(Vec256<int32_t> a, Vec256<int32_t> b) {
  return Vec256<int64_t>{__lasx_xvmulwev_d_w(a.raw, b.raw)};
}
HWY_API Vec256<uint64_t> MulEven(Vec256<uint32_t> a, Vec256<uint32_t> b) {
  return Vec256<uint64_t>{__lasx_xvmulwev_d_wu(a.raw, b.raw)};
}

HWY_API Vec256<int16_t> MulFixedPoint15(Vec256<int16_t> a, Vec256<int16_t> b) {
  const DFromV<decltype(a)> di16;
  const RepartitionToWide<decltype(di16)> di32;
  auto i32Prd = MulEven(a, b) + Set(di32, 0x4000);
  i32Prd.raw = __lasx_xvslli_w(i32Prd.raw, 1);
  return ResizeBitCast(di16, i32Prd);
}

// ------------------------------ ShiftLeft
// __lasx_xvslli_{b,h,w,d} ONLY accept real immediate const value, which means
// they will reject __lasx_xvslli_b{v, kBits} this way.  Gcc will generate one
// xvslli.{b,h,w,d} instruction for "<<" operator if the kBits is a realy
// immediate const value ,otherwise it will generate two instructions
// (xvreplgr2vr.{b,h,w,d} + vsll.{b,h,w,d}), but Clang will always generate two
// instructions(same with Gcc), so don't use intrinsic directly insted let
// compiler itself to generate instructions based on kBits at compile time,
// which will save one instruction for Gcc at least.  Same for ShiftRight.
template <int kBits>
HWY_API Vec256<uint8_t> ShiftLeft(Vec256<uint8_t> v) {
  using rawType = typename detail::Raw256<uint8_t>::type;
  return Vec256<uint8_t>{(rawType)((v32u8)v.raw << kBits)};
}

template <int kBits>
HWY_API Vec256<uint16_t> ShiftLeft(Vec256<uint16_t> v) {
  using rawType = typename detail::Raw256<uint16_t>::type;
  return Vec256<uint16_t>{(rawType)((v16u16)v.raw << kBits)};
}

template <int kBits>
HWY_API Vec256<uint32_t> ShiftLeft(Vec256<uint32_t> v) {
  using rawType = typename detail::Raw256<uint32_t>::type;
  return Vec256<uint32_t>{(rawType)((v8u32)v.raw << kBits)};
}

template <int kBits>
HWY_API Vec256<uint64_t> ShiftLeft(Vec256<uint64_t> v) {
  using rawType = typename detail::Raw256<uint64_t>::type;
  return Vec256<uint64_t>{(rawType)((v4u64)v.raw << kBits)};
}

template <int kBits>
HWY_API Vec256<int8_t> ShiftLeft(Vec256<int8_t> v) {
  using rawType = typename detail::Raw256<int8_t>::type;
  return Vec256<int8_t>{(rawType)((v32i8)v.raw << kBits)};
}

template <int kBits>
HWY_API Vec256<int16_t> ShiftLeft(Vec256<int16_t> v) {
  using rawType = typename detail::Raw256<int16_t>::type;
  return Vec256<int16_t>{(rawType)((v16i16)v.raw << kBits)};
}

template <int kBits>
HWY_API Vec256<int32_t> ShiftLeft(Vec256<int32_t> v) {
  using rawType = typename detail::Raw256<int32_t>::type;
  return Vec256<int32_t>{(rawType)((v8i32)v.raw << kBits)};
}

template <int kBits>
HWY_API Vec256<int64_t> ShiftLeft(Vec256<int64_t> v) {
  using rawType = typename detail::Raw256<int64_t>::type;
  return Vec256<int64_t>{(rawType)((v4i64)v.raw << kBits)};
}

// ------------------------------ ShiftRight

template <int kBits>
HWY_API Vec256<uint8_t> ShiftRight(Vec256<uint8_t> v) {
  using rawType = typename detail::Raw256<uint8_t>::type;
  return Vec256<uint8_t>{(rawType)((v32u8)v.raw >> kBits)};
}

template <int kBits>
HWY_API Vec256<uint16_t> ShiftRight(Vec256<uint16_t> v) {
  using rawType = typename detail::Raw256<uint16_t>::type;
  return Vec256<uint16_t>{(rawType)((v16u16)v.raw >> kBits)};
}

template <int kBits>
HWY_API Vec256<uint32_t> ShiftRight(Vec256<uint32_t> v) {
  using rawType = typename detail::Raw256<uint32_t>::type;
  return Vec256<uint32_t>{(rawType)((v8u32)v.raw >> kBits)};
}

template <int kBits>
HWY_API Vec256<uint64_t> ShiftRight(Vec256<uint64_t> v) {
  using rawType = typename detail::Raw256<uint64_t>::type;
  return Vec256<uint64_t>{(rawType)((v4u64)v.raw >> kBits)};
}

template <int kBits>
HWY_API Vec256<int8_t> ShiftRight(Vec256<int8_t> v) {
  using rawType = typename detail::Raw256<int8_t>::type;
  return Vec256<int8_t>{(rawType)((v32i8)v.raw >> kBits)};
}

template <int kBits>
HWY_API Vec256<int16_t> ShiftRight(Vec256<int16_t> v) {
  using rawType = typename detail::Raw256<int16_t>::type;
  return Vec256<int16_t>{(rawType)((v16i16)v.raw >> kBits)};
}

template <int kBits>
HWY_API Vec256<int32_t> ShiftRight(Vec256<int32_t> v) {
  using rawType = typename detail::Raw256<int32_t>::type;
  return Vec256<int32_t>{(rawType)((v8i32)v.raw >> kBits)};
}

template <int kBits>
HWY_API Vec256<int64_t> ShiftRight(Vec256<int64_t> v) {
  using rawType = typename detail::Raw256<int64_t>::type;
  return Vec256<int64_t>{(rawType)((v4i64)v.raw >> kBits)};
}

// ------------------------------ RotateRight

template <int kBits>
HWY_API Vec256<uint8_t> RotateRight(const Vec256<uint8_t> v) {
  static_assert(0 <= kBits && kBits < 8, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(7, 8 - kBits)>(v));
}

template <int kBits>
HWY_API Vec256<uint16_t> RotateRight(const Vec256<uint16_t> v) {
  static_assert(0 <= kBits && kBits < 16, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(15, 16 - kBits)>(v));
}

template <int kBits>
HWY_API Vec256<uint32_t> RotateRight(const Vec256<uint32_t> v) {
  static_assert(0 <= kBits && kBits < 32, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(31, 32 - kBits)>(v));
}

template <int kBits>
HWY_API Vec256<uint64_t> RotateRight(const Vec256<uint64_t> v) {
  static_assert(0 <= kBits && kBits < 64, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<HWY_MIN(63, 64 - kBits)>(v));
}

// ------------------------------ Rol/Ror
template <class T, HWY_IF_UI8(T)>
HWY_API Vec256<T> Ror(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{__lasx_xvrotr_b(a.raw, b.raw)};
}

template <class T, HWY_IF_UI8(T)>
HWY_API Vec256<T> Rol(Vec256<T> a, Vec256<T> b) {
  b.raw = __lasx_xvsub_b(__lasx_xvreplgr2vr_b(8), b.raw);
  return Vec256<T>{__lasx_xvrotr_b(a.raw, b.raw)};
}

template <class T, HWY_IF_UI16(T)>
HWY_API Vec256<T> Ror(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{__lasx_xvrotr_h(a.raw, b.raw)};
}

template <class T, HWY_IF_UI16(T)>
HWY_API Vec256<T> Rol(Vec256<T> a, Vec256<T> b) {
  b.raw = __lasx_xvsub_h(__lasx_xvreplgr2vr_h(16), b.raw);
  return Vec256<T>{__lasx_xvrotr_h(a.raw, b.raw)};
}

template <class T, HWY_IF_UI32(T)>
HWY_API Vec256<T> Ror(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{__lasx_xvrotr_w(a.raw, b.raw)};
}

template <class T, HWY_IF_UI32(T)>
HWY_API Vec256<T> Rol(Vec256<T> a, Vec256<T> b) {
  b.raw = __lasx_xvsub_w(__lasx_xvreplgr2vr_w(32), b.raw);
  return Vec256<T>{__lasx_xvrotr_w(a.raw, b.raw)};
}

template <class T, HWY_IF_UI64(T)>
HWY_API Vec256<T> Ror(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{__lasx_xvrotr_d(a.raw, b.raw)};
}

template <class T, HWY_IF_UI64(T)>
HWY_API Vec256<T> Rol(Vec256<T> a, Vec256<T> b) {
  b.raw = __lasx_xvsub_d(__lasx_xvreplgr2vr_d(32), b.raw);
  return Vec256<T>{__lasx_xvrotr_d(a.raw, b.raw)};
}

// ------------------------------ BroadcastSignBit (ShiftRight, compare, mask)

HWY_API Vec256<int8_t> BroadcastSignBit(const Vec256<int8_t> v) {
  return Vec256<int8_t>{__lasx_xvsrai_b(v.raw, 7)};
}

HWY_API Vec256<int16_t> BroadcastSignBit(const Vec256<int16_t> v) {
  return Vec256<int16_t>{__lasx_xvsrai_h(v.raw, 15)};
}

HWY_API Vec256<int32_t> BroadcastSignBit(const Vec256<int32_t> v) {
  return Vec256<int32_t>{__lasx_xvsrai_w(v.raw, 31)};
}

HWY_API Vec256<int64_t> BroadcastSignBit(const Vec256<int64_t> v) {
  return Vec256<int64_t>{__lasx_xvsrai_d(v.raw, 63)};
}

// ------------------------------ IfNegativeThenElse (BroadcastSignBit)
template <typename T>
HWY_API Vec256<T> IfNegativeThenElse(Vec256<T> v, Vec256<T> yes, Vec256<T> no) {
  static_assert(IsSigned<T>(), "Only works for signed/float");
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  const auto mask = MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
  return IfThenElse(mask, yes, no);
}

// ------------------------------ IfNegativeThenNegOrUndefIfZero

HWY_API Vec256<int8_t> IfNegativeThenNegOrUndefIfZero(Vec256<int8_t> mask,
                                                      Vec256<int8_t> v) {
  return IfNegativeThenElse(mask, Vec256<int8_t>{__lasx_xvneg_b(v.raw)}, v);
}

HWY_API Vec256<int16_t> IfNegativeThenNegOrUndefIfZero(Vec256<int16_t> mask,
                                                       Vec256<int16_t> v) {
  return IfNegativeThenElse(mask, Vec256<int16_t>{__lasx_xvneg_h(v.raw)}, v);
}

HWY_API Vec256<int32_t> IfNegativeThenNegOrUndefIfZero(Vec256<int32_t> mask,
                                                       Vec256<int32_t> v) {
  return IfNegativeThenElse(mask, Vec256<int32_t>{__lasx_xvneg_w(v.raw)}, v);
}

// ------------------------------ ShiftLeftSame

HWY_API Vec256<uint8_t> ShiftLeftSame(const Vec256<uint8_t> v, const int bits) {
  return Vec256<uint8_t>{__lasx_xvsll_b(v.raw, __lasx_xvreplgr2vr_b(bits))};
}

HWY_API Vec256<uint16_t> ShiftLeftSame(const Vec256<uint16_t> v,
                                       const int bits) {
  return Vec256<uint16_t>{__lasx_xvsll_h(v.raw, __lasx_xvreplgr2vr_h(bits))};
}

HWY_API Vec256<uint32_t> ShiftLeftSame(const Vec256<uint32_t> v,
                                       const int bits) {
  return Vec256<uint32_t>{__lasx_xvsll_w(v.raw, __lasx_xvreplgr2vr_w(bits))};
}

HWY_API Vec256<uint64_t> ShiftLeftSame(const Vec256<uint64_t> v,
                                       const int bits) {
  return Vec256<uint64_t>{__lasx_xvsll_d(v.raw, __lasx_xvreplgr2vr_d(bits))};
}

HWY_API Vec256<int8_t> ShiftLeftSame(const Vec256<int8_t> v, const int bits) {
  return Vec256<int8_t>{__lasx_xvsll_b(v.raw, __lasx_xvreplgr2vr_b(bits))};
}

HWY_API Vec256<int16_t> ShiftLeftSame(const Vec256<int16_t> v, const int bits) {
  return Vec256<int16_t>{__lasx_xvsll_h(v.raw, __lasx_xvreplgr2vr_h(bits))};
}

HWY_API Vec256<int32_t> ShiftLeftSame(const Vec256<int32_t> v, const int bits) {
  return Vec256<int32_t>{__lasx_xvsll_w(v.raw, __lasx_xvreplgr2vr_w(bits))};
}

HWY_API Vec256<int64_t> ShiftLeftSame(const Vec256<int64_t> v, const int bits) {
  return Vec256<int64_t>{__lasx_xvsll_d(v.raw, __lasx_xvreplgr2vr_d(bits))};
}

// ------------------------------ ShiftRightSame (BroadcastSignBit)

HWY_API Vec256<uint8_t> ShiftRightSame(const Vec256<uint8_t> v,
                                       const int bits) {
  return Vec256<uint8_t>{__lasx_xvsrl_b(v.raw, __lasx_xvreplgr2vr_b(bits))};
}

HWY_API Vec256<uint16_t> ShiftRightSame(const Vec256<uint16_t> v,
                                        const int bits) {
  return Vec256<uint16_t>{__lasx_xvsrl_h(v.raw, __lasx_xvreplgr2vr_h(bits))};
}

HWY_API Vec256<uint32_t> ShiftRightSame(const Vec256<uint32_t> v,
                                        const int bits) {
  return Vec256<uint32_t>{__lasx_xvsrl_w(v.raw, __lasx_xvreplgr2vr_w(bits))};
}

HWY_API Vec256<uint64_t> ShiftRightSame(const Vec256<uint64_t> v,
                                        const int bits) {
  return Vec256<uint64_t>{__lasx_xvsrl_d(v.raw, __lasx_xvreplgr2vr_d(bits))};
}

HWY_API Vec256<int8_t> ShiftRightSame(const Vec256<int8_t> v, const int bits) {
  return Vec256<int8_t>{__lasx_xvsra_b(v.raw, __lasx_xvreplgr2vr_b(bits))};
}

HWY_API Vec256<int16_t> ShiftRightSame(const Vec256<int16_t> v,
                                       const int bits) {
  return Vec256<int16_t>{__lasx_xvsra_h(v.raw, __lasx_xvreplgr2vr_h(bits))};
}

HWY_API Vec256<int32_t> ShiftRightSame(const Vec256<int32_t> v,
                                       const int bits) {
  return Vec256<int32_t>{__lasx_xvsra_w(v.raw, __lasx_xvreplgr2vr_w(bits))};
}

HWY_API Vec256<int64_t> ShiftRightSame(const Vec256<int64_t> v,
                                       const int bits) {
  return Vec256<int64_t>{__lasx_xvsra_d(v.raw, __lasx_xvreplgr2vr_d(bits))};
}

// ------------------------------ Neg (Xor, Sub)

// Tag dispatch instead of SFINAE for MSVC 2017 compatibility
namespace detail {

template <typename T>
HWY_INLINE Vec256<T> Neg(hwy::FloatTag /*tag*/, const Vec256<T> v) {
  const DFromV<decltype(v)> d;
  return Xor(v, SignBit(d));
}

template <typename T>
HWY_INLINE Vec256<T> Neg(hwy::SpecialTag /*tag*/, const Vec256<T> v) {
  const DFromV<decltype(v)> d;
  return Xor(v, SignBit(d));
}

// Not floating-point
template <typename T>
HWY_INLINE Vec256<T> Neg(hwy::SignedTag /*tag*/, const Vec256<T> v) {
  const DFromV<decltype(v)> d;
  return Zero(d) - v;
}

}  // namespace detail

template <typename T, HWY_IF_V_FLOAT_D<T>>
HWY_API Vec256<T> Neg(const Vec256<T> v) {
  return detail::Neg(hwy::TypeTag<T>(), v);
}

// ------------------------------ Floating-point mul / div

HWY_API Vec256<float> operator*(Vec256<float> a, Vec256<float> b) {
  return Vec256<float>{__lasx_xvfmul_s(a.raw, b.raw)};
}
HWY_API Vec256<double> operator*(Vec256<double> a, Vec256<double> b) {
  return Vec256<double>{__lasx_xvfmul_d(a.raw, b.raw)};
}

HWY_API Vec256<float> operator/(Vec256<float> a, Vec256<float> b) {
  return Vec256<float>{__lasx_xvfdiv_s(a.raw, b.raw)};
}
HWY_API Vec256<double> operator/(Vec256<double> a, Vec256<double> b) {
  return Vec256<double>{__lasx_xvfdiv_d(a.raw, b.raw)};
}

// Approximate reciprocal

HWY_API Vec256<float> ApproximateReciprocal(Vec256<float> v) {
  return Vec256<float>{__lasx_xvfrecip_s(v.raw)};
}

HWY_API Vec256<double> ApproximateReciprocal(Vec256<double> v) {
  return Vec256<double>{__lasx_xvfrecip_d(v.raw)};
}

// ------------------------------ Floating-point multiply-add variants

HWY_API Vec256<float> MulAdd(Vec256<float> mul, Vec256<float> x,
                             Vec256<float> add) {
  return Vec256<float>{__lasx_xvfmadd_s(mul.raw, x.raw, add.raw)};
}
HWY_API Vec256<double> MulAdd(Vec256<double> mul, Vec256<double> x,
                              Vec256<double> add) {
  return Vec256<double>{__lasx_xvfmadd_d(mul.raw, x.raw, add.raw)};
}

HWY_API Vec256<float> NegMulAdd(Vec256<float> mul, Vec256<float> x,
                                Vec256<float> add) {
  return add - mul * x;
}
HWY_API Vec256<double> NegMulAdd(Vec256<double> mul, Vec256<double> x,
                                 Vec256<double> add) {
  return add - mul * x;
}

HWY_API Vec256<float> MulSub(Vec256<float> mul, Vec256<float> x,
                             Vec256<float> sub) {
  return Vec256<float>{__lasx_xvfmsub_s(mul.raw, x.raw, sub.raw)};
}
HWY_API Vec256<double> MulSub(Vec256<double> mul, Vec256<double> x,
                              Vec256<double> sub) {
  return Vec256<double>{__lasx_xvfmsub_s(mul.raw, x.raw, sub.raw)};
}

HWY_API Vec256<float> NegMulSub(Vec256<float> mul, Vec256<float> x,
                                Vec256<float> sub) {
  return Vec256<float>{__lasx_xvfnmadd_s(mul.raw, x.raw, sub.raw)};
}
HWY_API Vec256<double> NegMulSub(Vec256<double> mul, Vec256<double> x,
                                 Vec256<double> sub) {
  return Vec256<double>{__lasx_xvfnmadd_d(mul.raw, x.raw, sub.raw)};
}

HWY_API Vec256<float> MulAddSub(Vec256<float> mul, Vec256<float> x,
                                Vec256<float> sub_or_add) {
  const auto vec1 = MulAdd(mul, x, sub_or_add);
  const auto vec2 = MulSub(mul, x, sub_or_add);
  const DFromV<decltype(x)> d;
  const RebindToUnSigned<decltype(d)> du;
  const RepartitionToWide<decltype(du)> dd;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvbitsel_v(
                        BitCast(du, vec2).raw, BitCast(du, vec1).raw,
                        BitCast(du, Set(dd, 0xffffffff00000000)).raw)});
}

HWY_API Vec256<double> MulAddSub(Vec256<double> mul, Vec256<double> x,
                                 Vec256<double> sub_or_add) {
  const auto vec1 = MulAdd(mul, x, sub_or_add);
  const auto vec2 = MulSub(mul, x, sub_or_add);
  const DFromV<decltype(x)> d;
  const RebindToUnSigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvextrins_d(
                        BitCast(du, vec2).raw, BitCast(du, vec1).raw, 0x11)});
}

// ------------------------------ Floating-point square root

// Full precision square root
HWY_API Vec256<float> Sqrt(Vec256<float> v) {
  return Vec256<float>{__lasx_xvfsqrt_s(v.raw)};
}

HWY_API Vec256<double> Sqrt(Vec256<double> v) {
  return Vec256<double>{__lasx_xvfsqrt_d(v.raw)};
}

// Approximate reciprocal square root
HWY_API Vec256<float> ApproximateReciprocalSqrt(Vec256<float> v) {
  return Vec256<float>{__lasx_xvfrsqrt_s(v.raw)};
}

HWY_API Vec256<double> ApproximateReciprocalSqrt(Vec256<double> v) {
  return Vec256<double>{__lasx_xvfrsqrt_d(v.raw)};
}

// ------------------------------ Floating-point rounding

// Toward nearest integer, tie to even
HWY_API Vec256<float> Round(Vec256<float> v) {
  return Vec256<float>{__lasx_xvftintrne_w_s(v.raw)};
}

HWY_API Vec256<double> Round(Vec256<double> v) {
  return Vec256<double>{__lasx_xvftintrne_l_d(v.raw)};
}

// Toward zero, aka truncate
HWY_API Vec256<float> Trunc(Vec256<float> v) {
  return Vec256<float>{__lasx_xvftintrz_w_s(v.raw)};
}

HWY_API Vec256<double> Trunc(Vec256<double> v) {
  return Vec256<double>{__lasx_xvftintrz_l_d(v.raw)};
}

// Toward +infinity, aka ceiling
HWY_API Vec256<float> Ceil(Vec256<float> v) {
  return Vec256<float>{__lasx_xvftintrp_w_s(v.raw)};
}

HWY_API Vec256<double> Ceil(Vec256<double> v) {
  return Vec256<double>{__lasx_xvftintrp_l_d(v.raw)};
}

// Toward -infinity, aka floor
HWY_API Vec256<float> Floor(Vec256<float> v) {
  return Vec256<float>{__lasx_xvftintrm_w_s(v.raw)};
}

HWY_API Vec256<double> Floor(Vec256<double> v) {
  return Vec256<double>{__lasx_xvftinrm_l_d(v.raw)};
}

// ------------------------------ Floating-point classification
HWY_API Mask256<float> IsNaN(Vec256<float> v) {
  return Mask256<float>{___lasx_xvfcmp_cun_s(v.raw, v.raw)};
}

HWY_API Mask256<double> IsNaN(Vec256<double> v) {
  return Mask256<double>{__lasx_xvfcmp_cun_d(v.raw, v.raw)};
}

HWY_API Mask256<float> IsEitherNaN(Vec256<float> a, Vec256<float> b) {
  return Mask256<float>{__lasx_xvfcmp_cun_s(a.raw, b.raw)};
}

HWY_API Mask256<double> IsEitherNaN(Vec256<double> a, Vec256<double> b) {
  return Mask256<double>{__lasx_xvfcmp_cun_d(a.raw, b.raw)};
}

// ================================================== MEMORY

// ------------------------------ Load

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> Load(D /* tag */, const TFromD<D>* HWY_RESTRICT aligned) {
  return VFromD<D>{__lasx_xvld(aligned, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> LoadU(D /* tag */, const TFromD<D>* HWY_RESTRICT p) {
  return VFromD<D>{__lasx_xvld(p, 0)};
}

// ------------------------------ MaskedLoad

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> MaskedLoad(MFromD<D> m, D d,
                             const TFromD<D>* HWY_RESTRICT p) {
  return IfThenElseZero(m, LoadU(d, p));
}

// ------------------------------ LoadDup128

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> LoadDup128(D d, const TFromD<D>* HWY_RESTRICT p) {
  detail::U256 u;
  u.i128[0] = u.i128[1] = __lsx_vld(p, 0);
  return VFromD<D>{Vec256<uint8_t>(u.i256)};
}

// ------------------------------ Store

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void Store(VFromD<D> v, D /* tag */, TFromD<D>* HWY_RESTRICT aligned) {
  __lasx_xvst(v.raw, aligned, 0);
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void StoreU(VFromD<D> v, D /* tag */, TFromD<D>* HWY_RESTRICT p) {
  __lasx_xvst(v.raw, p, 0);
}

// ------------------------------ BlendedStore
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void BlendedStore(VFromD<D> v, MFromD<D> m, D d,
                          TFromD<D>* HWY_RESTRICT p) {
  const RebindToUnsigned<decltype(d)> du;
  const auto blended =
      IfThenElse(RebindMask(du, m), BitCast(du, v), BitCast(du, LoadU(d, p)));
  StoreU(BitCast(d, blended), d, p);
}

// ------------------------------ Non-temporal stores
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void Stream(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT aligned) {
  __builtin_prefetch(aligned, 1, 0);
  Store(v, d, aligned);
}

// ------------------------------ ScatterOffset
// ------------------------------ Gather
// ------------------------------ MaskedGatherIndexOr

// ================================================== SWIZZLE
// ------------------------------ LowerHalf

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_NOT_FLOAT_NOR_SPECIAL_D(D)>
HWY_API VFromD<D> LowerHalf(D /* tag */, VFromD<Twice<D>> v) {
  detail::U256 u;
  u.i256 = v.raw;
  return VFromD<D>{u.i128[0]};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_BF16_D(D)>
HWY_API Vec128<bfloat16_t> LowerHalf(D /* tag */, Vec256<bfloat16_t> v) {
  detail::U256 u;
  u.i256 = v.raw;
  return Vec128<bfloat16_t>{u.i128[0]};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F16_D(D)>
HWY_API Vec128<float16_t> LowerHalf(D /* tag */, Vec256<float16_t> v) {
  detail::U256 u;
  u.i256 = v.raw;
  return Vec128<bfloat16_t>{u.i128[0]};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API Vec128<float> LowerHalf(D /* tag */, Vec256<float> v) {
  const RebindToUnsigned<decltype(d)> du;
  const Twice<decltype(du)> dut;
  detail::U256 u;
  u.i256 = BitCast(dut, v).raw;
  return BitCast(D(), VFromD<decltype(du)>{u.i128[0]});
  ;
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F64_D(D)>
HWY_API Vec128<double> LowerHalf(D /* tag */, Vec256<double> v) {
  const RebindToUnsigned<decltype(d)> du;
  const Twice<decltype(du)> dut;
  detail::U256 u;
  u.i256 = BitCast(dut, v).raw;
  return BitCast(D(), VFromD<decltype(du)>{u.i128[0]});
  ;
}

template <typename T>
HWY_API Vec128<T> LowerHalf(Vec256<T> v) {
  const Full128<T> dh;
  return LowerHalf(dh, v);
}

// ------------------------------ UpperHalf

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_NOT_FLOAT3264_D(D)>
HWY_API VFromD<D> UpperHalf(D d, VFromD<Twice<D>> v) {
  const RebindToUnsigned<decltype(d)> du;
  const Twice<decltype(du)> dut;
  detail::U256 u;
  u.i256 = BitCast(dut, v).raw;
  return BitCast(d, VFromD<decltype(du)>{u .128i [1]});
}

// ------------------------------ ExtractLane (Store)
template <typename T>
HWY_API T ExtractLane(const Vec256<T> v, size_t i) {
  const DFromV<decltype(v)> d;
  HWY_DASSERT(i < Lanes(d));

#if !HWY_IS_DEBUG_BUILD && HWY_COMPILER_GCC  // includes clang
  constexpr size_t kLanesPerBlock = 16 / sizeof(T);
  if (__builtin_constant_p(i < kLanesPerBlock) && (i < kLanesPerBlock)) {
    return ExtractLane(LowerHalf(Half<decltype(d)>(), v), i);
  }
#endif

  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, d, lanes);
  return lanes[i];
}

// ------------------------------ InsertLane (Store)
template <typename T>
HWY_API Vec256<T> InsertLane(const Vec256<T> v, size_t i, T t) {
  return detail::InsertLaneUsingBroadcastAndBlend(v, i, t);
}

// ------------------------------ GetLane (LowerHalf)
template <typename T>
HWY_API T GetLane(const Vec256<T> v) {
  return GetLane(LowerHalf(v));
}

// ------------------------------ ExtractBlock (LowerHalf, UpperHalf)

template <int kBlockIdx, class T>
HWY_API Vec128<T> ExtractBlock(Vec256<T> v) {
  static_assert(kBlockIdx == 0 || kBlockIdx == 1, "Invalid block index");
  const Half<DFromV<decltype(v)>> dh;
  return (kBlockIdx == 0) ? LowerHalf(dh, v) : UpperHalf(dh, v);
}

// ------------------------------ ZeroExtendVector

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_NOT_FLOAT_NOR_SPECIAL_D(D)>
HWY_API VFromD<D> ZeroExtendVector(D /* tag */, VFromD<Half<D>> lo) {
  detail::U256 u;
  u.i128[0] = lo.raw;
  u.i128[1] = Zero(Half<D>()).raw;
  return VFromD<D>{u.i256};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_FLOAT3264_D(D)>
HWY_API VFromD<D> ZeroExtendVector(D /* tag */, VFromD<Half<D>> lo) {
  detail::U256 u;
  const RebindToUnsigned<decltype(lo)> du128;
  const Twice<decltype(du128)> du128t;
  u.i128[0] = BitCast(du128, lo).raw;
  u.i128[1] = Zero(Half<D>()).raw;
  return BitCast(D(), VFromD<decltype(du128t)>(u.i256));
}

// ------------------------------ ZeroExtendResizeBitCast

namespace detail {

template <class DTo, class DFrom>
HWY_INLINE VFromD<DTo> ZeroExtendResizeBitCast(
    hwy::SizeTag<8> /* from_size_tag */, hwy::SizeTag<32> /* to_size_tag */,
    DTo d_to, DFrom d_from, VFromD<DFrom> v) {
  const Twice<decltype(d_from)> dt_from;
  const Twice<decltype(dt_from)> dq_from;
  return BitCast(d_to, ZeroExtendVector(dq_from, ZeroExtendVector(dt_from, v)));
}

}  // namespace detail

// ------------------------------ Combine

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_NOT_FLOAT3264_D(D)>
HWY_API VFromD<D> Combine(D d, VFromD<Half<D>> hi, VFromD<Half<D>> lo) {
  detail::U256 u;
  u.i128[0] = lo.raw;
  u.i128[1] = hi.raw;
  return VFromD<D>{u.i256};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_FLOAT3264_D(D)>
HWY_API VFromD<D> Combine(D d, VFromD<Half<D>> hi, VFromD<Half<D>> lo) {
  const RebindToUnsigned<decltype(d)> du;
  const Half<decltype(du)> du128;
  detail::U256 u;
  u.i128[0] = BitCast(du128, lo).raw;
  u.i128[1] = BitCast(du128, hi).raw;
  return BitCast(d, VFromD<decltype(du)>{u.i256});
}

// ------------------------------ ShiftLeftBytes
template <int kBytes, class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ShiftLeftBytes(D /* tag */, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  const Repartition<uint8_t, decltype(v)> d8;
// xvbsll.v Only accept immediate value for kBytes, same for xvbsrl.v
#define R(x) \
  BitCast(D(), Vec256<uint8_t>{__lasx_xvbsll_v(BitCast(d8, v).raw, (x))})
  switch (kBytes) {
    case 0:
      return v;
    case 1:
      return R(1);
    case 2:
      return R(2);
    case 3:
      return R(3);
    case 4:
      return R(4);
    case 5:
      return R(5);
    case 6:
      return R(6);
    case 7:
      return R(7);
    case 8:
      return R(8);
    case 9:
      return R(9);
    case 10:
      return R(10);
    case 11:
      return R(11);
    case 12:
      return R(12);
    case 13:
      return R(13);
    case 14:
      return R(14);
    case 15:
      return R(15);
    case 16:
      return R(16);
  }
#undef R
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ShiftRightBytes(D /* tag */, VFromD<D> v) {
  static_assert(0 <= kBytes && kBytes <= 16, "Invalid kBytes");
  const Repartition<uint8_t, decltype(v)> d8;
#define R(x) \
  BitCast(D(), Vec256<uint8_t>{__lasx_xvbsrl_v(BitCast(d8, v).raw, (x))})
  switch (kBytes) {
    case 0:
      return v;
    case 1:
      return R(1);
    case 2:
      return R(2);
    case 3:
      return R(3);
    case 4:
      return R(4);
    case 5:
      return R(5);
    case 6:
      return R(6);
    case 7:
      return R(7);
    case 8:
      return R(8);
    case 9:
      return R(9);
    case 10:
      return R(10);
    case 11:
      return R(11);
    case 12:
      return R(12);
    case 13:
      return R(13);
    case 14:
      return R(14);
    case 15:
      return R(15);
    case 16:
      return R(16);
  }
#undef R
}

// ------------------------------ CombineShiftRightBytes
template <int kBytes, class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> CombineShiftRightBytes(D d, VFromD<D> hi, VFromD<D> lo) {
  Or(ShiftRightBytes<kBytes>(d, lo), ShiftLeftBytes<16 - kBytes>(d, hi));
}  // 2025-02-28//TODO statistics for weekly report

// ------------------------------ Broadcast

template <int kLane, class T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec256<T> Broadcast(const Vec256<T> v) {
  static_assert(0 <= kLane && kLane < 16, "Invalid lane");
  return Vec256<T>{__lasx_xvreplve_b(v.raw, kLane)};
}

template <int kLane, typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec256<T> Broadcast(const Vec256<T> v) {
  static_assert(0 <= kLane && kLane < 8, "Invalid lane");
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{__lasx_xvreplve_h(BitCast(du, v).raw, kLane)});
}

template <int kLane, typename T, HWY_IF_UI32(T)>
HWY_API Vec256<T> Broadcast(const Vec256<T> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  return Vec256<T>{__lasx_xvreplve_w(v.raw, kLane)};
}

template <int kLane, typename T, HWY_IF_UI64(T)>
HWY_API Vec256<T> Broadcast(const Vec256<T> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  return Vec256<T>{__lasx_xvreplve_d(v.raw, kLane)};
}

template <int kLane>
HWY_API Vec256<float> Broadcast(Vec256<float> v) {
  static_assert(0 <= kLane && kLane < 4, "Invalid lane");
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{__lasx_xvreplve_w(BitCast(du, v).raw, kLane)});
}

template <int kLane>
HWY_API Vec256<double> Broadcast(const Vec256<double> v) {
  static_assert(0 <= kLane && kLane < 2, "Invalid lane");
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{__lasx_xvreplve_d(BitCast(du, v).raw, kLane)});
}

// ------------------------------ BroadcastBlock

template <int kBlockIdx, class T>
HWY_API Vec256<T> BroadcastBlock(Vec256<T> v) {
  static_assert(kBlockIdx == 0 || kBlockIdx == 1, "Invalid block index");
  const DFromV<decltype(v)> d;
  return (kBlockIdx == 0) ? ConcatLowerLower(d, v, v)
                          : ConcatUpperUpper(d, v, v);
}

// ------------------------------ BroadcastLane

namespace detail {

template <class T, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Vec256<T> BroadcastLane(hwy::SizeTag<0> /* lane_idx_tag */,
                                   Vec256<T> v) {
  return Vec256<T>{__lasx_xvreplve0_b(v.raw)};
}

template <class T, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Vec256<T> BroadcastLane(hwy::SizeTag<0> /* lane_idx_tag */,
                                   Vec256<T> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;  // for float16_t
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvreplve0_h(v.raw)});
}

template <class T, HWY_IF_UI32(T)>
HWY_INLINE Vec256<T> BroadcastLane(hwy::SizeTag<0> /* lane_idx_tag */,
                                   Vec256<T> v) {
  return Vec256<T>{__lasx_xvreplve0_w(v.raw)};
}

template <class T, HWY_IF_UI64(T)>
HWY_INLINE Vec256<T> BroadcastLane(hwy::SizeTag<0> /* lane_idx_tag */,
                                   Vec256<T> v) {
  return Vec256<T>{__lasx_xvreplve0_d(v.raw)};
}

HWY_INLINE Vec256<float> BroadcastLane(hwy::SizeTag<0> /* lane_idx_tag */,
                                       Vec256<float> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigend<decltype(d)> du;
  return BitCast(d,
                 VFromD<decltype(du)>{__lasx_xvreplve0_w(BitCast(du, v).raw)});
}

HWY_INLINE Vec256<double> BroadcastLane(hwy::SizeTag<0> /* lane_idx_tag */,
                                        Vec256<double> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigend<decltype(d)> du;
  return BitCast(d,
                 VFromD<decltype(du)>{__lasx_xvreplve0_d(BitCast(du, v).raw)});
}

template <size_t kLaneIdx, class T, hwy::EnableIf<kLaneIdx != 0>* = nullptr>
HWY_INLINE Vec256<T> BroadcastLane(hwy::SizeTag<kLaneIdx> /* lane_idx_tag */,
                                   Vec256<T> v) {
  constexpr size_t kLanesPerBlock = 16 / sizeof(T);
  constexpr int kBlockIdx = static_cast<int>(kLaneIdx / kLanesPerBlock);
  constexpr int kLaneInBlkIdx =
      static_cast<int>(kLaneIdx) & (kLanesPerBlock - 1);
  return Broadcast<kLaneInBlkIdx>(BroadcastBlock<kBlockIdx>(v));
}
}  // namespace detail

template <int kLaneIdx, class T>
HWY_API Vec256<T> BroadcastLane(Vec256<T> v) {
  static_assert(kLaneIdx >= 0, "Invalid lane");
  return detail::BroadcastLane(hwy::SizeTag<static_cast<size_t>(kLaneIdx)>(),
                               v);
}

// ------------------------------ Hard-coded shuffles

// Notation: let Vec256<int32_t> have lanes 7,6,5,4,3,2,1,0 (0 is
// least-significant). Shuffle0321 rotates four-lane blocks one lane to the
// right (the previous least-significant lane is now most-significant =>
// 47650321). These could also be implemented via CombineShiftRightBytes but
// the shuffle_abcd notation is more convenient.

// Swap 32-bit halves in 64-bit halves.
template <typename T, HWY_IF_UI32(T)>
HWY_API Vec256<T> Shuffle2301(const Vec256<T> v) {
  return Vec256<T>{__lasx_xvshuf4i_w(v.raw, 0xb1)};
}
HWY_API Vec256<float> Shuffle2301(const Vec256<float> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du){__lasx_xvshuf4i_w(BitCast(du, v).raw, 0xb1)}>);
}

// Used by generic_ops-inl.h
namespace detail {

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> ShuffleTwo2301(const Vec256<T> a, const Vec256<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_w(
                        BitCast(du, b).raw, BitCast(du, a).raw, 0xb1)});
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> ShuffleTwo1230(const Vec256<T> a, const Vec256<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_w(
                        BitCast(du, b).raw, BitCast(du, a).raw, 0x6c)});
}
template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> ShuffleTwo3012(const Vec256<T> a, const Vec256<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_w(
                        BitCast(du, b).raw, BitCast(du, a).raw, 0xc6)});
}

}  // namespace detail

// Swap 64-bit halves
HWY_API Vec256<uint32_t> Shuffle1032(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{__lasx_xvshuf4i_w(v.raw, 0x4e)};
}
HWY_API Vec256<int32_t> Shuffle1032(const Vec256<int32_t> v) {
  return Vec256<int32_t>{__lasx_xvshuf4i_w(v.raw, 0x4e)};
}
HWY_API Vec256<float> Shuffle1032(const Vec256<float> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  BitCast(d, VFromD<decltype(du)>{__lasx_xvshuf4i_w(BitCast(du, v).raw, 0x4e)});
}
HWY_API Vec256<uint64_t> Shuffle01(const Vec256<uint64_t> v) {
  return Vec256<uint64_t>{__lasx_xvshuf4i_w(v.raw, 0x4e)};
}
HWY_API Vec256<int64_t> Shuffle01(const Vec256<int64_t> v) {
  return Vec256<int64_t>{__lasx_xvshuf4i_w(v.raw, 0x4e)};
}
HWY_API Vec256<double> Shuffle01(const Vec256<double> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  BitCast(d, VFromD<decltype(du)>{__lasx_xvshuf4i_w(BitCast(du, v).raw, 0x4e)});
}

// Rotate right 32 bits
HWY_API Vec256<uint32_t> Shuffle0321(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{__lasx_xvshuf4i_w(v.raw, 0x39)};
}
HWY_API Vec256<int32_t> Shuffle0321(const Vec256<int32_t> v) {
  return Vec256<int32_t>{__lasx_xvshuf4i_w(v.raw, 0x39)};
}
HWY_API Vec256<float> Shuffle0321(const Vec256<float> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  BitCast(d, VFromD<decltype(du)>{__lasx_xvshuf4i_w(BitCast(du, v).raw, 0x39)});
}
// Rotate left 32 bits
HWY_API Vec256<uint32_t> Shuffle2103(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{__lasx_xvshuf4i_w(v.raw, 0x93)};
}
HWY_API Vec256<int32_t> Shuffle2103(const Vec256<int32_t> v) {
  return Vec256<int32_t>{__lasx_xvshuf4i_w(v.raw, 0x93)};
}
HWY_API Vec256<float> Shuffle2103(const Vec256<float> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  BitCast(d, VFromD<decltype(du)>{__lasx_xvshuf4i_w(BitCast(du, v).raw, 0x93)});
}

// Reverse
HWY_API Vec256<uint32_t> Shuffle0123(const Vec256<uint32_t> v) {
  return Vec256<uint32_t>{__lasx_xvshuf4i_w(v.raw, 0x1B)};
}
HWY_API Vec256<int32_t> Shuffle0123(const Vec256<int32_t> v) {
  return Vec256<int32_t>{__lasx_xvshuf4i_w(v.raw, 0x1B)};
}
HWY_API Vec256<float> Shuffle0123(const Vec256<float> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  BitCast(d, VFromD<decltype(du)>{__lasx_xvshuf4i_w(BitCast(du, v).raw, 0x1b)});
}

// ------------------------------ TableLookupLanes

// Returned by SetTableIndices/IndicesFromVec for use by TableLookupLanes.
template <typename T>
struct Indices256 {
  __m256i raw;
};

template <class D, HWY_IF_V_SIZE_D(D, 32), typename TI>
HWY_API Indices256<TFromD<D>> IndicesFromVec(D /* tag */, Vec256<TI> vec) {
  static_assert(sizeof(TFromD<D>) == sizeof(TI), "Index size must match lane");
#if HWY_IS_DEBUG_BUILD
  const Full256<TI> di;
  HWY_DASSERT(AllFalse(di, Lt(vec, Zero(di))) &&
              AllTrue(di, Lt(vec, Set(di, static_cast<TI>(2 * Lanes(di))))));
#endif
  return Indices256<TFromD<D>>{vec.raw};
}

// TODO
template <class D, HWY_IF_V_SIZE_D(D, 32), typename TI>
HWY_API Indices256<TFromD<D>> SetTableIndices(D d, const TI* idx) {
  const Rebind<TI, decltype(d)> di;
  return IndicesFromVec(d, LoadU(di, idx));
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec256<T> TableLookupLanes(Vec256<T> v, Indices256<T> idx) {
  const DFromV<decltype(v)> d;
  const auto a = ConcatLowerLower(d, v, v);
  const auto b = ConcatUpperUpper(d, v, v);
  return Vec256<T>{__lasx_xvshuf_b(b, a, idx.raw)};
}

template <typename T, HWY_IF_T_SIZE(T, 2), HWY_IF_NOT_SPECIAL_FLOAT(T)>
HWY_API Vec256<T> TableLookupLanes(Vec256<T> v, Indices256<T> idx) {
  const DFromV<decltype(v)> d;
  const auto a = ConcatLowerLower(d, v, v);
  const auto b = ConcatUpperUpper(d, v, v);
  return Vec256<T>{__lasx_xvshuf_h(b, a, idx.raw)};
}

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> TableLookupLanes(Vec256<T> v, Indices256<T> idx) {
  return Vec256<T>{__lasx_xvperm_w(v.raw, idx.raw)};
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec256<T> TableLookupLanes(Vec256<T> v, Indices256<T> idx) {
  const DFromV<decltype(v)> d;
  const auto a = ConcatLowerLower(d, v, v);
  const auto b = ConcatUpperUpper(d, v, v);
  return Vec256<T>{__lasx_xvshuf_d(b, a, idx.raw)};
}

HWY_API Vec256<float> TableLookupLanes(const Vec256<float> v,
                                       const Indices256<float> idx) {
  const Full256<float> df;
  const Full256<uint32_t> du;
  BitCast(df, Vec256<uint32_t>{__lasx_xvperm_w(BitCast(du, v).raw, idx.raw)});
}

HWY_API Vec256<double> TableLookupLanes(const Vec256<double> v,
                                        const Indices256<double> idx) {
  const Full256<double> df;
  const Full256<uint64_t> du;
  const auto a = ConcatLowerLower(d, v, v);
  const auto b = ConcatUpperUpper(d, v, v);
  return BitCast(df, Vec256<uint64_t>{__lasx_xvshuf_d(
                         BitCast(du, b).raw, BitCast(du, a), idx.raw)});
}

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec256<T> TwoTablesLookupLanes(Vec256<T> a, Vec256<T> b,
                                       Indices256<T> idx) {
  const DFromV<decltype(a)> d;
  const auto idx2 = Indices256<uint8_t>{__lasx_xvandi_b(idx.raw, 31)};
  const auto sel_hi_mask = MaskFromVec(ShiftLeft<2>{idx.raw});
  const auto lo_lookup_result = TableLookupLanes(a, idx);
  const auto hi_lookup_result = TableLookupLanes(b, idx2);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
}

template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec256<T> TwoTablesLookupLanes(Vec256<T> a, Vec256<T> b,
                                       Indices256<T> idx) {
  const DFromV<decltype(a)> d;
  const auto sel_hi_mask = MaskFromVec(ShiftLeft<11>{idx.raw});
  const auto lo_lookup_result = TableLookupLanes(a, idx);
  const auto hi_lookup_result = TableLookupLanes(b, idx2);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
}

template <typename T, HWY_IF_UI32(T)>
HWY_API Vec256<T> TwoTablesLookupLanes(Vec256<T> a, Vec256<T> b,
                                       Indices256<T> idx) {
  const DFromV<decltype(a)> d;
  const RebindToFloat<decltype(d)> df;
  const Vec256<T> idx_vec{idx.raw};

  const auto sel_hi_mask = MaskFromVec(BitCast(df, ShiftLeft<28>(idx_vec)));
  const auto lo_lookup_result = BitCast(df, TableLookupLanes(a, idx));
  const auto hi_lookup_result = BitCast(df, TableLookupLanes(b, idx));
  return BitCast(d,
                 IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result));
}

HWY_API Vec256<float> TwoTablesLookupLanes(Vec256<float> a, Vec256<float> b,
                                           Indices256<float> idx) {
  const DFromV<decltype(a)> d;
  const auto sel_hi_mask =
      MaskFromVec(BitCast(d, ShiftLeft<28>(Vec256<uint32_t>{idx.raw})));
  const auto lo_lookup_result = TableLookupLanes(a, idx);
  const auto hi_lookup_result = TableLookupLanes(b, idx);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
}

template <typename T, HWY_IF_UI64(T)>
HWY_API Vec256<T> TwoTablesLookupLanes(Vec256<T> a, Vec256<T> b,
                                       Indices256<T> idx) {
  const DFromV<decltype(a)> d;
  const auto sel_hi_mask =
      MaskFromVec(BitCast(d, ShiftLeft<61>(Vec256<uint64_t>{idx.raw})));
  const auto lo_lookup_result = TableLookupLanes(a, idx);
  const auto hi_lookup_result = TableLookupLanes(b, idx);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
}

HWY_API Vec256<double> TwoTablesLookupLanes(Vec256<double> a, Vec256<double> b,
                                            Indices256<double> idx) {
  const DFromV<decltype(a)> d;
  const auto sel_hi_mask =
      MaskFromVec(BitCast(d, ShiftLeft<61>(Vec256<uint64_t>{idx.raw})));
  const auto lo_lookup_result = TableLookupLanes(a, idx);
  const auto hi_lookup_result = TableLookupLanes(b, idx);
  return IfThenElse(sel_hi_mask, hi_lookup_result, lo_lookup_result);
}

// ------------------------------ SwapAdjacentBlocks

template <typename T>
HWY_API Vec256<T> SwapAdjacentBlocks(Vec256<T> v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, Vec256<uint8_t>{__lasx_xvpermi_q(v, v, 0x01)});
}

// ------------------------------ Reverse (RotateRight)

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Reverse(D d, const VFromD<D> v) {
  alignas(32) static constexpr int32_t kReverse[8] = {7, 6, 5, 4, 3, 2, 1, 0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Reverse(D d, const VFromD<D> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{__lasx_xvpermi_d(BitCast(du, v).raw, 0x1b)});
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse(D d, const VFromD<D> v) {
  alignas(32) static constexpr int16_t kReverse[16] = {
      15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> Reverse(D d, const VFromD<D> v) {
  alignas(32) static constexpr TFromD<D> kReverse[32] = {
      31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16,
      15, 14, 13, 12, 11, 10, 9,  8,  7,  6,  5,  4,  3,  2,  1,  0};
  return TableLookupLanes(v, SetTableIndices(d, kReverse));
}

// ------------------------------ Reverse2 (in x86_128)

// ------------------------------ Reverse4 (SwapAdjacentBlocks)

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse4(D d, const VFromD<D> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{__lasx_xvshuf4i_h(BitCast(du, v).raw, 0x1b)});
}

// 32 bit Reverse4 defined in x86_128.

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Reverse4(D /* tag */, const VFromD<D> v) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(
      d, VFromD<decltype(du)>{__lasx_xvpermi_d(BitCast(du, v).raw, 0x1b)});
}

// ------------------------------ Reverse8

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Reverse8(D d, const VFromD<D> v) {
  const RebindToSigned<decltype(d)> di;
  const VFromD<decltype(di)> shuffle = Dup128VecFromValues(
      di, 0x0F0E, 0x0D0C, 0x0B0A, 0x0908, 0x0706, 0x0504, 0x0302, 0x0100);
  return BitCast(d, TableLookupBytes(v, shuffle));
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Reverse8(D d, const VFromD<D> v) {
  return Reverse(d, v);
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Reverse8(D /* tag */, const VFromD<D> /* v */) {
  HWY_ASSERT(0);  // AVX2 does not have 8 64-bit lanes
}

// ------------------------------ ReverseBits in x86_512

// ------------------------------ InterleaveLower

// Interleaves lanes from halves of the 128-bit blocks of "a" (which provides
// the least-significant lane) and "b". To concatenate two half-width integers
// into one, use ZipLower/Upper instead (also works with scalar).

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec256<T> InterleaveLower(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{__lasx_xvilvl_b(b.raw, a.raw)};
}
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec256<T> InterleaveLower(Vec256<T> a, Vec256<T> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;  // for float16_t
  return BitCast(d,
                 VU{__lasx_xvilvl_h(BitCast(du, b).raw, BitCast(du, a).raw)});
}
template <typename T, HWY_IF_UI32(T)>
HWY_API Vec256<T> InterleaveLower(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{__lasx_xvilvl_w(b.raw, a.raw)};
}
template <typename T, HWY_IF_UI64(T)>
HWY_API Vec256<T> InterleaveLower(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{__lasx_xvilvl_d(b.raw, a.raw)};
}

HWY_API Vec256<float> InterleaveLower(Vec256<float> a, Vec256<float> b) {
  const Full256<uint32_t> du;
  const Full256<float> df;
  return BitCast(df, Full256<uint32_t>{__lasx_xvilvl_w(BitCast(du, b).raw,
                                                       BitCast(du, a).raw)});
}
HWY_API Vec256<double> InterleaveLower(Vec256<double> a, Vec256<double> b) {
  const Full256<uint64_t> du;
  const Full256<double> df;
  return BitCast(df, Full256<uint64_t>{__lasx_xvilvl_d(BitCast(du, b).raw,
                                                       BitCast(du, a).raw)});
}

// ------------------------------ InterleaveUpper

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> InterleaveUpper(D /* tag */, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lasx_xvilvh_b(b.raw, a.raw)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> InterleaveUpper(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToUnsigned<decltype(d)> du;
  using VU = VFromD<decltype(du)>;  // for float16_t
  return BitCast(d,
                 VU{__lasx_xvilvh_h(BitCast(du, b).raw, BitCast(du, a).raw)});
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> InterleaveUpper(D /* tag */, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lasx_xvilvh_w(b.raw, a.raw)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> InterleaveUpper(D /* tag */, VFromD<D> a, VFromD<D> b) {
  return VFromD<D>{__lasx_xvilvh_d(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F32_D(D)>
HWY_API VFromD<D> InterleaveUpper(D /* tag */, VFromD<D> a, VFromD<D> b) {
  const RebindToUnsigned<D> du;
  return BitCast(D(), VFromD<decltype(du)>{__lasx_xvilvh_w(
                          BitCast(du, b).raw, BitCast(du, a).raw)});
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API VFromD<D> InterleaveUpper(D /* tag */, VFromD<D> a, VFromD<D> b) {
  const RebindToUnsigned<D> du;
  return BitCast(D(), VFromD<decltype(du)>{__lasx_xvilvh_d(
                          BitCast(du, b).raw, BitCast(du, a).raw)});
}

// ------------------------------ Blocks (LowerHalf, ZeroExtendVector)

// hiH,hiL loH,loL |-> hiL,loL (= lower halves)
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ConcatLowerLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_q(
                        BitCast(du, hi).raw, BitCast(du, lo).raw, 0x20)});
}

// hiH,hiL loH,loL |-> hiL,loH (= inner halves / swap blocks)
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ConcatLowerUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_q(
                        BitCast(du, hi).raw, BitCast(du, lo).raw, 0x21)});
}

// hiH,hiL loH,loL |-> hiH,loL (= outer halves)
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ConcatUpperLower(D d, VFromD<D> hi, VFromD<D> lo) {
  const RebindToUnsigned<decltype(d)> du;  // for float16_t
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_q(
                        BitCast(du, hi).raw, BitCast(du, lo).raw, 0x30)});
}

// hiH,hiL loH,loL |-> hiH,loH (= upper halves)
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ConcatUpperUpper(D d, VFromD<D> hi, VFromD<D> lo) {
  const RebindToUnsigned<decltype(d)> du;  // for float16_t
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_q(
                        BitCast(du, hi).raw, BitCast(du, lo).raw, 0x31)});
}

// ---------------------------- InsertBlock (ConcatLowerLower, ConcatUpperLower)
template <int kBlockIdx, class T>
HWY_API Vec256<T> InsertBlock(Vec256<T> v, Vec128<T> blk_to_insert) {
  static_assert(kBlockIdx == 0 || kBlockIdx == 1, "Invalid block index");

  const DFromV<decltype(v)> d;
  const auto vec_to_insert = ResizeBitCast(d, blk_to_insert);
  return (kBlockIdx == 0) ? ConcatUpperLower(d, v, vec_to_insert)
                          : ConcatLowerLower(d, vec_to_insert, v);
}

// ------------------------------ ConcatOdd

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
  __m256i od = __lasx_xvpickod_b(hi.raw, lo.raw);
  return VFromD<D>{__lasx_xvpermi_d(od, 0xd8)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
  const RebindToUnsigned<decltype(d)> du;
  __m256i od = __lasx_xvpickod_h(BitCast(du, hi).raw, BitCast(du, lo).raw);
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_d(od, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
  __m256i od = __lasx_xvpickod_w(hi.raw, lo.raw);
  return VFromD<D>{__lasx_xvpermi_d(od, 0xd8)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
  const RebindToUnsigned<decltype(d)> du;
  __m256i od = __lasx_xvpickod_w(BitCast(du, hi).raw, BitCast(du, lo).raw);
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_d(od, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> ConcatOdd(D d, VFromD<D> hi, VFromD<D> lo) {
  __m256i od = __lasx_xvpickod_d(hi.raw, lo.raw);
  return VFromD<D>{__lasx_xvpermi_d(od, 0xd8)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API Vec256<double> ConcatOdd(D d, Vec256<double> hi, Vec256<double> lo) {
  const RebindToUnsigned<decltype(d)> du;
  __m256i od = __lasx_xvpickod_d(BitCast(du, hi).raw, BitCast(du, lo).raw);
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_d(od, 0xd8)});
}

// ------------------------------ ConcatEven

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
  __m256i ev = __lasx_xvpickev_b(hi.raw, lo.raw);
  return VFromD<D>{__lasx_xvpermi_d(ev, 0xd8)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
  const RebindToUnsigned<decltype(d)> du;
  __m256i ev = __lasx_xvpickev_h(BitCast(du, hi).raw, BitCast(du, lo).raw);
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_d(ev, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
  __m256i ev = __lasx_xvpickev_w(hi.raw, lo.raw);
  return VFromD<D>{__lasx_xvpermi_d(ev, 0xd8)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
  const RebindToUnsigned<decltype(d)> du;
  __m256i ev = __lasx_xvpickev_w(BitCast(du, hi).raw, BitCast(du, lo).raw);
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_d(ev, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI64_D(D)>
HWY_API VFromD<D> ConcatEven(D d, VFromD<D> hi, VFromD<D> lo) {
  __m256i ev = __lasx_xvpickev_d(hi.raw, lo.raw);
  return VFromD<D>{__lasx_xvpermi_d(ev, 0xd8)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API Vec256<double> ConcatEven(D d, Vec256<double> hi, Vec256<double> lo) {
  const RebindToUnsigned<decltype(d)> du;
  __m256i ev = __lasx_xvpickev_d(BitCast(du, hi).raw, BitCast(du, lo).raw);
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_d(ev, 0xd8)});
}

// ------------------------------ InterleaveWholeLower

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> InterleaveWholeLower(D d, VFromD<D> a, VFromD<D> b) {
  return ConcatLowerLower(d, InterleaveUpper(d, a, b), InterleaveLower(a, b));
}

// ------------------------------ InterleaveWholeUpper

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> InterleaveWholeUpper(D d, VFromD<D> a, VFromD<D> b) {
  return ConcatUpperUpper(d, InterleaveUpper(d, a, b), InterleaveLower(a, b));
}

// ------------------------------ DupEven (InterleaveLower)

template <typename T, HWY_IF_UI32(T)>
HWY_API Vec256<T> DupEven(Vec256<T> v) {
  return Vec256<T>{__lasx_xvpackev_w(v.raw, v.raw)};
}
HWY_API Vec256<float> DupEven(Vec256<float> v) {
  const Full256<uint32_t> du;
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec256<uint32_t>{__lasx_xvpackev_w(BitCast(du, v).raw,
                                                       BitCast(du, v).raw)});
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec256<T> DupEven(const Vec256<T> v) {
  const DFromV<decltype(v)> d;
  return InterleaveLower(d, v, v);
}

// ------------------------------ DupOdd (InterleaveUpper)

template <typename T, HWY_IF_UI32(T)>
HWY_API Vec256<T> DupOdd(Vec256<T> v) {
  return Vec256<T>{__lasx_xvpackod_w(v.raw, v.raw)};
}
HWY_API Vec256<float> DupOdd(Vec256<float> v) {
  const Full256<uint32_t> du;
  const DFromV<decltype(v)> d;
  return BitCast(d, Vec256<uint32_t>{__lasx_xvpackod_w(BitCast(du, v).raw,
                                                       BitCast(du, v).raw)});
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec256<T> DupOdd(const Vec256<T> v) {
  const DFromV<decltype(v)> d;
  return InterleaveUpper(d, v, v);
}

// ------------------------------ OddEven

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Vec256<T> OddEven(Vec256<T> a, Vec256<T> b) {
  __m256i c = __lasx_xvpackod_b(a.raw, a.raw);
  return Vec256<T>{__lasx_xvpackev_b(c, b.raw)};
}

template <typename T, HWY_IF_UI16(T)>
HWY_INLINE Vec256<T> OddEven(Vec256<T> a, Vec256<T> b) {
  __m256i c = __lasx_xvpackod_h(a.raw, a.raw);
  return Vec256<T>{__lasx_xvpackev_h(c, b.raw)};
}

template <typename T, HWY_IF_UI32(T)>
HWY_INLINE Vec256<T> OddEven(Vec256<T> a, Vec256<T> b) {
  __m256i c = __lasx_xvpackod_w(a.raw, a.raw);
  return Vec256<T>{__lasx_xvpackev_w(c, b.raw)};
}

template <typename T, HWY_IF_UI64(T)>
HWY_INLINE Vec256<T> OddEven(Vec256<T> a, Vec256<T> b) {
  return Vec256<T>{__lasx_xvextrins_d(b.raw, a.raw, 0x01)};
}

HWY_API Vec256<float> OddEven(Vec256<float> a, Vec256<float> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  __m256i c = __lasx_xvpackod_w(BitCast(du, a).raw, BitCast(du, a).raw);
  return BitCast(
      d, VFromD<decltype(du)>{__lasx_xvpackev_w(c, BitCast(du, b).raw)});
}

HWY_API Vec256<double> OddEven(Vec256<double> a, Vec256<double> b) {
  const DFromV<decltype(a)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvextrins_d(
                        BitCast(du, b).raw, BitCast(du, a).raw, 0x01)});
}

// -------------------------- InterleaveEven

template <class D, HWY_IF_LANES_D(D, 8), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> InterleaveEven(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpackev_w(
                        BitCast(du, b).raw, BitCast(du, a).raw)});
}

// I64/U64/F64 InterleaveEven is generic for vector lengths >= 32 bytes
template <class D, HWY_IF_LANES_GT_D(D, 2), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> InterleaveEven(D /*d*/, VFromD<D> a, VFromD<D> b) {
  return InterleaveLower(a, b);
}

// -------------------------- InterleaveOdd

template <class D, HWY_IF_LANES_D(D, 8), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> InterleaveOdd(D d, VFromD<D> a, VFromD<D> b) {
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpackod_w(
                        BitCast(du, b).raw, BitCast(du, a).raw)});
}

// I64/U64/F64 InterleaveOdd is generic for vector lengths >= 32 bytes
template <class D, HWY_IF_LANES_GT_D(D, 2), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> InterleaveOdd(D d, VFromD<D> a, VFromD<D> b) {
  return InterleaveUpper(d, a, b);
}

// ------------------------------ OddEvenBlocks

template <typename T>
Vec256<T> OddEvenBlocks(Vec256<T> odd, Vec256<T> even) {
  const DFromV<decltype(odd)> d;
  const RebindToUnsigned<decltype(d)> du;
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvpermi_q(
                        BitCast(du, odd), BitCast(du, even).raw, 0x30)});
}

// ------------------------------ ReverseBlocks (SwapAdjacentBlocks)

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> ReverseBlocks(D /*d*/, VFromD<D> v) {
  return SwapAdjacentBlocks(v);
}

// ------------------------------ TableLookupBytes (ZeroExtendVector)

// Both full
template <typename T, typename TI>
HWY_API Vec256<TI> TableLookupBytes(Vec256<T> bytes, Vec256<TI> from) {
  const DFromV<decltype(from)> d;
  return BitCast(d, Vec256<uint8_t>{__lasx_xvshuf_b(BitCast(Full256<uint8_t>(), bytes).raw,
                                                    BitCast(Full256<uint8_t>(), bytes).raw,
                                                    BitCast(Full256<uint8_t>(), from).raw});
}

// Partial index vector
template <typename T, typename TI, size_t NI>
HWY_API Vec128<TI, NI> TableLookupBytes(Vec256<T> bytes, Vec128<TI, NI> from) {
  const Full256<TI> di;
  const Half<decltype(di)> dih;
  // First expand to full 128, then 256.
  const auto from_256 = ZeroExtendVector(di, Vec128<TI>{from.raw});
  const auto tbl_full = TableLookupBytes(bytes, from_256);
  // Shrink to 128, then partial.
  return Vec128<TI, NI>{LowerHalf(dih, tbl_full).raw};
}

// Partial table vector
template <typename T, size_t N, typename TI>
HWY_API Vec256<TI> TableLookupBytes(Vec128<T, N> bytes, Vec256<TI> from) {
  const Full256<T> d;
  // First expand to full 128, then 256.
  const auto bytes_256 = ZeroExtendVector(d, Vec128<T>{bytes.raw});
  return TableLookupBytes(bytes_256, from);
}

// ------------------------------ Per4LaneBlockShuffle

namespace detail {

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_INLINE VFromD<D> Per4LaneBlkShufDupSet4xU32(D d, const uint32_t x3,
                                                const uint32_t x2,
                                                const uint32_t x1,
                                                const uint32_t x0) {
  return Dup128VecFromValues(d, x0, x1, x2, x3);
}

template <size_t kIdx3210, class V, HWY_IF_NOT_FLOAT(TFromV<V>)>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<kIdx3210> /*idx_3210_tag*/,
                                  hwy::SizeTag<4> /*lane_size_tag*/,
                                  hwy::SizeTag<32> /*vect_size_tag*/, V v) {
  const DFromV<decltype(v)> d;
  V idx = Per4LaneBlkShufDupSet4xU32(d, kIdx3210 & 3, (kIdx3210 >> 2) & 3,
                                     (kIdx3210 >> 4) & 3, (kIdx3210 >> 6) & 3);
  return V{__lasx_xvshuf_w(v.raw, v.raw, idx)};
}

template <size_t kIdx3210, class V, HWY_IF_FLOAT(TFromV<V>)>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<kIdx3210> /*idx_3210_tag*/,
                                  hwy::SizeTag<4> /*lane_size_tag*/,
                                  hwy::SizeTag<32> /*vect_size_tag*/, V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  V idx = Per4LaneBlkShufDupSet4xU32(du, kIdx3210 & 3, (kIdx3210 >> 2) & 3,
                                     (kIdx3210 >> 4) & 3, (kIdx3210 >> 6) & 3);
  return BitCast(d, VFromD<decltype(du)>{__lasx_xvshuf_w(
                        BitCast(du, v).raw, BitCast(du, v).raw, idx)});
}

template <class V>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<0x44> /*idx_3210_tag*/,
                                  hwy::SizeTag<8> /*lane_size_tag*/,
                                  hwy::SizeTag<32> /*vect_size_tag*/, V v) {
  const DFromV<decltype(v)> d;
  return ConcatLowerLower(d, v, v);
}

template <class V>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<0xEE> /*idx_3210_tag*/,
                                  hwy::SizeTag<8> /*lane_size_tag*/,
                                  hwy::SizeTag<32> /*vect_size_tag*/, V v) {
  const DFromV<decltype(v)> d;
  return ConcatUpperUpper(d, v, v);
}

template <size_t kIdx3210, class V, HWY_IF_NOT_FLOAT(TFromV<V>)>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<kIdx3210> /*idx_3210_tag*/,
                                  hwy::SizeTag<8> /*lane_size_tag*/,
                                  hwy::SizeTag<32> /*vect_size_tag*/, V v) {
  const DFromV<decltype(v)> d;
  alignas(32) uint64_t idx[4] = {kIdx3210 & 3, (kIdx3210 >> 2) & 3,
                                 (kIdx3210 >> 4) & 3, (kIdx3210 >> 6) & 3};
  return TableLookupLanes(v, SetTableIndices(d, idx));
}

template <size_t kIdx3210, class V, HWY_IF_FLOAT(TFromV<V>)>
HWY_INLINE V Per4LaneBlockShuffle(hwy::SizeTag<kIdx3210> /*idx_3210_tag*/,
                                  hwy::SizeTag<8> /*lane_size_tag*/,
                                  hwy::SizeTag<32> /*vect_size_tag*/, V v) {
  const DFromV<decltype(v)> d;
  alignas(32) uint64_t idx[4] = {kIdx3210 & 3, (kIdx3210 >> 2) & 3,
                                 (kIdx3210 >> 4) & 3, (kIdx3210 >> 6) & 3};
  return BitCast(d, TableLookupLanes(v, SetTableIndices(d, idx)));
}

}  // namespace detail

// ------------------------------ SlideUpLanes

namespace detail {

template <class D, HWY_IF_V_SIZE_D(D, 32),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2))>
HWY_INLINE VFromD<D> TableLookupSlideUpLanes(D d, VFromD<D> v, size_t amt) {
  const Repartition<uint8_t, decltype(d)> du8;
  const auto idx_vec =
      Iota(du8, static_cast<uint8_t>(size_t{0} - amt * sizeof(TFromD<D>)));
  const Indices256<TFromD<D>> idx{idx_vec.raw};
  return TableLookupLanes(v, idx);
}

template <class D, HWY_IF_V_SIZE_GT_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | 0)>
HWY_INLINE VFromD<D> TableLookupSlideUpLanes(D d, VFromD<D> v, size_t amt) {
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;

  const auto idx = Iota(du, static_cast<TU>(size_t{0} - amt));
  const auto masked_idx = And(idx, Set(du, static_cast<TU>(MaxLanes(d) - 1)));
  return IfThenElseZero(RebindMask(d, idx == masked_idx),
                        TableLookupLanes(v, IndicesFromVec(d, masked_idx)));
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<D> TableLookupSlideUpLanes(D d, VFromD<D> v, size_t amt) {
  const RepartitionToNarrow<D> dn;
  return BitCast(d, TableLookupSlideUpLanes(dn, BitCast(dn, v), amt * 2));
}

}  // namespace detail

template <int kBlocks, class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> SlideUpBlocks(D d, VFromD<D> v) {
  static_assert(0 <= kBlocks && kBlocks <= 1,
                "kBlocks must be between 0 and 1");
  return (kBlocks == 1) ? ConcatLowerLower(d, v, Zero(d)) : v;
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> SlideUpLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD
  constexpr size_t kLanesPerBlock = 16 / sizeof(TFromD<D>);
  if (__builtin_constant_p(amt)) {
    const auto v_lo = ConcatLowerLower(d, v, Zero(d));
    switch (amt * sizeof(TFromD<D>)) {
      case 0:
        return v;
      case 1:
        return CombineShiftRightBytes<15>(d, v, v_lo);
      case 2:
        return CombineShiftRightBytes<14>(d, v, v_lo);
      case 3:
        return CombineShiftRightBytes<13>(d, v, v_lo);
      case 4:
        return CombineShiftRightBytes<12>(d, v, v_lo);
      case 5:
        return CombineShiftRightBytes<11>(d, v, v_lo);
      case 6:
        return CombineShiftRightBytes<10>(d, v, v_lo);
      case 7:
        return CombineShiftRightBytes<9>(d, v, v_lo);
      case 8:
        return CombineShiftRightBytes<8>(d, v, v_lo);
      case 9:
        return CombineShiftRightBytes<7>(d, v, v_lo);
      case 10:
        return CombineShiftRightBytes<6>(d, v, v_lo);
      case 11:
        return CombineShiftRightBytes<5>(d, v, v_lo);
      case 12:
        return CombineShiftRightBytes<4>(d, v, v_lo);
      case 13:
        return CombineShiftRightBytes<3>(d, v, v_lo);
      case 14:
        return CombineShiftRightBytes<2>(d, v, v_lo);
      case 15:
        return CombineShiftRightBytes<1>(d, v, v_lo);
    }
  }

  if (__builtin_constant_p(amt >= kLanesPerBlock) && amt >= kLanesPerBlock) {
    const Half<decltype(d)> dh;
    return Combine(d, SlideUpLanes(dh, LowerHalf(dh, v), amt - kLanesPerBlock),
                   Zero(dh));
  }
#endif

  return detail::TableLookupSlideUpLanes(d, v, amt);
}

// ------------------------------ Slide1Up

template <typename D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> Slide1Up(D d, VFromD<D> v) {
  const auto v_lo = ConcatLowerLower(d, v, Zero(d));
  return CombineShiftRightBytes<15>(d, v, v_lo);
}

template <typename D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Slide1Up(D d, VFromD<D> v) {
  const auto v_lo = ConcatLowerLower(d, v, Zero(d));
  return CombineShiftRightBytes<14>(d, v, v_lo);
}

template <typename D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Slide1Up(D d, VFromD<D> v) {
  const auto v_lo = ConcatLowerLower(d, v, Zero(d));
  return CombineShiftRightBytes<12>(d, v, v_lo);
}

template <typename D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Slide1Up(D /*d*/, VFromD<D> v) {
  const auto v_lo = ConcatLowerLower(d, v, Zero(d));
  return CombineShiftRightBytes<8>(d, v, v_lo);
}

// ------------------------------ SlideDownLanes

namespace detail {

template <class D, HWY_IF_V_SIZE_D(D, 32),
          HWY_IF_T_SIZE_ONE_OF_D(
              D, (1 << 1) | ((HWY_TARGET > HWY_AVX3) ? (1 << 2) : 0))>
HWY_INLINE VFromD<D> TableLookupSlideDownLanes(D d, VFromD<D> v, size_t amt) {
  const Repartition<uint8_t, decltype(d)> du8;

  auto idx_vec = Iota(du8, static_cast<uint8_t>(amt * sizeof(TFromD<D>)));

  const RebindToSigned<decltype(du8)> di8;
  idx_vec =
      Or(idx_vec, BitCast(du8, VecFromMask(di8, BitCast(di8, idx_vec) >
                                                    Set(di8, int8_t{31}))));
  return TableLookupLanes(v, Indices256<TFromD<D>>{idx_vec.raw});
}

template <class D, HWY_IF_V_SIZE_GT_D(D, 16),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (0))>
HWY_INLINE VFromD<D> TableLookupSlideDownLanes(D d, VFromD<D> v, size_t amt) {
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;

  const auto idx = Iota(du, static_cast<TU>(amt));
  const auto masked_idx = And(idx, Set(du, static_cast<TU>(MaxLanes(d) - 1)));

  return IfThenElseZero(RebindMask(d, idx == masked_idx),
                        TableLookupLanes(v, IndicesFromVec(d, masked_idx)));
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 8)>
HWY_INLINE VFromD<D> TableLookupSlideDownLanes(D d, VFromD<D> v, size_t amt) {
  const RepartitionToNarrow<D> dn;
  return BitCast(d, TableLookupSlideDownLanes(dn, BitCast(dn, v), amt * 2));
}

}  // namespace detail

template <int kBlocks, class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> SlideDownBlocks(D d, VFromD<D> v) {
  static_assert(0 <= kBlocks && kBlocks <= 1,
                "kBlocks must be between 0 and 1");
  const Half<decltype(d)> dh;
  return (kBlocks == 1) ? ZeroExtendVector(d, UpperHalf(dh, v)) : v;
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API VFromD<D> SlideDownLanes(D d, VFromD<D> v, size_t amt) {
#if !HWY_IS_DEBUG_BUILD
  constexpr size_t kLanesPerBlock = 16 / sizeof(TFromD<D>);
  const Half<decltype(d)> dh;
  if (__builtin_constant_p(amt)) {
    const auto v_hi = ZeroExtendVector(d, UpperHalf(dh, v));
    switch (amt * sizeof(TFromD<D>)) {
      case 0:
        return v;
      case 1:
        return CombineShiftRightBytes<1>(d, v_hi, v);
      case 2:
        return CombineShiftRightBytes<2>(d, v_hi, v);
      case 3:
        return CombineShiftRightBytes<3>(d, v_hi, v);
      case 4:
        return CombineShiftRightBytes<4>(d, v_hi, v);
      case 5:
        return CombineShiftRightBytes<5>(d, v_hi, v);
      case 6:
        return CombineShiftRightBytes<6>(d, v_hi, v);
      case 7:
        return CombineShiftRightBytes<7>(d, v_hi, v);
      case 8:
        return CombineShiftRightBytes<8>(d, v_hi, v);
      case 9:
        return CombineShiftRightBytes<9>(d, v_hi, v);
      case 10:
        return CombineShiftRightBytes<10>(d, v_hi, v);
      case 11:
        return CombineShiftRightBytes<11>(d, v_hi, v);
      case 12:
        return CombineShiftRightBytes<12>(d, v_hi, v);
      case 13:
        return CombineShiftRightBytes<13>(d, v_hi, v);
      case 14:
        return CombineShiftRightBytes<14>(d, v_hi, v);
      case 15:
        return CombineShiftRightBytes<15>(d, v_hi, v);
    }
  }

  if (__builtin_constant_p(amt >= kLanesPerBlock) && amt >= kLanesPerBlock) {
    return ZeroExtendVector(
        d, SlideDownLanes(dh, UpperHalf(dh, v), amt - kLanesPerBlock));
  }
#endif

  return detail::TableLookupSlideDownLanes(d, v, amt);
}

// ------------------------------ Slide1Down

template <typename D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 1)>
HWY_API VFromD<D> Slide1Down(D d, VFromD<D> v) {
  const Half<decltype(d)> dh;
  const auto v_hi = ZeroExtendVector(d, UpperHalf(dh, v));
  return CombineShiftRightBytes<1>(d, v_hi, v);
}

template <typename D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API VFromD<D> Slide1Down(D d, VFromD<D> v) {
  const Half<decltype(d)> dh;
  const auto v_hi = ZeroExtendVector(d, UpperHalf(dh, v));
  return CombineShiftRightBytes<2>(d, v_hi, v);
}

template <typename D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 4)>
HWY_API VFromD<D> Slide1Down(D d, VFromD<D> v) {
  const Half<decltype(d)> dh;
  const auto v_hi = ZeroExtendVector(d, UpperHalf(dh, v));
  return CombineShiftRightBytes<4>(d, v_hi, v);
}

template <typename D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 8)>
HWY_API VFromD<D> Slide1Down(D /*d*/, VFromD<D> v) {
  const Half<decltype(d)> dh;
  const auto v_hi = ZeroExtendVector(d, UpperHalf(dh, v));
  return CombineShiftRightBytes<8>(d, v_hi, v);
}

// ------------------------------ Shl (Mul, ZipLower)

HWY_INLINE Vec256<uint8_t> Shl(hwy::UnsignedTag /*tag*/, Vec256<uint8_t> v,
                               Vec256<uint8_t> bits) {
  return Vec256<uint8_t>{__lasx_xvsll_b(v.raw, bits.raw)};
}

HWY_INLINE Vec256<uint16_t> Shl(hwy::UnsignedTag /*tag*/, Vec256<uint16_t> v,
                                Vec256<uint16_t> bits) {
  return Vec256<uint16_t>{__lasx_xvsll_h(v.raw, bits.raw)};
}

HWY_INLINE Vec256<uint32_t> Shl(hwy::UnsignedTag /*tag*/, Vec256<uint32_t> v,
                                Vec256<uint32_t> bits) {
  return Vec256<uint32_t>{__lasx_xvsll_w(v.raw, bits.raw)};
}

HWY_INLINE Vec256<uint64_t> Shl(hwy::UnsignedTag /*tag*/, Vec256<uint64_t> v,
                                Vec256<uint64_t> bits) {
  return Vec256<uint64_t>{__lasx_xvsllv_d(v.raw, bits.raw)};
}

template <typename T>
HWY_INLINE Vec256<T> Shl(hwy::SignedTag /*tag*/, Vec256<T> v, Vec256<T> bits) {
  // Signed left shifts are the same as unsigned.
  const Full256<T> di;
  const Full256<MakeUnsigned<T>> du;
  return BitCast(di,
                 Shl(hwy::UnsignedTag(), BitCast(du, v), BitCast(du, bits)));
}

template <typename T>
HWY_API Vec256<T> operator<<(Vec256<T> v, Vec256<T> bits) {
  return detail::Shl(hwy::TypeTag<T>(), v, bits);
}

// ------------------------------ Shr (MulHigh, IfThenElse, Not)

HWY_API Vec256<uint8_t> operator>>(Vec256<uint8_t> v, Vec256<uint8_t> bits) {
  return Vec256<uint8_t>{__lasx_xvsrl_b(v.raw, bits.raw)};
}

HWY_API Vec256<uint16_t> operator>>(Vec256<uint16_t> v, Vec256<uint16_t> bits) {
  return Vec256<uint16_t>{__lasx_xvsrl_h(v.raw, bits.raw)};
}

HWY_API Vec256<uint32_t> operator>>(Vec256<uint32_t> v, Vec256<uint32_t> bits) {
  return Vec256<uint32_t>{__lasx_xvsrl_w(v.raw, bits.raw)};
}

HWY_API Vec256<uint64_t> operator>>(Vec256<uint64_t> v, Vec256<uint64_t> bits) {
  return Vec256<uint64_t>{__lasx_xvsrl_d(v.raw, bits.raw)};
}

HWY_API Vec256<int8_t> operator>>(Vec256<int8_t> v, Vec256<int8_t> bits) {
  return Vec256<int8_t>{__lasx_xvsra_b(v.raw, bits.raw)};
}

HWY_API Vec256<int16_t> operator>>(Vec256<int16_t> v, Vec256<int16_t> bits) {
  return Vec256<int16_t>{__lasx_xvsra_h(v.raw, bits.raw)};
}

HWY_API Vec256<int32_t> operator>>(Vec256<int32_t> v, Vec256<int32_t> bits) {
  return Vec256<int32_t>{__lasx_xvsra_w(v.raw, bits.raw)};
}

HWY_API Vec256<int64_t> operator>>(Vec256<int64_t> v, Vec256<int64_t> bits) {
  return Vec256<int64_t>{__lasx_xvsra_d(v.raw, bits.raw)};
}

// ------------------------------ WidenMulPairwiseAdd

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I32_D(D)>
HWY_API VFromD<D> WidenMulPairwiseAdd(D /*d32*/, Vec256<int16_t> a,
                                      Vec256<int16_t> b) {
  __m256i ev = __lasx_xvmulwev_w_h(b.raw, a.raw);
  return VFromD<D>{__lasx_xvmaddwod_w_h(ev, b.raw, a.raw)};
}

// ------------------------------ SatWidenMulPairwiseAdd

template <class DI16, HWY_IF_V_SIZE_D(DI16, 32), HWY_IF_I16_D(DI16)>
HWY_API VFromD<DI16> SatWidenMulPairwiseAdd(
    DI16 /* tag */, VFromD<Repartition<uint8_t, DI16>> a,
    VFromD<Repartition<int8_t, DI16>> b) {
  __m256i ev = __lasx_xvmulwev_h_bu_b(a, b);
  __m256i od = __lasx_xvmulwod_h_bu_b(a, b);
  return VFromD<DI16>{__lasx_xvsadd_h(ev, od)};
}

// ------------------------------ SatWidenMulPairwiseAccumulate

template <class DI32, HWY_IF_I32_D(DI32), HWY_IF_V_SIZE_D(DI32, 32)>
HWY_API VFromD<DI32> SatWidenMulPairwiseAccumulate(
    DI32 /* tag */, VFromD<Repartition<int16_t, DI32>> a,
    VFromD<Repartition<int16_t, DI32>> b, VFromD<DI32> sum) {
  __m256i ev = __lasx_xvmulwev_w_h(a, b);
  __m256i od = __lasx_xvmaddwod_w_h(ev, a, b);
  return VFromD<DI32>{__lasx_xvsadd_w(sum.raw, od)};
}

// ------------------------------ ReorderWidenMulAccumulate

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I32_D(D)>
HWY_API VFromD<D> ReorderWidenMulAccumulate(D d, Vec256<int16_t> a,
                                            Vec256<int16_t> b,
                                            const VFromD<D> sum0,
                                            VFromD<D>& /*sum1*/) {
  return sum0 + WidenMulPairwiseAdd(d, a, b);
}

// ------------------------------ RearrangeToOddPlusEven
HWY_API Vec256<int32_t> RearrangeToOddPlusEven(const Vec256<int32_t> sum0,
                                               Vec256<int32_t> /*sum1*/) {
  return sum0;  // invariant already holds
}

HWY_API Vec256<uint32_t> RearrangeToOddPlusEven(const Vec256<uint32_t> sum0,
                                                Vec256<uint32_t> /*sum1*/) {
  return sum0;  // invariant already holds
}

// ================================================== CONVERT

// ------------------------------ Promotions (part w/ narrow lanes -> full)

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<float> v) {
  cosnt Full256<uint32_t> du;
  auto from_128 = ZeroExtendVector(D(), v);
  from_128 = BitCast(
      d, Full256<uint32_t>{__lasx_xvpermi_d(BitCast(du, from_128).raw, 0xd8)});
  return VFromD<D>{__lasx_xvfcvtl_d_s(from_128)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<int32_t> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __laxs_xvpermi_d(vec_temp, 0xd8);
  vec_temp = __lasx_xvsllwil_d_w(vec_temp, 0);
  return VFromD<D>{__lasx_xvffint_d_l(temp)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API Vec256<double> PromoteTo(D /* tag */, Vec128<uint32_t> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __laxs_xvpermi_d(vec_temp, 0xd8);
  vec_temp = __lasx_xvsllwil_du_wu(vec_temp, 0);
  return VFromD<D>{__lasx_xvffint_d_lu(vec_temp)};
}

// Unsigned: zero-extend.
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U16_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<uint8_t> v) {
  __m256i vec_temp = Combine(D(), v).raw;
  vec_temp = __laxs_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_hu_bu(vec_temp, 0)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<uint8_t, 8> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvsllwil_hu_bu(vec_temp, 0);
  return VFromD<D>{__lasx_xvsllwil_wu_hu(vec_temp)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<uint16_t> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_wu_hu(vec_temp, 0)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<uint32_t> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_du_wu(vec_temp)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec64<uint16_t> v) {
  __m256i vec_temp Combine(D(), v, v).raw;
  vec_temp = __lasx_xvsllwil_wu_hu(vec_temp, 0);
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_du_wu(vec_temp)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec32<uint8_t> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvsllwil_hu_bu(vec_temp, 0);
  vec_temp = __lasx_xvsllwil_wu_hu(vec_temp, 0);
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_du_wu(vec_temp, 0)};
}

// Signed: replicate sign bit.
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I16_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<int8_t> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_h_b(vec_temp, 0)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<int8_t, 8> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvsllwil_h_b(vec_temp, 0);
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_w_h(vec_temp, 0)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I32_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<int16_t> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_w_h(vec_temp, 0)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec128<int32_t> v) {
  __m256i vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_d_u(vec_temp, 0)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec64<int16_t> v) {
  __m256 vec_temp = Combine(D(), v, v).raw;
  vec_temp = __lasx_xvsllwil_w_h(vec_temp, 0);
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_d_w(vec_temp, 0)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteTo(D /* tag */, Vec32<int8_t> v) {
  __m256i vec_temp = Combine(D(), v, v);
  vec_temp = __lasx_xvsllwil_h_b(vec_temp, 0);
  vec_temp = __lasx_xvsllwil_w_h(vec_temp, 0);
  vec_temp = __lasx_xvpermi_d(vec_temp, 0xd8);
  return VFromD<D>{__lasx_xvsllwil_d_w(vec_temp, 0)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I64_D(D)>
HWY_API VFromD<D> PromoteInRangeTo(D /*di64*/, VFromD<Rebind<float, D>> v) {
  const v4f32 f32 = reinterpret_cast<v4f32>(v.raw);
  const v4i64 i64 = {static_cast<int64_t>(f32[0],
                     static_cast<int64_t>(f32[1],
                     static_cast<int64_t>(f32[2],
                     static_cast<int64_t>(f32[3])};
  return VFromD<D>{reinterpret_cast<__m256i>(i64)};
}
template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U64_D(D)>
HWY_API VFromD<D> PromoteInRangeTo(D /* tag */, VFromD<Rebind<float, D>> v) {
  const v4f32 f32 = reinterpret_cast<v4f32>(v.raw);
  const v4i64 i64 = {static_cast<uint64_t>(f32[0],
                     static_cast<uint64_t>(f32[1],
                     static_cast<uint64_t>(f32[2],
                     static_cast<uint64_t>(f32[3])};
  return VFromD<D>{reinterpret_cast<__m256i>(i64)};
}

// ------------------------------ PromoteEvenTo/PromoteOddTo
namespace detail {

// I32->I64 PromoteEvenTo/PromoteOddTo

template <class D, HWY_IF_LANES_D(D, 4)>
HWY_INLINE VFromD<D> PromoteEvenTo(hwy::SignedTag /*to_type_tag*/,
                                   hwy::SizeTag<8> /*to_lane_size_tag*/,
                                   hwy::SignedTag /*from_type_tag*/, D d_to,
                                   Vec256<int32_t> v) {
  return BitCast(d_to, OddEven(DupEven(BroadcastSignBit(v)), v));
}

template <class D, HWY_IF_LANES_D(D, 4)>
HWY_INLINE VFromD<D> PromoteOddTo(hwy::SignedTag /*to_type_tag*/,
                                  hwy::SizeTag<8> /*to_lane_size_tag*/,
                                  hwy::SignedTag /*from_type_tag*/, D d_to,
                                  Vec256<int32_t> v) {
  return BitCast(d_to, OddEven(BroadcastSignBit(v), DupOdd(v)));
}

}  // namespace detail

// ------------------------------ Demotions (full -> part w/ narrow lanes)

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int32_t> v) {
  const __m256i u16 = __lasx_xvssrani_hu_w(v.raw, v.raw, 0);
  return LowerHalf(VFromD<Twice<D>>{__lasx_xvpermi_d(u16, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D dn, Vec256<uint32_t> v) {
  const __m256i u16 = __lasx_xvssrlni_hu_w(v.raw, v.raw, 0);
  return LowerHalf(VFromD<Twice<D>>{__lasx_xvpermi_d(u16, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int32_t> v) {
  const __m256i i16 = __lasx_xvssrani_h_w(v.raw, v.raw, 0);
  return LowerHalf(VFromD<Twice<D>>{__lasx_xvpermi_d(i16, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int32_t> v) {
  const __m256i i16_blocks = __lasx_xvssrani_h_w(v.raw, v.raw, 0);
  const __m256i i16_concat = __lasx_xvpermi_d(i16_blocks, 0xd8);
  const __m256i i8_blocks = __lasx_xvssrani_bu_h(i16_concat, i16_concat, 0);
  return LowerHalf(LowerHalf(Vec256<uint8_t>{i8_blocks}));
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D dn, Vec256<uint32_t> v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  return DemoteTo(dn, BitCast(di, Min(v, Set(d, 0x7FFFFFFFu))));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int16_t> v) {
  const __m256i u8 = __lasx_xvssrani_bu_h(v.raw, v.raw, 0);
  return LowerHalf(VFromD<Twice<D>>{__lasx_xvpermi_d(u8, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D dn, Vec256<uint16_t> v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  return DemoteTo(dn, BitCast(di, Min(v, Set(d, 0x7FFFu))));
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_I8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int32_t> v) {
  const __m256i i16_blocks = __lasx_xvssrani_h_w(v.raw, v.raw, 0);
  const __m256i i16_concat = __lasx_xvpermi_d(i16_blocks, 0xd8);
  const __m256i i16 = __lasx_xvssrani_b_h(i16_concat, i16_concat);
  return LowerHalf(LowerHalf(VFromD<Twice<Twice<D>>>{i16}));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int16_t> v) {
  const __m256i i8 = __lasx_xvssrani_b_h(v.raw, v.raw, 0);
  return LowerHalf(VFromD<Twice<D>>{__lasx_xvpermi_d(i8, 0xd8)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrani_w_d(v.raw, v.raw, 0);
  return LowerHalf(VFromD<Twice<D>>{__lasx_xvpermi_d(i32_blocks, 0xd8)});
}
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_I16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrani_w_d(v.raw, v.raw, 0);
  const __m256i i32_concat = __lasx_xvpermi_d(i32_blocks, 0xd8);
  const __m256i i16_blocks = __lasx_xvssrani_h_w(i32_concat, i32_concat, 0);
  return LowerHalf(LowerHalf(Vec256<int16_t>{i16_blocks}));
}
template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_I8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrani_w_d(v.raw, v.raw, 0);
  const __m256i i32_concat = __lasx_xvpermi_d(i32_blocks, 0xd8);
  const __m256i i16_blocks = __lasx_xvssrani_h_w(i32_concat, i32_concat, 0);
  const __m256i i8_blocks = __lasx_xvssrani_b_h(i16_blocks, i16_blocks, 0);
  LowerHalf(LowerHalf(LowerHalf(Vec256<int8_t>{i8_blocks})));
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrani_wu_d(v.raw, v.raw, 0);
  LowerHalf(VFromD<Twice<D>>{__lasx_xvpermi_d(i32_blocks, 0xd8)});
}
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrani_w_d(v.raw, v.raw, 0);
  const __m256i i32_concat = __lasx_xvpermi_d(i32_blocks, 0xd8);
  const __m256i i16_blocks = __lasx_xvssrani_hu_w(i32_concat, i32_concat, 0);
  LowerHalf(LowerHalf(VFromD<Twice<Twice<D>>>{i16_blocks}));
}
template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<int64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrani_w_d(v.raw, v.raw, 0);
  const __m256i i32_concat = __lasx_xvpermi_d(i32_blocks, 0xd8);
  const __m256i i16_blocks = __lasx_xvssrani_h_w(i32_concat, i32_concat, 0);
  const __m256i i8_blocks = __lasx_xvssrani_bu_h(i16_blocks, i16_blocks, 0);
  LowerHalf(LowerHalf(LowerHalf(Vec256<uint8_t>{i8_blocks})));
}
template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<uint64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrlni_wu_d(v.raw, v.raw, 0);
  LowerHalf(VFromD<Twice<D>>{__lasx_xvpermi_d(i32_blocks, 0xd8)});
}
template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<uint64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrlni_wu_d(v.raw, v.raw, 0);
  const __m256i i32_concat = __lasx_xvpermi_d(i32_blocks, 0xd8);
  const __m256i i16_blocks = __lasx_xvssrlni_hu_w(i32_concat, i32_concat, 0);
  return LowerHalf(LowerHalf(Vec256<uint16_t>{i16_blocks}));
}
template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_U8_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<uint64_t> v) {
  const __m256i i32_blocks = __lasx_xvssrlni_wu_d(v.raw, v.raw, 0);
  const __m256i i32_concat = __lasx_xvpermi_d(i32_blocks, 0xd8);
  const __m256i i16_blocks = __lasx_xvssrlni_hu_w(i32_concat, i32_concat, 0);
  const __m256i i8_blocks = __lasx_xvssrlni_bu_h(i16_blocks, i16_blocks, 0);
  return LowerHalf(LowerHalf(LowerHalf(Vec256<uint8_t>{i8_blocks})));
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /*d16*/, Vec256<int32_t> a,
                                   Vec256<int32_t> b) {
  return VFromD<D>{__lasx_xvssrani_h_w(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /*d16*/, Vec256<int32_t> a,
                                   Vec256<int32_t> b) {
  return VFromD<D>{__lasx_xvssrani_hu_w(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U16_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, Vec256<uint32_t> a,
                                   Vec256<uint32_t> b) {
  // const DFromV<decltype(a)> d;
  // const RebindToSigned<decltype(d)> di;
  // const auto max_i32 = Set(d, 0x7FFFFFFFu);
  // return ReorderDemote2To(dn, BitCast(di, Min(a, max_i32)),
  //                        BitCast(di, Min(b, max_i32)));
  (void)dn;
  return VFromD<D>{__lasx_xvssrlni_hu_w(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /*d16*/, Vec256<int16_t> a,
                                   Vec256<int16_t> b) {
  return VFromD<D>{__lasx_xvssrani_b_h(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D /*d16*/, Vec256<int16_t> a,
                                   Vec256<int16_t> b) {
  return VFromD<D>{__lasx_xvssrani_bu_h(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U8_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, Vec256<uint16_t> a,
                                   Vec256<uint16_t> b) {
  (void)dn;
  return VFromD<D>{__lasx_xvssrlni_bu_h(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I32_D(D)>
HWY_API Vec256<int32_t> ReorderDemote2To(D dn, Vec256<int64_t> a,
                                         Vec256<int64_t> b) {
  (void)dn;
  return VFromD<D>{__lasx_xvssrani_w_d(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_U32_D(D)>
HWY_API Vec256<uint32_t> ReorderDemote2To(D dn, Vec256<int64_t> a,
                                          Vec256<int64_t> b) {
  (void)dn;
  return VFromD<D>{__lasx_xvssrani_wu_d(b.raw, a.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_UI32_D(D)>
HWY_API VFromD<D> ReorderDemote2To(D dn, Vec256<uint64_t> a,
                                   Vec256<uint64_t> b) {
  (void)dn;
  return VFromD<D>{__lasx_xvssrlni_w_d(b.raw, a.raw)};
}

template <class D, class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL(TFromD<D>),
          HWY_IF_V_SIZE_D(D, 32), HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_V(V, sizeof(TFromD<D>) * 2),
          HWY_IF_LANES_D(D, HWY_MAX_LANES_D(DFromV<V>) * 2),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2) | (1 << 4) | 1 << 8)>
HWY_API VFromD<D> OrderedDemote2To(D d, V a, V b) {
  return VFromD<D>{__lasx_xvpermi_d(ReorderDemote2To(d, a, b).raw, 0xd8)};
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, Vec256<double> v) {
  const Full256<int32_t> di;
  const Vec256<float> f32_blocks{__lasx_xvfcvt_s_d(v.raw, v.raw)};
  const auto f32_concat =
      BitCast(Twice<D>(), VFromD<decltype(di)>{__lasx_xvpermi_d(
                              BitCast(di, f32_blocks).raw, 0xd8)});
  return LowerHalf(f32_concat);
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_I32_D(D)>
HWY_API VFromD<D> DemoteInRangeTo(D /* tag */, Vec256<double> v) {
  const __m256i i32_blocks = __lasx_xvftint_w_d(v.raw, v.raw);
  return LowerHalf(D(), VFromD<Twice<D>>{__lasx_xvpermi_d(i32_blocks, 0xd8)});
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> DemoteInRangeTo(D /* tag */, Vec256<double> v) {
  const auto raw_vec = reinterpret_cast<v4f64>(v.raw);
  return Dup128VecFromValues(
      D(), static_cast<uint32_t>(vec_raw[0]), static_cast<uint32_t>(vec_raw[1]),
      static_cast<uint32_t>(vec_raw[2]), static_cast<uint32_t>(vec_raw[3]));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<int64_t, D>> v) {
  const Twice<D> d;
  const RebindToSigned<decltype(d)> di;
  const Vec256<float> f32_blocks{__lasx_xvffint_s_l(v.raw, v.raw)};
  return LowerHalf(BitCast(d, VFromD<decltype(di)>{__lasx_xvpermi_d(
                                  BitCast(di, f32_blocks).raw, 0xd8)}));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_F32_D(D)>
HWY_API VFromD<D> DemoteTo(D /* tag */, VFromD<Rebind<uint64_t, D>> v) {
  const auto raw_vec = reinterpret_cast<v4u64>(v.raw);
  return Dup128VecFromValues(
      D(), static_cast<float>(vec_raw[0]), static_cast<float>(vec_raw[1]),
      static_cast<float>(vec_raw[2]), static_cast<float>(vec_raw[3]));
}  // statistics for weekly report 179 interfaces 2025-03-07

// For already range-limited input [0, 255].
HWY_API Vec128<uint8_t, 8> U8FromU32(const Vec256<uint32_t> v) {
  const Full256<uint32_t> d32;
  const Full64<uint8_t> d8;
  alignas(32) static constexpr uint32_t k8From32[8] = {
      0x0C080400u, 0x13121110u, 0, 0, 0x13121110u, 0x0C080400u, 0, 0};
  // Place first four bytes in lo[0], remaining 4 in hi[1].
  const auto quad = VFromD<decltype(d32)>{
      __lasx_xvshuf_b(Zero(d32).raw, v.raw, Load(d32, k8From32).raw)};
  // Interleave both quadruplets - OR instead of unpack reduces port5 pressure.
  const auto lo = LowerHalf(quad);
  const auto hi = UpperHalf(Half<decltype(d32)>(), quad);
  return BitCast(d8, LowerHalf(lo | hi));
}

// ------------------------------ Truncations

template <class D, HWY_IF_V_SIZE_D(D, 4), HWY_IF_U8_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec256<uint64_t> v) {
  const Full256<uint16_t> d16;
  alignas(32) static constexpr uint16_t kMap[16] = {0, 8, 16, 24};
  const __m256i i32_blocks = __lasx_xvpickev_w(v.raw, v.raw);
  const __m256i i32_concat = __lasx_xvpermi_d(i32_blocks, 0xd8);
  const auto i16 = TableLookupBytes(i32_concat, Load(d16, kMap));
  return LowerHalf(LowerHalf(LowerHalf(Vec256<uint8_t>{i16.raw})));
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_U16_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec256<uint64_t> v) {
  const __m256i i32_blocks = __lasx_xvpickev_w(v.raw, v.raw);
  const __m256i i32_concat = __lasx_xvpermi_d(i32_blocks, 0xd8);
  const __m256i i16 = __lasx_xvpickev_h(i32_concat, i32_concat);
  return LowerHalf(LowerHalf(Vec256<uint16_t>{i16.raw}));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U32_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec256<uint64_t> v) {
  const Full256<uint32_t> d32;
  alignas(32) static constexpr uint32_t kEven[8] = {0, 2, 4, 6, 0, 2, 4, 6};
  const auto v32 =
      TableLookupLanes(BitCast(d32, v), SetTableIndices(d32, kEven));
  return LowerHalf(Vec256<uint32_t>{v32.raw});
}

template <class D, HWY_IF_V_SIZE_D(D, 8), HWY_IF_U8_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec256<uint32_t> v) {
  const Full256<uint32_t> d32;
  alignas(32) static constexpr uint32_t kEven[8] = {0x06040200, 0x0E0C0A08};
  const __m256i i16_blocks = __lasx_xvpickve_h(v.raw, v.raw);
  const __m256i i16_concat = __lasx_xvpermi_d(i16_blocks, 0xd8);
  const auto i8 = TableLookupBytes(i6_concat, Load(d32, kEven));
  return LowerHalf(LowerHalf(Vec256<uint8_t>{i8.raw}));
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U16_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec256<uint32_t> v) {
  const __m256i i16_blocks = __lasx_xvpickev_h(v.raw, v.raw);
  const __m256i i16_concat = __lasx_xvpermi_d(i16_blocks, 0xd8);
  return LowerHalf(Vec256<uint8_t>{i16_concat});
}

template <class D, HWY_IF_V_SIZE_D(D, 16), HWY_IF_U8_D(D)>
HWY_API VFromD<D> TruncateTo(D /* tag */, Vec256<uint16_t> v) {
  const __m256i i8_blocks = __lasx_xvpickev_b(v.raw, v.raw);
  const __m256i i8_concat = __lasx_xvpermi_d(i8_blocks, 0xd8);
  return LowerHalf(Vec256<uint8_t>{i8_concat});
}

// ------------------------------ Integer <=> fp (ShiftRight, OddEven)

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConvertTo(D /* tag */, Vec256<int32_t> v) {
  return VFromD<D>{__lasx_xvffint_s_w(v.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F32_D(D)>
HWY_API VFromD<D> ConvertTo(D /*df*/, Vec256<uint32_t> v) {
  return VFromD<D>{__lasx_xvffint_s_wu(v.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API VFromD<D> ConvertTo(D /*dd*/, Vec256<int64_t> v) {
  return VFromD<D>{__lasx_xvffint_d_l(v.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_F64_D(D)>
HWY_API VFromD<D> ConvertTo(D /*dd*/, Vec256<uint64_t> v) {
  return VFromD<D>{__lasx_xvffint_d_lu(v.raw)};
}

// Truncates (rounds toward zero).

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I32_D(D)>
HWY_API VFromD<D> ConvertInRangeTo(D /*d*/, Vec256<float> v) {
  return VFromD<D>{__lasx_xvftint_w_s(v.raw)};
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_I64_D(D)>
HWY_API VFromD<D> ConvertInRangeTo(D /*di*/, Vec256<double> v) {
  return VFromD<D>{__lasx_xvftint_l_d(v.raw)};
}
template <class DU, HWY_IF_V_SIZE_D(DU, 32), HWY_IF_U32_D(DU)>
HWY_API VFromD<DU> ConvertInRangeTo(DU /*du*/, VFromD<RebindToFloat<DU>> v) {
  return VFromD<DU>{__lasx_xvfftin_wu_s(v.raw)};
}
template <class DU, HWY_IF_V_SIZE_D(DU, 32), HWY_IF_U64_D(DU)>
HWY_API VFromD<DU> ConvertInRangeTo(DU /*du*/, VFromD<RebindToFloat<DU>> v) {
  return VFromD<DU>{__lasx_xvfftin_lu_d(v.raw)};
}

// ------------------------------ LoadMaskBits (TestBit)

namespace detail {

// 256 suffix avoids ambiguity with x86_128 without needing HWY_IF_V_SIZE.
template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_INLINE Mask256<T> LoadMaskBits256(uint64_t mask_bits) {
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  const Repartition<uint32_t, decltype(d)> du32;
  const auto vbits = BitCast(du, Set(du32, static_cast<uint32_t>(mask_bits)));

  // Replicate bytes 8x such that each byte contains the bit that governs it.
  const Repartition<uint64_t, decltype(d)> du64;
  alignas(32) static constexpr uint64_t kRep8[4] = {
      0x0000000000000000ull, 0x0101010101010101ull, 0x0202020202020202ull,
      0x0303030303030303ull};
  const auto rep8 = TableLookupBytes(vbits, BitCast(du, Load(du64, kRep8)));

  const VFromD<decltype(du)> bit = Dup128VecFromValues(
      du, 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128);
  return RebindMask(d, TestBit(rep8, bit));
}

template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Mask256<T> LoadMaskBits256(uint64_t mask_bits) {
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) static constexpr uint16_t kBit[16] = {
      1,     2,     4,     8,     16,     32,     64,     128,
      0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000};
  const auto vmask_bits = Set(du, static_cast<uint16_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Mask256<T> LoadMaskBits256(uint64_t mask_bits) {
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) static constexpr uint32_t kBit[8] = {1, 2, 4, 8, 16, 32, 64, 128};
  const auto vmask_bits = Set(du, static_cast<uint32_t>(mask_bits));
  return RebindMask(d, TestBit(vmask_bits, Load(du, kBit)));
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Mask256<T> LoadMaskBits256(uint64_t mask_bits) {
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  alignas(32) static constexpr uint64_t kBit[8] = {1, 2, 4, 8};
  return RebindMask(d, TestBit(Set(du, mask_bits), Load(du, kBit)));
}

}  // namespace detail

// `p` points to at least 8 readable bytes, not all of which need be valid.
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API MFromD<D> LoadMaskBits(D d, const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t kN = MaxLanes(d);
  constexpr size_t kNumBytes = (kN + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (kN < 8) {
    mask_bits &= (1ull << kN) - 1;
  }

  return detail::LoadMaskBits256<TFromD<D>>(mask_bits);
}

// ------------------------------ BitsFromMask

template <class D, HWY_IF_T_SIZE_D(D, 1), HWY_IF_V_SIZE_D(D, 32)>
HWY_API uint64_t BitsFromMask(D d, MFromD<D> mask) {
  const auto sign_bits = __lasx_xvmskltz_b(mask.raw);
  return static_cast<uint32_t>(__lasx_xvpickve2gr_w(sign_bits, 0) |
                               (__lasx_xvpickve2gr_w(sign_bits, 4) << 16));
}

template <class D, HWY_IF_T_SIZE_D(D, 2), HWY_IF_V_SIZE_D(D, 32)>
HWY_API uint64_t BitsFromMask(D d, MFromD<D> mask) {
  const auto sign_bits = __lasx_xvpickod_b(mask.raw, mask.raw);
  const auto sign_shuf = __lasx_xvpermi_d(sign_bits, 0xd8);
  const auto sign_last = __lasx_xvmskltz_b(sign_shuf);
  return static_cast<unsigned>(__lasx_xvpickve2gr_w(sign_last, 0));
}

template <class D, HWY_IF_T_SIZE_D(D, 4), HWY_IF_V_SIZE_D(D, 32)>
HWY_API uint64_t BitsFromMask(D d, MFromD<D> mask) {
  const auto sign_bits = __lasx_xvpickod_h(mask.raw, mask.raw);
  const auto sign_shuf = __lasx_xvpermi_d(sign_bits, 0xd8);
  const auto sign_last = __lasx_xvmskltz_h(sign_shuf);
  return static_cast<unsigned>(__lasx_xvpickve2gr_w(sign_last, 0));
}

template <class D, HWY_IF_T_SIZE_D(D, 8), HWY_IF_V_SIZE_D(D, 32)>
HWY_API uint64_t BitsFromMask(D d, MFromD<D> mask) {
  const auto sign_bits = __lasx_xvpickod_w(mask.raw, mask.raw);
  const auto sign_shuf = __lasx_xvpermi_d(sign_bits, 0xd8);
  const auto sign_last = __lasx_xvmskltz_w(sign_shuf);
  return static_cast<unsigned>(__lasx_xvpickve2gr_w(sign_last, 0));
}

// ------------------------------ StoreMaskBits
// `p` points to at least 8 writable bytes.
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API size_t StoreMaskBits(D d, MFromD<D> mask, uint8_t* bits) {
  constexpr size_t N = Lanes(d);
  constexpr size_t kNumBytes = (N + 7) / 8;

  const uint64_t mask_bits = BitsFromMask(d, mask);
  CopyBytes<kNumBytes>(&mask_bits, bits);
  return kNumBytes;
}

// ------------------------------ Mask testing

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API bool AllFalse(D d, MFromD<D> mask) {
  return BitsFromMask(d, mask) == 0;
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API bool AllTrue(D d, MFromD<D> mask) {
  constexpr uint64_t kAllBits = (1ull << Lanes(d)) - 1;
  return BitsFromMask(d, mask) == kAllBits;
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API size_t CountTrue(D d, MFromD<D> mask) {
  return PopCount(BitsFromMask(d, mask));
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API size_t FindKnownFirstTrue(D d, MFromD<D> mask) {
  const uint32_t mask_bits = static_cast<uint32_t>(BitsFromMask(d, mask));
  return Num0BitsBelowLS1Bit_Nonzero32(mask_bits);
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API intptr_t FindFirstTrue(D d, MFromD<D> mask) {
  const uint32_t mask_bits = static_cast<uint32_t>(BitsFromMask(d, mask));
  return mask_bits ? intptr_t(Num0BitsBelowLS1Bit_Nonzero32(mask_bits)) : -1;
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API size_t FindKnownLastTrue(D d, MFromD<D> mask) {
  const uint32_t mask_bits = static_cast<uint32_t>(BitsFromMask(d, mask));
  return 31 - Num0BitsAboveMS1Bit_Nonzero32(mask_bits);
}

template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API intptr_t FindLastTrue(D d, MFromD<D> mask) {
  const uint32_t mask_bits = static_cast<uint32_t>(BitsFromMask(d, mask));
  return mask_bits ? intptr_t(31 - Num0BitsAboveMS1Bit_Nonzero32(mask_bits))
                   : -1;
}

// ------------------------------ Compress, CompressBits

namespace detail {

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec256<uint32_t> IndicesFromBits256(uint64_t mask_bits) {
  const Full256<uint32_t> d32;
  // We need a masked Iota(). With 8 lanes, there are 256 combinations and a LUT
  // of SetTableIndices would require 8 KiB, a large part of L1D. The other
  // alternative is _pext_u64, but this is extremely slow on Zen2 (18 cycles)
  // and unavailable in 32-bit builds. We instead compress each index into 4
  // bits, for a total of 1 KiB.
  alignas(16) static constexpr uint32_t packed_array[256] = {
      // PrintCompress32x8Tables
      0x76543210, 0x76543218, 0x76543209, 0x76543298, 0x7654310a, 0x765431a8,
      0x765430a9, 0x76543a98, 0x7654210b, 0x765421b8, 0x765420b9, 0x76542b98,
      0x765410ba, 0x76541ba8, 0x76540ba9, 0x7654ba98, 0x7653210c, 0x765321c8,
      0x765320c9, 0x76532c98, 0x765310ca, 0x76531ca8, 0x76530ca9, 0x7653ca98,
      0x765210cb, 0x76521cb8, 0x76520cb9, 0x7652cb98, 0x76510cba, 0x7651cba8,
      0x7650cba9, 0x765cba98, 0x7643210d, 0x764321d8, 0x764320d9, 0x76432d98,
      0x764310da, 0x76431da8, 0x76430da9, 0x7643da98, 0x764210db, 0x76421db8,
      0x76420db9, 0x7642db98, 0x76410dba, 0x7641dba8, 0x7640dba9, 0x764dba98,
      0x763210dc, 0x76321dc8, 0x76320dc9, 0x7632dc98, 0x76310dca, 0x7631dca8,
      0x7630dca9, 0x763dca98, 0x76210dcb, 0x7621dcb8, 0x7620dcb9, 0x762dcb98,
      0x7610dcba, 0x761dcba8, 0x760dcba9, 0x76dcba98, 0x7543210e, 0x754321e8,
      0x754320e9, 0x75432e98, 0x754310ea, 0x75431ea8, 0x75430ea9, 0x7543ea98,
      0x754210eb, 0x75421eb8, 0x75420eb9, 0x7542eb98, 0x75410eba, 0x7541eba8,
      0x7540eba9, 0x754eba98, 0x753210ec, 0x75321ec8, 0x75320ec9, 0x7532ec98,
      0x75310eca, 0x7531eca8, 0x7530eca9, 0x753eca98, 0x75210ecb, 0x7521ecb8,
      0x7520ecb9, 0x752ecb98, 0x7510ecba, 0x751ecba8, 0x750ecba9, 0x75ecba98,
      0x743210ed, 0x74321ed8, 0x74320ed9, 0x7432ed98, 0x74310eda, 0x7431eda8,
      0x7430eda9, 0x743eda98, 0x74210edb, 0x7421edb8, 0x7420edb9, 0x742edb98,
      0x7410edba, 0x741edba8, 0x740edba9, 0x74edba98, 0x73210edc, 0x7321edc8,
      0x7320edc9, 0x732edc98, 0x7310edca, 0x731edca8, 0x730edca9, 0x73edca98,
      0x7210edcb, 0x721edcb8, 0x720edcb9, 0x72edcb98, 0x710edcba, 0x71edcba8,
      0x70edcba9, 0x7edcba98, 0x6543210f, 0x654321f8, 0x654320f9, 0x65432f98,
      0x654310fa, 0x65431fa8, 0x65430fa9, 0x6543fa98, 0x654210fb, 0x65421fb8,
      0x65420fb9, 0x6542fb98, 0x65410fba, 0x6541fba8, 0x6540fba9, 0x654fba98,
      0x653210fc, 0x65321fc8, 0x65320fc9, 0x6532fc98, 0x65310fca, 0x6531fca8,
      0x6530fca9, 0x653fca98, 0x65210fcb, 0x6521fcb8, 0x6520fcb9, 0x652fcb98,
      0x6510fcba, 0x651fcba8, 0x650fcba9, 0x65fcba98, 0x643210fd, 0x64321fd8,
      0x64320fd9, 0x6432fd98, 0x64310fda, 0x6431fda8, 0x6430fda9, 0x643fda98,
      0x64210fdb, 0x6421fdb8, 0x6420fdb9, 0x642fdb98, 0x6410fdba, 0x641fdba8,
      0x640fdba9, 0x64fdba98, 0x63210fdc, 0x6321fdc8, 0x6320fdc9, 0x632fdc98,
      0x6310fdca, 0x631fdca8, 0x630fdca9, 0x63fdca98, 0x6210fdcb, 0x621fdcb8,
      0x620fdcb9, 0x62fdcb98, 0x610fdcba, 0x61fdcba8, 0x60fdcba9, 0x6fdcba98,
      0x543210fe, 0x54321fe8, 0x54320fe9, 0x5432fe98, 0x54310fea, 0x5431fea8,
      0x5430fea9, 0x543fea98, 0x54210feb, 0x5421feb8, 0x5420feb9, 0x542feb98,
      0x5410feba, 0x541feba8, 0x540feba9, 0x54feba98, 0x53210fec, 0x5321fec8,
      0x5320fec9, 0x532fec98, 0x5310feca, 0x531feca8, 0x530feca9, 0x53feca98,
      0x5210fecb, 0x521fecb8, 0x520fecb9, 0x52fecb98, 0x510fecba, 0x51fecba8,
      0x50fecba9, 0x5fecba98, 0x43210fed, 0x4321fed8, 0x4320fed9, 0x432fed98,
      0x4310feda, 0x431feda8, 0x430feda9, 0x43feda98, 0x4210fedb, 0x421fedb8,
      0x420fedb9, 0x42fedb98, 0x410fedba, 0x41fedba8, 0x40fedba9, 0x4fedba98,
      0x3210fedc, 0x321fedc8, 0x320fedc9, 0x32fedc98, 0x310fedca, 0x31fedca8,
      0x30fedca9, 0x3fedca98, 0x210fedcb, 0x21fedcb8, 0x20fedcb9, 0x2fedcb98,
      0x10fedcba, 0x1fedcba8, 0x0fedcba9, 0xfedcba98};

  // No need to mask because __lasx_xvperm_w ignores bits 3..31.
  // Just shift each copy of the 32 bit LUT to extract its 4-bit fields.
  const auto packed = Set(d32, packed_array[mask_bits]);
  alignas(32) static constexpr uint32_t shifts[8] = {0,  4,  8,  12,
                                                     16, 20, 24, 28};
  return packed >> Load(d32, shifts);
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec256<uint64_t> IndicesFromBits256(uint64_t mask_bits) {
  const Full256<uint64_t> d64;

  // For 64-bit, there are only 4 lanes, so we can afford to load the
  // entire index vector directly.
  alignas(32) static constexpr uint64_t u64_indices[4 * 16] = {
      // PrintCompress64x4PairTables
      0,  1,  2, 3, 8, 1,  2,  3, 9, 0,  2,  3, 8, 9, 2,  3,
      10, 0,  1, 3, 8, 10, 1,  3, 9, 10, 0,  3, 8, 9, 10, 3,
      11, 0,  1, 2, 8, 11, 1,  2, 9, 11, 0,  2, 8, 9, 11, 2,
      10, 11, 0, 1, 8, 10, 11, 1, 9, 10, 11, 0, 8, 9, 10, 11};
  return Load(d64, u64_indices + 32 * mask_bits);
}

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_INLINE Vec256<uint32_t> IndicesFromNotBits256(uint64_t mask_bits) {
  const Full256<uint32_t> d32;
  // We need a masked Iota(). With 8 lanes, there are 256 combinations and a LUT
  // of SetTableIndices would require 8 KiB, a large part of L1D. We instead
  // compress each index into 4 bits, for a total of 1 KiB.
  alignas(16) static constexpr uint32_t packed_array[256] = {
      // PrintCompressNot32x8Tables
      0xfedcba98, 0x8fedcba9, 0x9fedcba8, 0x98fedcba, 0xafedcb98, 0xa8fedcb9,
      0xa9fedcb8, 0xa98fedcb, 0xbfedca98, 0xb8fedca9, 0xb9fedca8, 0xb98fedca,
      0xbafedc98, 0xba8fedc9, 0xba9fedc8, 0xba98fedc, 0xcfedba98, 0xc8fedba9,
      0xc9fedba8, 0xc98fedba, 0xcafedb98, 0xca8fedb9, 0xca9fedb8, 0xca98fedb,
      0xcbfeda98, 0xcb8feda9, 0xcb9feda8, 0xcb98feda, 0xcbafed98, 0xcba8fed9,
      0xcba9fed8, 0xcba98fed, 0xdfecba98, 0xd8fecba9, 0xd9fecba8, 0xd98fecba,
      0xdafecb98, 0xda8fecb9, 0xda9fecb8, 0xda98fecb, 0xdbfeca98, 0xdb8feca9,
      0xdb9feca8, 0xdb98feca, 0xdbafec98, 0xdba8fec9, 0xdba9fec8, 0xdba98fec,
      0xdcfeba98, 0xdc8feba9, 0xdc9feba8, 0xdc98feba, 0xdcafeb98, 0xdca8feb9,
      0xdca9feb8, 0xdca98feb, 0xdcbfea98, 0xdcb8fea9, 0xdcb9fea8, 0xdcb98fea,
      0xdcbafe98, 0xdcba8fe9, 0xdcba9fe8, 0xdcba98fe, 0xefdcba98, 0xe8fdcba9,
      0xe9fdcba8, 0xe98fdcba, 0xeafdcb98, 0xea8fdcb9, 0xea9fdcb8, 0xea98fdcb,
      0xebfdca98, 0xeb8fdca9, 0xeb9fdca8, 0xeb98fdca, 0xebafdc98, 0xeba8fdc9,
      0xeba9fdc8, 0xeba98fdc, 0xecfdba98, 0xec8fdba9, 0xec9fdba8, 0xec98fdba,
      0xecafdb98, 0xeca8fdb9, 0xeca9fdb8, 0xeca98fdb, 0xecbfda98, 0xecb8fda9,
      0xecb9fda8, 0xecb98fda, 0xecbafd98, 0xecba8fd9, 0xecba9fd8, 0xecba98fd,
      0xedfcba98, 0xed8fcba9, 0xed9fcba8, 0xed98fcba, 0xedafcb98, 0xeda8fcb9,
      0xeda9fcb8, 0xeda98fcb, 0xedbfca98, 0xedb8fca9, 0xedb9fca8, 0xedb98fca,
      0xedbafc98, 0xedba8fc9, 0xedba9fc8, 0xedba98fc, 0xedcfba98, 0xedc8fba9,
      0xedc9fba8, 0xedc98fba, 0xedcafb98, 0xedca8fb9, 0xedca9fb8, 0xedca98fb,
      0xedcbfa98, 0xedcb8fa9, 0xedcb9fa8, 0xedcb98fa, 0xedcbaf98, 0xedcba8f9,
      0xedcba9f8, 0xedcba98f, 0xfedcba98, 0xf8edcba9, 0xf9edcba8, 0xf98edcba,
      0xfaedcb98, 0xfa8edcb9, 0xfa9edcb8, 0xfa98edcb, 0xfbedca98, 0xfb8edca9,
      0xfb9edca8, 0xfb98edca, 0xfbaedc98, 0xfba8edc9, 0xfba9edc8, 0xfba98edc,
      0xfcedba98, 0xfc8edba9, 0xfc9edba8, 0xfc98edba, 0xfcaedb98, 0xfca8edb9,
      0xfca9edb8, 0xfca98edb, 0xfcbeda98, 0xfcb8eda9, 0xfcb9eda8, 0xfcb98eda,
      0xfcbaed98, 0xfcba8ed9, 0xfcba9ed8, 0xfcba98ed, 0xfdecba98, 0xfd8ecba9,
      0xfd9ecba8, 0xfd98ecba, 0xfdaecb98, 0xfda8ecb9, 0xfda9ecb8, 0xfda98ecb,
      0xfdbeca98, 0xfdb8eca9, 0xfdb9eca8, 0xfdb98eca, 0xfdbaec98, 0xfdba8ec9,
      0xfdba9ec8, 0xfdba98ec, 0xfdceba98, 0xfdc8eba9, 0xfdc9eba8, 0xfdc98eba,
      0xfdcaeb98, 0xfdca8eb9, 0xfdca9eb8, 0xfdca98eb, 0xfdcbea98, 0xfdcb8ea9,
      0xfdcb9ea8, 0xfdcb98ea, 0xfdcbae98, 0xfdcba8e9, 0xfdcba9e8, 0xfdcba98e,
      0xfedcba98, 0xfe8dcba9, 0xfe9dcba8, 0xfe98dcba, 0xfeadcb98, 0xfea8dcb9,
      0xfea9dcb8, 0xfea98dcb, 0xfebdca98, 0xfeb8dca9, 0xfeb9dca8, 0xfeb98dca,
      0xfebadc98, 0xfeba8dc9, 0xfeba9dc8, 0xfeba98dc, 0xfecdba98, 0xfec8dba9,
      0xfec9dba8, 0xfec98dba, 0xfecadb98, 0xfeca8db9, 0xfeca9db8, 0xfeca98db,
      0xfecbda98, 0xfecb8da9, 0xfecb9da8, 0xfecb98da, 0xfecbad98, 0xfecba8d9,
      0xfecba9d8, 0xfecba98d, 0xfedcba98, 0xfed8cba9, 0xfed9cba8, 0xfed98cba,
      0xfedacb98, 0xfeda8cb9, 0xfeda9cb8, 0xfeda98cb, 0xfedbca98, 0xfedb8ca9,
      0xfedb9ca8, 0xfedb98ca, 0xfedbac98, 0xfedba8c9, 0xfedba9c8, 0xfedba98c,
      0xfedcba98, 0xfedc8ba9, 0xfedc9ba8, 0xfedc98ba, 0xfedcab98, 0xfedca8b9,
      0xfedca9b8, 0xfedca98b, 0xfedcba98, 0xfedcb8a9, 0xfedcb9a8, 0xfedcb98a,
      0xfedcba98, 0xfedcba89, 0xfedcba98, 0xfedcba98};

  // No need to mask because <__lasx_xvperm_w> ignores bits 3..31.
  // Just shift each copy of the 32 bit LUT to extract its 4-bit fields.
  const Vec256<uint32_t> packed = Set(d32, packed_array[mask_bits]);
  alignas(32) static constexpr uint32_t shifts[8] = {0,  4,  8,  12,
                                                     16, 20, 24, 28};
  return packed >> Load(d32, shifts);
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_INLINE Vec256<uint64_t> IndicesFromNotBits256(uint64_t mask_bits) {
  const Full256<uint64_t> d64;

  // For 64-bit, there are only 4 lanes, so we can afford to load
  // the entire index vector directly.
  alignas(32) static constexpr uint64_t u64_indices[4 * 16] = {
      // PrintCompressNot64x4PairTables
      8, 9, 10, 11, 9, 10, 11, 0, 8, 10, 11, 1, 10, 11, 0, 1,
      8, 9, 11, 2,  9, 11, 0,  2, 8, 11, 1,  2, 11, 0,  1, 2,
      8, 9, 10, 3,  9, 10, 0,  3, 8, 10, 1,  3, 10, 0,  1, 3,
      8, 9, 2,  3,  9, 0,  2,  3, 8, 1,  2,  3, 0,  1,  2, 3};
  return Load(d64, u64_indices + 32 * mask_bits);
}

template <typename T, HWY_IF_NOT_T_SIZE(T, 2)>
HWY_INLINE Vec256<T> Compress(Vec256<T> v, const uint64_t mask_bits) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;

  HWY_DASSERT(mask_bits < (1ull << Lanes(d)));
  const Indices256<TFromD<decltype(di)>> indices{
      IndicesFromBits256<T>(mask_bits).raw};
  return BitCast(d, TableLookupLanes(BitCast(di, v), indices));
}

// LUTs are infeasible for 2^16 possible masks, so splice together two
// half-vector Compress.
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Vec256<T> Compress(Vec256<T> v, const uint64_t mask_bits) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  const auto vu16 = BitCast(du, v);  // (required for float16_t inputs)
  const Half<decltype(du)> duh;
  const auto half0 = LowerHalf(duh, vu16);
  const auto half1 = UpperHalf(duh, vu16);

  const uint64_t mask_bits0 = mask_bits & 0xFF;
  const uint64_t mask_bits1 = mask_bits >> 8;
  const auto compressed0 = detail::CompressBits(half0, mask_bits0);
  const auto compressed1 = detail::CompressBits(half1, mask_bits1);

  alignas(32) uint16_t all_true[16] = {};
  // Store mask=true lanes, left to right.
  const size_t num_true0 = PopCount(mask_bits0);
  Store(compressed0, duh, all_true);
  StoreU(compressed1, duh, all_true + num_true0);

  if (hwy::HWY_NAMESPACE::CompressIsPartition<T>::value) {
    // Store mask=false lanes, right to left. The second vector fills the upper
    // half with right-aligned false lanes. The first vector is shifted
    // rightwards to overwrite the true lanes of the second.
    alignas(32) uint16_t all_false[16] = {};
    const size_t num_true1 = PopCount(mask_bits1);
    Store(compressed1, duh, all_false + 8);
    StoreU(compressed0, duh, all_false + num_true1);

    const auto mask = FirstN(du, num_true0 + num_true1);
    return BitCast(d,
                   IfThenElse(mask, Load(du, all_true), Load(du, all_false)));
  } else {
    // Only care about the mask=true lanes.
    return BitCast(d, Load(du, all_true));
  }
}

template <typename T, HWY_IF_T_SIZE_ONE_OF(T, (1 << 4) | (1 << 8))>
HWY_INLINE Vec256<T> CompressNot(Vec256<T> v, const uint64_t mask_bits) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;

  HWY_DASSERT(mask_bits < (1ull << Lanes(d)));
  const Indices256<TFromD<decltype(di)>> indices{
      IndicesFromNotBits256<T>(mask_bits).raw};
  return BitCast(d, TableLookupLanes(BitCast(di, v), indices));
}

// LUTs are infeasible for 2^16 possible masks, so splice together two
// half-vector Compress.
template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_INLINE Vec256<T> CompressNot(Vec256<T> v, const uint64_t mask_bits) {
  // Compress ensures only the lower 16 bits are set, so flip those.
  return Compress(v, mask_bits ^ 0xFFFF);
}

}  // namespace detail

template <typename T, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec256<T> Compress(Vec256<T> v, Mask256<T> m) {
  const DFromV<decltype(v)> d;
  return detail::Compress(v, BitsFromMask(d, m));
}

template <typename T, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec256<T> CompressNot(Vec256<T> v, Mask256<T> m) {
  const DFromV<decltype(v)> d;
  return detail::CompressNot(v, BitsFromMask(d, m));
}

HWY_API Vec256<uint64_t> CompressBlocksNot(Vec256<uint64_t> v,
                                           Mask256<uint64_t> mask) {
  return CompressNot(v, mask);
}

template <typename T, HWY_IF_NOT_T_SIZE(T, 1)>
HWY_API Vec256<T> CompressBits(Vec256<T> v, const uint8_t* HWY_RESTRICT bits) {
  constexpr size_t N = 32 / sizeof(T);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }

  return detail::Compress(v, mask_bits);
}

// ------------------------------ CompressStore, CompressBitsStore

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressStore(VFromD<D> v, MFromD<D> m, D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = BitsFromMask(d, m);
  const size_t count = PopCount(mask_bits);
  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

template <class D, HWY_IF_V_SIZE_D(D, 32),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> m, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = BitsFromMask(d, m);
  const size_t count = PopCount(mask_bits);
  using TU = MakeUnsigned<T>;

  const RebindToUnsigned<decltype(d)> du;
  HWY_DASSERT(mask_bits < (1ull << Lanes(d)));
  const Vec256<TU> idx_mask = detail::IndicesFromBits256<TFromD<D>>(mask_bits);
  // Shift nibble MSB into MSB
  const shiftVal = sizeof(TU) == 4 ? 28 : 60;
  const Mask256<TU> mask32 = MaskFromVec(ShiftLeft<shiftVal>(idx_mask));
  // First cast to unsigned (RebindMask cannot change lane size)
  const MFromD<decltype(du)> mask_u{mask32.raw};
  const MFromD<D> mask = RebindMask(d, mask_u);
  const VFromD<D> compressed = BitCast(
      d,
      TableLookupLanes(BitCast(du, v), Indices256<TU>{idx_mask.raw}));

  BlendedStore(compressed, mask, d, unaligned);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_T_SIZE_D(D, 2)>
HWY_API size_t CompressBlendedStore(VFromD<D> v, MFromD<D> m, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  const uint64_t mask_bits = BitsFromMask(d, m);
  const size_t count = PopCount(mask_bits);
  const VFromD<D> compressed = detail::Compress(v, mask_bits);
  BlendedStore(compressed, FirstN(d, count), d, unaligned);
  return count;
}

template <class D, HWY_IF_V_SIZE_D(D, 32), HWY_IF_NOT_T_SIZE_D(D, 1)>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  constexpr size_t N = Lanes(d);
  constexpr size_t kNumBytes = (N + 7) / 8;

  uint64_t mask_bits = 0;
  CopyBytes<kNumBytes>(bits, &mask_bits);

  if (N < 8) {
    mask_bits &= (1ull << N) - 1;
  }
  const size_t count = PopCount(mask_bits);

  StoreU(detail::Compress(v, mask_bits), d, unaligned);
  detail::MaybeUnpoison(unaligned, count);
  return count;
}

// ------------------------------ Dup128MaskFromMaskBits

// Generic for all vector lengths >= 32 bytes
template <class D, HWY_IF_V_SIZE_GT_D(D, 16)>
HWY_API MFromD<D> Dup128MaskFromMaskBits(D d, unsigned mask_bits) {
  const Half<decltype(d)> dh;
  const auto mh = Dup128MaskFromMaskBits(dh, mask_bits);
  return CombineMasks(d, mh, mh);
}

// ------------------------------ Expand

template <typename T, HWY_IF_T_SIZE(T, 1)>
HWY_API Vec256<T> Expand(Vec256<T> v, Mask256<T> mask) {
  const DFromV<decltype(v)> d;
  // LUTs are infeasible for so many mask combinations, so Combine two
  // half-vector Expand.
  const Half<decltype(d)> dh;
  const uint64_t mask_bits = BitsFromMask(d, mask);
  constexpr size_t N = 32 / sizeof(T);
  const size_t countL = PopCount(mask_bits & ((1 << (N / 2)) - 1));
  const Mask128<T> maskL = MaskFromVec(LowerHalf(VecFromMask(d, mask)));
  const Vec128<T> expandL = Expand(LowerHalf(v), maskL);
  // We have to shift the input by a variable number of bytes, but there isn't
  // a table-driven option for that until VBMI, and CPUs with that likely also
  // have VBMI2 and thus native Expand.
  alignas(32) T lanes[N];
  Store(v, d, lanes);
  const Mask128<T> maskH = MaskFromVec(UpperHalf(dh, VecFromMask(d, mask)));
  const Vec128<T> expandH = Expand(LoadU(dh, lanes + countL), maskH);
  return Combine(d, expandH, expandL);
}

template <typename T, HWY_IF_T_SIZE(T, 2)>
HWY_API Vec256<T> Expand(Vec256<T> v, Mask256<T> mask) {
  const Full256<T> d;
  // LUTs are infeasible for 2^16 possible masks, so splice together two
  // half-vector Expand.
  const Half<decltype(d)> dh;
  const Mask128<T> maskL = MaskFromVec(LowerHalf(VecFromMask(d, mask)));
  const Vec128<T> expandL = Expand(LowerHalf(v), maskL);
  // We have to shift the input by a variable number of u16. permutevar_epi16
  // requires AVX3 and if we had that, we'd use native u32 Expand. The only
  // alternative is re-loading, which incurs a store to load forwarding stall.
  alignas(32) T lanes[32 / sizeof(T)];
  Store(v, d, lanes);
  const Vec128<T> vH = LoadU(dh, lanes + CountTrue(dh, maskL));
  const Mask128<T> maskH = MaskFromVec(UpperHalf(dh, VecFromMask(d, mask)));
  const Vec128<T> expandH = Expand(vH, maskH);
  return Combine(d, expandH, expandL);
}

template <typename T, HWY_IF_T_SIZE(T, 4)>
HWY_API Vec256<T> Expand(Vec256<T> v, Mask256<T> mask) {
  const Full256<T> d;
#if HWY_TARGET <= HWY_AVX3
  const RebindToUnsigned<decltype(d)> du;
  const MFromD<decltype(du)> mu = RebindMask(du, mask);
  return BitCast(d, detail::NativeExpand(BitCast(du, v), mu));
#else
  const RebindToUnsigned<decltype(d)> du;
  const uint64_t mask_bits = BitsFromMask(d, mask);

  alignas(16) constexpr uint32_t packed_array[256] = {
      // PrintExpand32x8Nibble.
      0xffffffff, 0xfffffff0, 0xffffff0f, 0xffffff10, 0xfffff0ff, 0xfffff1f0,
      0xfffff10f, 0xfffff210, 0xffff0fff, 0xffff1ff0, 0xffff1f0f, 0xffff2f10,
      0xffff10ff, 0xffff21f0, 0xffff210f, 0xffff3210, 0xfff0ffff, 0xfff1fff0,
      0xfff1ff0f, 0xfff2ff10, 0xfff1f0ff, 0xfff2f1f0, 0xfff2f10f, 0xfff3f210,
      0xfff10fff, 0xfff21ff0, 0xfff21f0f, 0xfff32f10, 0xfff210ff, 0xfff321f0,
      0xfff3210f, 0xfff43210, 0xff0fffff, 0xff1ffff0, 0xff1fff0f, 0xff2fff10,
      0xff1ff0ff, 0xff2ff1f0, 0xff2ff10f, 0xff3ff210, 0xff1f0fff, 0xff2f1ff0,
      0xff2f1f0f, 0xff3f2f10, 0xff2f10ff, 0xff3f21f0, 0xff3f210f, 0xff4f3210,
      0xff10ffff, 0xff21fff0, 0xff21ff0f, 0xff32ff10, 0xff21f0ff, 0xff32f1f0,
      0xff32f10f, 0xff43f210, 0xff210fff, 0xff321ff0, 0xff321f0f, 0xff432f10,
      0xff3210ff, 0xff4321f0, 0xff43210f, 0xff543210, 0xf0ffffff, 0xf1fffff0,
      0xf1ffff0f, 0xf2ffff10, 0xf1fff0ff, 0xf2fff1f0, 0xf2fff10f, 0xf3fff210,
      0xf1ff0fff, 0xf2ff1ff0, 0xf2ff1f0f, 0xf3ff2f10, 0xf2ff10ff, 0xf3ff21f0,
      0xf3ff210f, 0xf4ff3210, 0xf1f0ffff, 0xf2f1fff0, 0xf2f1ff0f, 0xf3f2ff10,
      0xf2f1f0ff, 0xf3f2f1f0, 0xf3f2f10f, 0xf4f3f210, 0xf2f10fff, 0xf3f21ff0,
      0xf3f21f0f, 0xf4f32f10, 0xf3f210ff, 0xf4f321f0, 0xf4f3210f, 0xf5f43210,
      0xf10fffff, 0xf21ffff0, 0xf21fff0f, 0xf32fff10, 0xf21ff0ff, 0xf32ff1f0,
      0xf32ff10f, 0xf43ff210, 0xf21f0fff, 0xf32f1ff0, 0xf32f1f0f, 0xf43f2f10,
      0xf32f10ff, 0xf43f21f0, 0xf43f210f, 0xf54f3210, 0xf210ffff, 0xf321fff0,
      0xf321ff0f, 0xf432ff10, 0xf321f0ff, 0xf432f1f0, 0xf432f10f, 0xf543f210,
      0xf3210fff, 0xf4321ff0, 0xf4321f0f, 0xf5432f10, 0xf43210ff, 0xf54321f0,
      0xf543210f, 0xf6543210, 0x0fffffff, 0x1ffffff0, 0x1fffff0f, 0x2fffff10,
      0x1ffff0ff, 0x2ffff1f0, 0x2ffff10f, 0x3ffff210, 0x1fff0fff, 0x2fff1ff0,
      0x2fff1f0f, 0x3fff2f10, 0x2fff10ff, 0x3fff21f0, 0x3fff210f, 0x4fff3210,
      0x1ff0ffff, 0x2ff1fff0, 0x2ff1ff0f, 0x3ff2ff10, 0x2ff1f0ff, 0x3ff2f1f0,
      0x3ff2f10f, 0x4ff3f210, 0x2ff10fff, 0x3ff21ff0, 0x3ff21f0f, 0x4ff32f10,
      0x3ff210ff, 0x4ff321f0, 0x4ff3210f, 0x5ff43210, 0x1f0fffff, 0x2f1ffff0,
      0x2f1fff0f, 0x3f2fff10, 0x2f1ff0ff, 0x3f2ff1f0, 0x3f2ff10f, 0x4f3ff210,
      0x2f1f0fff, 0x3f2f1ff0, 0x3f2f1f0f, 0x4f3f2f10, 0x3f2f10ff, 0x4f3f21f0,
      0x4f3f210f, 0x5f4f3210, 0x2f10ffff, 0x3f21fff0, 0x3f21ff0f, 0x4f32ff10,
      0x3f21f0ff, 0x4f32f1f0, 0x4f32f10f, 0x5f43f210, 0x3f210fff, 0x4f321ff0,
      0x4f321f0f, 0x5f432f10, 0x4f3210ff, 0x5f4321f0, 0x5f43210f, 0x6f543210,
      0x10ffffff, 0x21fffff0, 0x21ffff0f, 0x32ffff10, 0x21fff0ff, 0x32fff1f0,
      0x32fff10f, 0x43fff210, 0x21ff0fff, 0x32ff1ff0, 0x32ff1f0f, 0x43ff2f10,
      0x32ff10ff, 0x43ff21f0, 0x43ff210f, 0x54ff3210, 0x21f0ffff, 0x32f1fff0,
      0x32f1ff0f, 0x43f2ff10, 0x32f1f0ff, 0x43f2f1f0, 0x43f2f10f, 0x54f3f210,
      0x32f10fff, 0x43f21ff0, 0x43f21f0f, 0x54f32f10, 0x43f210ff, 0x54f321f0,
      0x54f3210f, 0x65f43210, 0x210fffff, 0x321ffff0, 0x321fff0f, 0x432fff10,
      0x321ff0ff, 0x432ff1f0, 0x432ff10f, 0x543ff210, 0x321f0fff, 0x432f1ff0,
      0x432f1f0f, 0x543f2f10, 0x432f10ff, 0x543f21f0, 0x543f210f, 0x654f3210,
      0x3210ffff, 0x4321fff0, 0x4321ff0f, 0x5432ff10, 0x4321f0ff, 0x5432f1f0,
      0x5432f10f, 0x6543f210, 0x43210fff, 0x54321ff0, 0x54321f0f, 0x65432f10,
      0x543210ff, 0x654321f0, 0x6543210f, 0x76543210,
  };

  // For lane i, shift the i-th 4-bit index down to bits [0, 3).
  const Vec256<uint32_t> packed = Set(du, packed_array[mask_bits]);
  alignas(32) constexpr uint32_t shifts[8] = {0, 4, 8, 12, 16, 20, 24, 28};
  // TableLookupLanes ignores upper bits; avoid bounds-check in IndicesFromVec.
  const Indices256<uint32_t> indices{(packed >> Load(du, shifts)).raw};
  const Vec256<uint32_t> expand = TableLookupLanes(BitCast(du, v), indices);
  // TableLookupLanes cannot also zero masked-off lanes, so do that now.
  return IfThenElseZero(mask, BitCast(d, expand));
#endif
}

template <typename T, HWY_IF_T_SIZE(T, 8)>
HWY_API Vec256<T> Expand(Vec256<T> v, Mask256<T> mask) {
  const Full256<T> d;
  const RebindToUnsigned<decltype(d)> du;
  const uint64_t mask_bits = BitsFromMask(d, mask);

  alignas(16) constexpr uint64_t packed_array[16] = {
      // PrintExpand64x4Nibble.
      0x0000ffff, 0x0000fff0, 0x0000ff0f, 0x0000ff10, 0x0000f0ff, 0x0000f1f0,
      0x0000f10f, 0x0000f210, 0x00000fff, 0x00001ff0, 0x00001f0f, 0x00002f10,
      0x000010ff, 0x000021f0, 0x0000210f, 0x00003210};

  // For lane i, shift the i-th 4-bit index down to bits [0, 2).
  const Vec256<uint64_t> packed = Set(du, packed_array[mask_bits]);
  alignas(32) constexpr uint64_t shifts[8] = {0, 4, 8, 12, 16, 20, 24, 28};
  // 64-bit TableLookupLanes on AVX2 requires IndicesFromVec, which checks
  // bounds, so clear the upper bits.
  const Vec256<uint64_t> masked = And(packed >> Load(du, shifts), Set(du, 3));
  const Indices256<uint64_t> indices = IndicesFromVec(du, masked);
  const Vec256<uint64_t> expand = TableLookupLanes(BitCast(du, v), indices);
  // TableLookupLanes cannot also zero masked-off lanes, so do that now.
  return IfThenElseZero(mask, BitCast(d, expand));
}

// ------------------------------ LoadExpand

template <class D, HWY_IF_V_SIZE_D(D, 32),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 1) | (1 << 2))>
HWY_API VFromD<D> LoadExpand(MFromD<D> mask, D d,
                             const TFromD<D>* HWY_RESTRICT unaligned) {
  return Expand(LoadU(d, unaligned), mask);
}

template <class D, HWY_IF_V_SIZE_D(D, 32),
          HWY_IF_T_SIZE_ONE_OF_D(D, (1 << 4) | (1 << 8))>
HWY_API VFromD<D> LoadExpand(MFromD<D> mask, D d,
                             const TFromD<D>* HWY_RESTRICT unaligned) {
  return Expand(LoadU(d, unaligned), mask);
}

// ------------------------------ LoadInterleaved3/4

// Implemented in generic_ops, we just overload LoadTransposedBlocks3/4.

namespace detail {
// Input:
// 1 0 (<- first block of unaligned)
// 3 2
// 5 4
// Output:
// 3 0
// 4 1
// 5 2
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void LoadTransposedBlocks3(D d, const TFromD<D>* HWY_RESTRICT unaligned,
                                   VFromD<D>& A, VFromD<D>& B, VFromD<D>& C) {
  constexpr size_t N = Lanes(d);
  const VFromD<D> v10 = LoadU(d, unaligned + 0 * N);  // 1 0
  const VFromD<D> v32 = LoadU(d, unaligned + 1 * N);
  const VFromD<D> v54 = LoadU(d, unaligned + 2 * N);

  A = ConcatUpperLower(d, v32, v10);
  B = ConcatLowerUpper(d, v54, v10);
  C = ConcatUpperLower(d, v54, v32);
}

// Input (128-bit blocks):
// 1 0 (first block of unaligned)
// 3 2
// 5 4
// 7 6
// Output:
// 4 0 (LSB of vA)
// 5 1
// 6 2
// 7 3
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void LoadTransposedBlocks4(D d, const TFromD<D>* HWY_RESTRICT unaligned,
                                   VFromD<D>& vA, VFromD<D>& vB, VFromD<D>& vC,
                                   VFromD<D>& vD) {
  constexpr size_t N = Lanes(d);
  const VFromD<D> v10 = LoadU(d, unaligned + 0 * N);
  const VFromD<D> v32 = LoadU(d, unaligned + 1 * N);
  const VFromD<D> v54 = LoadU(d, unaligned + 2 * N);
  const VFromD<D> v76 = LoadU(d, unaligned + 3 * N);

  vA = ConcatLowerLower(d, v54, v10);
  vB = ConcatUpperUpper(d, v54, v10);
  vC = ConcatLowerLower(d, v76, v32);
  vD = ConcatUpperUpper(d, v76, v32);
}
}  // namespace detail

// ------------------------------ StoreInterleaved2/3/4 (ConcatUpperLower)

// Implemented in generic_ops, we just overload StoreTransposedBlocks2/3/4.

namespace detail {
// Input (128-bit blocks):
// 2 0 (LSB of i)
// 3 1
// Output:
// 1 0
// 3 2
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void StoreTransposedBlocks2(VFromD<D> i, VFromD<D> j, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  constexpr size_t N = Lanes(d);
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatUpperUpper(d, j, i);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
}

// Input (128-bit blocks):
// 3 0 (LSB of i)
// 4 1
// 5 2
// Output:
// 1 0
// 3 2
// 5 4
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void StoreTransposedBlocks3(VFromD<D> i, VFromD<D> j, VFromD<D> k, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  constexpr size_t N = Lanes(d);
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatUpperLower(d, i, k);
  const auto out2 = ConcatUpperUpper(d, k, j);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  StoreU(out2, d, unaligned + 2 * N);
}

// Input (128-bit blocks):
// 4 0 (LSB of i)
// 5 1
// 6 2
// 7 3
// Output:
// 1 0
// 3 2
// 5 4
// 7 6
template <class D, HWY_IF_V_SIZE_D(D, 32)>
HWY_API void StoreTransposedBlocks4(VFromD<D> i, VFromD<D> j, VFromD<D> k,
                                    VFromD<D> l, D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  constexpr size_t N = Lanes(d);
  // Write lower halves, then upper.
  const auto out0 = ConcatLowerLower(d, j, i);
  const auto out1 = ConcatLowerLower(d, l, k);
  StoreU(out0, d, unaligned + 0 * N);
  StoreU(out1, d, unaligned + 1 * N);
  const auto out2 = ConcatUpperUpper(d, j, i);
  const auto out3 = ConcatUpperUpper(d, l, k);
  StoreU(out2, d, unaligned + 2 * N);
  StoreU(out3, d, unaligned + 3 * N);
}
}  // namespace detail

// ------------------------------ Additional mask logical operations

template <class T>
HWY_API Mask256<T> SetAtOrAfterFirst(Mask256<T> mask) {
  const Full256<T> d;
  const Repartition<int64_t, decltype(d)> di64;
  const Repartition<int32_t, decltype(d)> di32;
  const Half<decltype(di64)> dh_i64;
  const Half<decltype(di32)> dh_i32;
  using VI32 = VFromD<decltype(di32)>;

  auto vmask = BitCast(di64, VecFromMask(d, mask));
  vmask = Or(vmask, Neg(vmask));

  // Copy the sign bit of the even int64_t lanes to the odd int64_t lanes
  const auto vidx = Dup128VecFromValues(di32, 0, 0, 1 + 4, 1 + 4);
  const auto vmask2 =
      VI32{__lasx_xvshuf_w(vidx.raw, Zero(di32).raw, BitCast(di32, vmask).raw)};
  vmask = Or(vmask, BitCast(di64, BroadcastSignBit(vmask2)));

  // Copy the sign bit of the lower 128-bit half to the upper 128-bit half
  const auto vmask3 =
      BroadcastSignBit(Broadcast<3>(BitCast(dh_i32, LowerHalf(dh_i64, vmask))));
  vmask = Or(vmask, BitCast(di64, Combine(di32, vmask3, Zero(dh_i32))));
  return MaskFromVec(BitCast(d, vmask));
}

template <class T>
HWY_API Mask256<T> SetBeforeFirst(Mask256<T> mask) {
  return Not(SetAtOrAfterFirst(mask));
}

template <class T>
HWY_API Mask256<T> SetOnlyFirst(Mask256<T> mask) {
  const Full256<T> d;
  const RebindToSigned<decltype(d)> di;
  const Repartition<int64_t, decltype(d)> di64;
  const Half<decltype(di64)> dh_i64;

  const auto zero = Zero(di64);
  const auto vmask = BitCast(di64, VecFromMask(d, mask));

  const auto vmask_eq_0 = VecFromMask(di64, vmask == zero);
  auto vmask2_lo = LowerHalf(dh_i64, vmask_eq_0);
  auto vmask2_hi = UpperHalf(dh_i64, vmask_eq_0);

  vmask2_lo = And(vmask2_lo, InterleaveLower(vmask2_lo, vmask2_lo));
  vmask2_hi = And(ConcatLowerUpper(dh_i64, vmask2_hi, vmask2_lo),
                  InterleaveUpper(dh_i64, vmask2_lo, vmask2_lo));
  vmask2_lo = InterleaveLower(Set(dh_i64, int64_t{-1}), vmask2_lo);

  const auto vmask2 = Combine(di64, vmask2_hi, vmask2_lo);
  const auto only_first_vmask = Neg(BitCast(di, And(vmask, Neg(vmask))));
  return MaskFromVec(BitCast(d, And(only_first_vmask, BitCast(di, vmask2))));
}

template <class T>
HWY_API Mask256<T> SetAtOrBeforeFirst(Mask256<T> mask) {
  const Full256<T> d;
  constexpr size_t kLanesPerBlock = MaxLanes(d) / 2;

  const auto vmask = VecFromMask(d, mask);
  const auto vmask_lo = ConcatLowerLower(d, vmask, Zero(d));
  return SetBeforeFirst(
      MaskFromVec(CombineShiftRightBytes<(kLanesPerBlock - 1) * sizeof(T)>(
          d, vmask, vmask_lo)));
}

// ------------------------------ LeadingZeroCount

template <class V, HWY_IF_UI32(TFromV<V>), HWY_IF_V_SIZE_V(V, 32)>
HWY_API V LeadingZeroCount(V v) {
  return V{__lasx_xvclz_w(v.raw)};
}

template <class V, HWY_IF_UI64(TFromV<V>), HWY_IF_V_SIZE_V(V, 32)>
HWY_API V LeadingZeroCount(V v) {
  return V{__lasx_xvclz_d(v.raw)};
}

namespace detail {

template <class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2)),
          HWY_IF_LANES_LE_D(DFromV<V>, HWY_MAX_BYTES / 4)>
static HWY_INLINE HWY_MAYBE_UNUSED V Lzcnt32ForU8OrU16OrU32(V v) {
  const DFromV<decltype(v)> d;
  const Rebind<int32_t, decltype(d)> di32;
  const Rebind<uint32_t, decltype(d)> du32;

  const auto v_lz_count = LeadingZeroCount(PromoteTo(du32, v));
  return DemoteTo(d, BitCast(di32, v_lz_count));
}

template <class V, HWY_IF_UNSIGNED_V(V), HWY_IF_T_SIZE_V(V, 4)>
static HWY_INLINE HWY_MAYBE_UNUSED V Lzcnt32ForU8OrU16OrU32(V v) {
  return LeadingZeroCount(v);
}

template <class V, HWY_IF_UNSIGNED_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2)),
          HWY_IF_LANES_GT_D(DFromV<V>, HWY_MAX_BYTES / 4)>
static HWY_INLINE HWY_MAYBE_UNUSED V Lzcnt32ForU8OrU16OrU32(V v) {
  const DFromV<decltype(v)> d;
  const RepartitionToWide<decltype(d)> dw;
  const RebindToSigned<decltype(dw)> dw_i;

  const auto lo_v_lz_count = Lzcnt32ForU8OrU16OrU32(PromoteLowerTo(dw, v));
  const auto hi_v_lz_count = Lzcnt32ForU8OrU16OrU32(PromoteUpperTo(dw, v));
  return OrderedDemote2To(d, BitCast(dw_i, lo_v_lz_count),
                          BitCast(dw_i, hi_v_lz_count));
}

}  // namespace detail

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2))>
HWY_API V LeadingZeroCount(V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;

  constexpr TU kNumOfBitsInT{sizeof(TU) * 8};
  const auto v_lzcnt32 = detail::Lzcnt32ForU8OrU16OrU32(BitCast(du, v));
  return BitCast(d, Min(v_lzcnt32 - Set(du, TU{32 - kNumOfBitsInT}),
                        Set(du, TU{kNumOfBitsInT})));
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 1) | (1 << 2))>
HWY_API V HighestSetBitIndex(V v) {
  const DFromV<decltype(v)> d;
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  return BitCast(
      d, Set(du, TU{31}) - detail::Lzcnt32ForU8OrU16OrU32(BitCast(du, v)));
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V),
          HWY_IF_T_SIZE_ONE_OF_V(V, (1 << 4) | (1 << 8))>
HWY_API V HighestSetBitIndex(V v) {
  const DFromV<decltype(v)> d;
  using T = TFromD<decltype(d)>;
  return BitCast(d, Set(d, T{sizeof(T) * 8 - 1}) - LeadingZeroCount(v));
}

template <class V, HWY_IF_NOT_FLOAT_NOR_SPECIAL_V(V)>
HWY_API V TrailingZeroCount(V v) {
  const DFromV<decltype(v)> d;
  const RebindToSigned<decltype(d)> di;
  using T = TFromD<decltype(d)>;

  const auto vi = BitCast(di, v);
  const auto lowest_bit = BitCast(d, And(vi, Neg(vi)));
  constexpr T kNumOfBitsInT{sizeof(T) * 8};
  const auto bit_idx = HighestSetBitIndex(lowest_bit);
  return IfThenElse(MaskFromVec(bit_idx), Set(d, kNumOfBitsInT), bit_idx);
}

// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();

// Note that the GCC warnings are not suppressed if we only wrap the *intrin.h -
// the warning seems to be issued at the call site of intrinsics, i.e. our code.
HWY_DIAGNOSTICS(pop)
