
#include <unistd.h>
#include <uv.h>

#include <cassert>
#include <string>

#include "constant.hpp"
#include "convert.hpp"
#include "daemon_mid.hpp"
#include "logger.hpp"
#include "router.hpp"
#include "frontend_connector.hpp"
#include "server_connector.hpp"
#include "worker_connector.hpp"

namespace processwarp {
#ifndef WORKER_DUMMY
/**
 * WorkerConnector instance getter as singleton pattern.
 * @return The singleton instance of WorkerConnector class.
 */
WorkerConnector& WorkerConnector::get_instance() {
  static WorkerConnector instance;
  return instance;
}

/**
 * Constructor for singleton pattern.
 * This class is singleton.
 * This method is private.
 */
WorkerConnector::WorkerConnector() {
}

/**
 * Initialize for WorkerConnector.
 * Create UNIX domain socket for worker.
 * @param loop libuv's loop for WorkerConnector.
 * @param pipe_path_ Path of pipe that for connecting with worker.
 * @param libs Library pathes to send to worker process.
 * @param lib_filter Library filter to send to worker process.
 */
void WorkerConnector::initialize(uv_loop_t* loop, const std::string& pipe_path_,
                                 const picojson::array& libs, const picojson::array& lib_filter) {
  pipe_path     = pipe_path_;
  config_libs   = libs;
  config_lib_filter = lib_filter;

  Connector::initialize(loop, pipe_path);
}

/**
 * Create new worker process for virtual machine.
 * Create new process with parameter there are need to create vm.
 * Bundle property with worker and initialize status.
 * The process shuld connect pipt after launch.
 * @param pid Process-id for new vm.
 * @param root_tid Root thread-id for new vm.
 * @param proc_addr Address of process information for new vm.
 * @param master_nid Node-id of master node for new vm.
 * @param name Process name for new vm.
 */
void WorkerConnector::create_vm(const vpid_t& pid, vtid_t root_tid, vaddr_t proc_addr,
                                const nid_t& master_nid, const std::string& name) {
  std::string worker_path = Util::file_dirname(Util::get_my_fullpath()) + "/worker";
  Router& router = Router::get_instance();

  // Exists vm for pid yet.
  if (properties.find(pid) != properties.end()) {
    return;
  }

  properties.insert(std::make_pair(pid, WorkerProperty()));
  WorkerProperty& property = properties.at(pid);
  property.status = PipeStatus::SETUP;
  property.pid    = pid;
  property.pipe   = nullptr;
  property.process.data = &property;

  char* args[] = {
    // const_cast<char*>(VALGRIND_PATH),
    const_cast<char*>(worker_path.c_str()),
    const_cast<char*>(pipe_path.c_str()),
    const_cast<char*>(Convert::vpid2str(pid).c_str()),
    nullptr
  };

  uv_stdio_container_t worker_stdio[3];
  worker_stdio[0].flags = UV_IGNORE;
  worker_stdio[1].flags = UV_INHERIT_FD;
  worker_stdio[1].data.fd = STDOUT_FILENO;
  worker_stdio[2].flags = UV_INHERIT_FD;
  worker_stdio[2].data.fd = STDERR_FILENO;

  uv_process_options_t options;
  memset(&options, 0, sizeof(options));
  options.exit_cb = WorkerConnector::on_exit;
  // options.file    = VALGRIND_PATH;
  options.file    = worker_path.c_str();
  options.args    = args;
  options.stdio_count = 3;
  options.stdio   = worker_stdio;

  int r = uv_spawn(loop, &property.process, &options);
  if (r) {
    /// @todo error
    Logger::err(DaemonMid::L3007, "uv_spawn", uv_err_name(r));
    assert(false);
  }

  // Send initialize data.
  picojson::object connect_data;
  connect_data.insert(std::make_pair("command", picojson::value(std::string("connect_worker"))));
  connect_data.insert(std::make_pair("my_nid", Convert::nid2json(router.get_my_nid())));
  connect_data.insert(std::make_pair("root_tid", Convert::vtid2json(root_tid)));
  connect_data.insert(std::make_pair("proc_addr", Convert::vaddr2json(proc_addr)));
  connect_data.insert(std::make_pair("master_nid", Convert::nid2json(master_nid)));
  connect_data.insert(std::make_pair("name", picojson::value(std::string(name))));
  connect_data.insert(std::make_pair("libs", picojson::value(config_libs)));
  connect_data.insert(std::make_pair("lib_filter", picojson::value(config_lib_filter)));
  send_data(pid, connect_data);
}

/**
 * Relay command to VM or memory by through a worker.
 * @param packet Command packet.
 */
void WorkerConnector::relay_command(const CommandPacket& packet) {
  assert(packet.module == Module::MEMORY || packet.module == Module::VM);

  if (properties.find(packet.pid) == properties.end()) return;

  picojson::object data;
  data.insert(std::make_pair("command", picojson::value(std::string("relay_command"))));
  data.insert(std::make_pair("pid", Convert::vpid2json(packet.pid)));
  data.insert(std::make_pair("dst_nid", Convert::nid2json(packet.dst_nid)));
  data.insert(std::make_pair("src_nid", Convert::nid2json(packet.src_nid)));
  data.insert(std::make_pair("module", Convert::int2json<Module::Type>(packet.module)));
  data.insert(std::make_pair("content", picojson::value(packet.content)));

  send_data(packet.pid, data);
}

/**
 * When worker process was finished, close a hundle and remove a property it bundled to worker.
 * @param req
 * @param exit_status
 * @param term_signal
 */
void WorkerConnector::on_exit(uv_process_t *req, int64_t exit_status, int term_signal) {
  WorkerConnector& THIS = WorkerConnector::get_instance();
  WorkerProperty& property = *reinterpret_cast<WorkerProperty*>(req->data);

  uv_close(reinterpret_cast<uv_handle_t*>(&property.process), nullptr);
  THIS.properties.erase(property.pid);
}

/**
 * When pipe was connect with worker, do nothing.
 * @param client Pipe connect with worker by libuv.
 */
void WorkerConnector::on_connect(uv_pipe_t& client) {
}

/**
 * When receive data, call capable methods by command type.
 * @param client Pipe connect with worker by libuv.
 * @param data Received JSON packet.
 */
void WorkerConnector::on_recv_data(uv_pipe_t& client, picojson::object& data) {
  std::string& command = data.at("command").get<std::string>();

  if (command == "relay_command") {
    recv_relay_command(pid_map.at(&client), data);

  } else if (command == "connect_worker") {
    recv_connect_worker(client, data);

  } else {
    /// @todo error
    assert(false);
  }
}

/**
 * When pipe was close, remove map of pipe and pid.
 * @param client Pipe disconnect by worker.
 */
void WorkerConnector::on_close(uv_pipe_t& client) {
  pid_map.erase(&client);
}

/**
 * When receive connect_worker command, bundle process-id with worker and pipe.
 * If packets waiting to connect are exist, relay there packets.
 * @param pipe Pipe connected just now.
 * @param param Parameter contain pid parameter of command.
 */
void WorkerConnector::recv_connect_worker(uv_pipe_t& pipe, picojson::object& param) {
  vpid_t pid = Convert::json2vpid(param.at("pid"));

  if (properties.find(pid) == properties.end()) {
    /// @todo error
    assert(false);
  }

  WorkerProperty& property = properties.at(pid);
  if (property.pipe != nullptr) {
    /// @todo error
    assert(false);
  }
  property.status = PipeStatus::CONNECT;
  property.pipe = &pipe;
  pid_map.insert(std::make_pair(&pipe, pid));

  for (auto& wait_one : property.send_wait) {
    Connector::send_data(*property.pipe, wait_one);
  }
  property.send_wait.clear();
}

/**
 * When receive command, relay a packet to target module through router and server if need.
 * @param pid Process-id send from.
 * @param content Packet content that contain command string and parameters.
 */
void WorkerConnector::recv_relay_command(const vpid_t& pid, picojson::object& content) {
  CommandPacket packet = {
    pid,
    Convert::json2nid(content.at("dst_nid")),
    NID::NONE,
    Convert::json2int<Module::Type>(content.at("module")),
    content.at("content").get<picojson::object>()
  };

  Router& router = Router::get_instance();
  router.relay_command(packet, false);
}

/**
 * Send data to specified worker.
 * @param pid Process-id to specifing worker.
 * @param data Packet to send.
 */
void WorkerConnector::send_data(const vpid_t& pid, const picojson::object& data) {
  WorkerProperty& property = properties.at(pid);

  if (property.status == PipeStatus::CONNECT) {
    Connector::send_data(*property.pipe, data);

  } else if (property.status == PipeStatus::SETUP) {
    property.send_wait.push_back(data);

  } else {
    /// @todo error
    assert(false);
  }
}
#endif  // ifndef WORKER_DUMMY
}  // namespace processwarp
