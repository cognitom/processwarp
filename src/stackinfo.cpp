
#include "data_store.hpp"
#include "func_store.hpp"
#include "stackinfo.hpp"

using namespace usagi;

// コンストラクタ。
StackInfo::StackInfo(FuncStore& func_,
		     vaddr_t ret_addr_,
		     unsigned int normal_pc_,
		     unsigned int unwind_pc_,
		     DataStore& stack_) :
  func(&func_ == nullptr ? VADDR_NULL : func_.addr),
  func_cache(func_),
  ret_addr(ret_addr_),
  normal_pc(normal_pc_),
  unwind_pc(unwind_pc_),
  stack(stack_.addr),
  stack_cache(stack_),
  var_arg(VADDR_NON),
  pc(0),
  phi0(0),
  phi1(0),
  type(VADDR_NON),
  type_cache1(nullptr),
  type_cache2(nullptr),
  output(VADDR_NON),
  output_cache(nullptr),
  value(VADDR_NON),
  value_cache(nullptr),
  address(VADDR_NON),
  address_cache(nullptr) {
}