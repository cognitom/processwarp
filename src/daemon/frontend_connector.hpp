#pragma once

#include <picojson.h>
#include <uv.h>

#include <map>

#include "connector.hpp"
#include "daemon_define.hpp"

namespace processwarp {
class FrontendConnector : public Connector {
 public:
  static FrontendConnector& get_instance();

  void initialize(uv_loop_t* loop);
  void send_connect_frontend(uv_pipe_t& client, int result);

 private:
  struct FrontendProperty {
    PipeStatus::Type status;
    FrontendType::Type type;
  };

  /** Map of pipe and propertiy. */
  std::map<const uv_pipe_t*, FrontendProperty> properties;

  FrontendConnector();
  FrontendConnector(const FrontendConnector&);
  FrontendConnector& operator=(const FrontendConnector&);

  void on_connect(uv_pipe_t& client) override;
  void on_recv_packet(uv_pipe_t& client, picojson::object& packet) override;
  void on_close(uv_pipe_t& client) override;

  void recv_connect_frontend(uv_pipe_t& client, picojson::object& packet);
  void recv_open_file(uv_pipe_t& client, picojson::object& packet);
};
}  // namespace processwarp