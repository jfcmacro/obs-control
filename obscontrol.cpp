#include <iostream>
#include <cstdlib>
#include <string>
#include <sys/types.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include "obscontrol.h"

typedef websocketpp::client<websocketpp::config::asio_client> client;

int messages = 0;

void on_message(websocketpp::connection_hdl, client::message_ptr msg) {
	std::cout << msg->get_payload() << std::endl;
	messages++;
}

int
main(int argc, char *argv[]) {

  pid_t child = fork();

  if (child == 0) {
    ::execlp("obs", "obs",
	   "--scene", "Docente", "--profile", "Docente",
	   "--websocket_port", "4888",
	     // "--websocket_password", "abcd1234",
	   "--websocket_debug", "true", nullptr);
    ::exit(100);
  }
  // else {
  // }

  client c;

  std::string uri = "ws://localhost:4888";

  try {
    // Set logging to be pretty verbose (everything except message payloads)
    c.set_access_channels(websocketpp::log::alevel::all);
    c.clear_access_channels(websocketpp::log::alevel::frame_payload);
    c.set_error_channels(websocketpp::log::elevel::all);

    // Initialize ASIO
    c.init_asio();

    // Register our message handler
    c.set_message_handler(&on_message);

    websocketpp::lib::error_code ec;
    client::connection_ptr con = c.get_connection(uri, ec);
    if (ec) {
      std::cout << "could not create connection because: " << ec.message() << std::endl;
      return 0;
    }

    // Note that connect here only requests a connection. No network messages are
    // exchanged until the event loop starts running in the next line.
    c.connect(con);

    // Start the ASIO io_service run loop
    // this will cause a single connection to be made to the server. c.run()
    // will exit when this connection is closed.
    c.run();
  } catch (websocketpp::exception const & e) {
    std::cout << e.what() << std::endl;
  }
  int status;

  ::waitpid(child, &status, 0);

  std::cout << "[OBSControl] Ending with status: " << status
	    << " and some times: " << messages << std::endl;
  return EXIT_SUCCESS;
}
