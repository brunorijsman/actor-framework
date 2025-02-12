// Simple HTTPS server that tells the time.

#include "caf/actor_system.hpp"
#include "caf/actor_system_config.hpp"
#include "caf/caf_main.hpp"
#include "caf/deep_to_string.hpp"
#include "caf/event_based_actor.hpp"
#include "caf/net/http/serve.hpp"
#include "caf/net/middleman.hpp"
#include "caf/net/ssl/acceptor.hpp"
#include "caf/net/ssl/context.hpp"
#include "caf/net/stream_transport.hpp"
#include "caf/net/tcp_accept_socket.hpp"
#include "caf/net/tcp_stream_socket.hpp"
#include "caf/scheduled_actor/flow.hpp"

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

using namespace std::literals;

static constexpr uint16_t default_port = 8080;

struct config : caf::actor_system_config {
  config() {
    opt_group{custom_options_, "global"} //
      .add<uint16_t>("port,p", "port to listen for incoming connections")
      .add<std::string>("cert-file", "PEM server certificate file")
      .add<std::string>("key-file", "PEM key file for the certificate");
  }
};

int caf_main(caf::actor_system& sys, const config& cfg) {
  using namespace caf;
  // Sanity checking.
  auto cert_file = get_or(cfg, "cert-file", ""sv);
  auto key_file = get_or(cfg, "key-file", ""sv);
  if (cert_file.empty()) {
    std::cerr << "*** mandatory parameter 'cert-file' missing or empty\n";
    return EXIT_FAILURE;
  }
  if (key_file.empty()) {
    std::cerr << "*** mandatory parameter 'key-file' missing or empty\n";
    return EXIT_FAILURE;
  }
  // Open up a TCP port for incoming connections.
  auto port = caf::get_or(cfg, "port", default_port);
  auto acc = net::ssl::acceptor::make_with_cert_file(port, cert_file, key_file);
  if (!acc) {
    std::cerr << "*** unable to initialize TLS: " << to_string(acc.error())
              << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "*** started listening for incoming connections on port " << port
            << '\n';
  // Create buffers to signal events from the WebSocket server to the worker.
  auto [worker_pull, server_push] = net::http::make_request_resource();
  // Spin up the HTTP server.
  net::http::serve(sys, std::move(*acc), std::move(server_push));
  // Spin up a worker to handle the HTTP requests.
  auto worker = sys.spawn([wres = worker_pull](event_based_actor* self) {
    // For each incoming request ...
    wres
      .observe_on(self) //
      .for_each([](const net::http::request& req) {
        // ... we simply return the current time as string.
        // Note: we cannot respond more than once to a request.
        auto str = caf::deep_to_string(make_timestamp());
        req.respond(net::http::status::ok, "text/plain", str);
      });
  });
  sys.await_all_actors_done();
  return EXIT_SUCCESS;
}

CAF_MAIN(caf::net::middleman)
