#pragma once

#include <string>
#include <vector>

#include "definitions.hpp"
#include "process.hpp"
#include "vmachine.hpp"

namespace processwarp {
class BuiltinGuiDelegate {
 public:
  virtual ~BuiltinGuiDelegate();

  virtual void builtin_gui_send_command(Process& proc, InnerModule::Type module,
                                        const picojson::object& content) = 0;

  virtual void builtin_gui_send_frontend_packet(Process& proc, const std::string& content) = 0;
};

class BuiltinGui {
 public:
  static BuiltinPostProc::Type create(Process& proc, Thread& thread, BuiltinFuncParam p,
                                      vaddr_t dst, std::vector<uint8_t>& src);
  static BuiltinPostProc::Type script(Process& proc, Thread& thread, BuiltinFuncParam p,
                                      vaddr_t dst, std::vector<uint8_t>& src);

  static void regist(VMachine& vm, BuiltinGuiDelegate& delegate);
};
}  // namespace processwarp
