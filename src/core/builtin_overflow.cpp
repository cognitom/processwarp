
#include <vector>

#include "builtin_overflow.hpp"
#include "process.hpp"

#if defined(__clang__) && (__clang_major__ > 3 || (__clang_major__ == 3 && __clang_minor__ >= 6))
#  pragma clang diagnostic push
#  pragma clang diagnostic ignored "-Wunused-local-typedef"
#  include "safeint3.hpp"
#  pragma clang diagnostic pop
#elif defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wattributes"
#  include "safeint3.hpp"
#  pragma GCC diagnostic pop
#else
#  include "safeint3.hpp"
#endif

namespace processwarp {

/**
 * @param SFUNC SafeIntの演算関数
 * @param INT_T 計算のベースになる整数型
 * @param WIDTH 整数のビット幅
 */
#define M_CASE_PER_WIDTH(SFUNC, INT_T, WIDTH)                           \
  case WIDTH: {                                                         \
    INT_T a = static_cast<INT_T>(Process::read_builtin_param_i##WIDTH(src, &seek)); \
    INT_T b = static_cast<INT_T>(Process::read_builtin_param_i##WIDTH(src, &seek)); \
    INT_T res;                                                          \
    bool flg = SFUNC<INT_T, INT_T>(a, b, res);                          \
    thread.memory->write<INT_T>(dst, res);                              \
    thread.memory->write<uint8_t>(dst + sizeof(INT_T), flg ? 0x00 : 0xff); \
  } break

/**
 * @param FNAME 作成する関数名
 * @param SFUNC SafeIntの演算関数
 * @param I16 16bit幅の計算のベースになる整数型
 * @param I32 32bit幅の計算のベースになる整数型
 * @param I64 64bit幅の計算のベースになる整数型
 */
#define M_FUNC_PER_METHOD(FNAME, SFUNC, I16, I32, I64)                  \
  BuiltinPostProc::Type BuiltinOverflow::FNAME(Process& proc, Thread& thread, \
                                               BuiltinFuncParam p, vaddr_t dst, \
                                               std::vector<uint8_t>& src) { \
    int seek = 0;                                                       \
    switch (p.i64) {                                                    \
      M_CASE_PER_WIDTH(SFUNC, I16, 16);                                 \
      M_CASE_PER_WIDTH(SFUNC, I32, 32);                                 \
      M_CASE_PER_WIDTH(SFUNC, I64, 64);                                 \
      default: assert(false); break;                                    \
    }                                                                   \
    return BuiltinPostProc::NORMAL;                                     \
  }

M_FUNC_PER_METHOD(sadd, SafeAdd,      int16_t, int32_t, int64_t)
M_FUNC_PER_METHOD(smul, SafeMultiply, int16_t, int32_t, int64_t)
M_FUNC_PER_METHOD(ssub, SafeSubtract, int16_t, int32_t, int64_t)
M_FUNC_PER_METHOD(uadd, SafeAdd,      uint16_t, uint32_t, uint64_t)
M_FUNC_PER_METHOD(umul, SafeMultiply, uint16_t, uint32_t, uint64_t)
M_FUNC_PER_METHOD(usub, SafeSubtract, uint16_t, uint32_t, uint64_t)

#undef M_FUNC_PER_METHOD
#undef M_CASE_PER_WIDTH

// VMにライブラリを登録する。
void BuiltinOverflow::regist(VMachine& vm) {
  vm.regist_builtin_func("llvm.sadd.with.overflow.i16", BuiltinOverflow::sadd, 16);
  vm.regist_builtin_func("llvm.sadd.with.overflow.i32", BuiltinOverflow::sadd, 32);
  vm.regist_builtin_func("llvm.sadd.with.overflow.i64", BuiltinOverflow::sadd, 64);

  vm.regist_builtin_func("llvm.smul.with.overflow.i16", BuiltinOverflow::smul, 16);
  vm.regist_builtin_func("llvm.smul.with.overflow.i32", BuiltinOverflow::smul, 32);
  vm.regist_builtin_func("llvm.smul.with.overflow.i64", BuiltinOverflow::smul, 64);

  vm.regist_builtin_func("llvm.ssub.with.overflow.i16", BuiltinOverflow::ssub, 16);
  vm.regist_builtin_func("llvm.ssub.with.overflow.i32", BuiltinOverflow::ssub, 32);
  vm.regist_builtin_func("llvm.ssub.with.overflow.i64", BuiltinOverflow::ssub, 64);

  vm.regist_builtin_func("llvm.uadd.with.overflow.i16", BuiltinOverflow::uadd, 16);
  vm.regist_builtin_func("llvm.uadd.with.overflow.i32", BuiltinOverflow::uadd, 32);
  vm.regist_builtin_func("llvm.uadd.with.overflow.i64", BuiltinOverflow::uadd, 64);

  vm.regist_builtin_func("llvm.umul.with.overflow.i16", BuiltinOverflow::umul, 16);
  vm.regist_builtin_func("llvm.umul.with.overflow.i32", BuiltinOverflow::umul, 32);
  vm.regist_builtin_func("llvm.umul.with.overflow.i64", BuiltinOverflow::umul, 64);

  vm.regist_builtin_func("llvm.usub.with.overflow.i16", BuiltinOverflow::usub, 16);
  vm.regist_builtin_func("llvm.usub.with.overflow.i32", BuiltinOverflow::usub, 32);
  vm.regist_builtin_func("llvm.usub.with.overflow.i64", BuiltinOverflow::usub, 64);
}
}  // namespace processwarp
