#pragma once

#include <picojson.h>

#include <map>
#include <memory>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "process.hpp"
#include "vmemory.hpp"

namespace processwarp {
class BuiltinGuiDelegate;
class VMachine;

/**
 * Delegate for VMachine-class.
 */
class VMachineDelegate {
 public:
  virtual ~VMachineDelegate();

  virtual void vmachine_send_packet(VMachine& vm, const nid_t& dst_nid,
                                    const std::string& packet) = 0;
  virtual void vmachine_finish(VMachine& vm) = 0;
  virtual void vmachine_finish_thread(VMachine& vm, const vtid_t& tid) = 0;
  virtual void vmachine_error(VMachine& vm, const std::string& message) = 0;
};

/**
 * VMachine for set of processes.
 */
class VMachine : private ProcessDelegate {
 public:
  /** VMachine's node-id. */
  const nid_t my_nid;
  /** Virtual memory for this virtual machine. */
  VMemory vmemory;


  VMachine(VMachineDelegate& delegate_,
           VMemoryDelegate& memory_delegate,
           const nid_t& my_nid_,
           const std::vector<void*>& libs_,
           const std::map<std::string, std::string>& lib_filter_);
  void initialize(const vpid_t& pid, const vtid_t& root_tid,
                  vaddr_t proc_addr, const nid_t& master_nid);
  void initialize_gui(BuiltinGuiDelegate& delegate);
  void execute();
  void terminate();

  void on_recv_update(vaddr_t addr);
  void recv_command(const picojson::object& content);
  void recv_packet(const std::string& data);
  Process& get_process();
  void warpout_thread(vtid_t tid);
  void regist_builtin_func(const std::string& name, builtin_func_t func, int i64);
  void regist_builtin_func(const std::string& name, builtin_func_t func, void* ptr);

  void request_warp_thread(const vtid_t tid, const nid_t& dst_node);

  std::unique_ptr<VMemory::Accessor> process_assign_accessor(const vpid_t& pid) override;
  void process_change_thread_set(Process& process) override;

 private:
  /** Event assignee */
  VMachineDelegate& delegate;
  /** Loaded external libraries for ffi. */
  const std::vector<void*>& libs;
  /**
   * Map of API name call from and call for that can access.
   * Key:API nam call from application.
   * Value:API name call for OS.
   */
  const std::map<std::string, std::string>& lib_filter;
  /** Process instance. */
  std::unique_ptr<Process> process;
  /** Map of API name and built-in function pointer and parameter. */
  std::map<std::string, std::pair<builtin_func_t, BuiltinFuncParam>> builtin_funcs;
  /** Executable threads pool in this node's process. */
  std::queue<vtid_t> loop_queue;

  void initialize_builtin();

  void kill_defunct_thread(const ProcessTree& sv_proc);
  void clean_defunct_processe(const ProcessTree& sv_proc);
  void clean_defunct_memoryspace(const ProcessTree& sv_proc);

  void recv_warp(picojson::object& json);
  void recv_warp_request(picojson::object& json);
  void recv_terminate(picojson::object& json);
  void send_packet(const nid_t& dst_nid, const std::string& command, picojson::object& packet);
  void send_warp(Thread& thread);
  void send_warp_request(const vtid_t tid, const nid_t& dst_nid);
  void send_terminate();
};
}  // namespace processwarp
