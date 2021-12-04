#include <iostream>
#include <sstream>
#include <vector>
#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <string>
#include <cstdlib>
#include <cctype>
#include <thread>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <json/json.h>
#include <openssl/sha.h>
#include <utf8.h>
#include "obscontrol.h"
#include "obswebsocket.h"

#define PORT_1 "4800"
#define PASS_1 "abcd1234"
#define HOSTNAME_1 "localhost"
#define PORT_2 "4900"
#define PASS_2 "1234abcd"
#define HOSTNAME_2 "localhost"
#define WAIT_TIME 5

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

// websocketpp::connection_hdl global_hdl;
int requestId = 100;
const std::string localhost { "localhost" };
const std::string wsproto { "ws" };

std::string
getMsgIdentify(std::string& authentication,
	       const int rpcVersion,
	       bool ignoreInvalidMessage,
	       EventSubscription eventSubscription) {
  Json::Value jsonAns;
  Json::Value jsonAnsData;

  jsonAnsData["rpcVersion"] = rpcVersion;
  if (authentication != "")
    jsonAnsData["authentication"] = authentication;
  jsonAnsData["ignoreInvalidMessages"] = true;
  jsonAnsData["eventSubscriptions"] = All;

  jsonAns["d"] = jsonAnsData;
  jsonAns["op"] = Identify;

  Json::StreamWriterBuilder writerBuilder;
  return Json::writeString(writerBuilder, jsonAns);
}

std::string
getMsgRequest(std::string& requestType) {
  Json::Value jsonAns;
  Json::Value jsonAnsData;

  jsonAnsData["requestType"] = requestType;
  jsonAnsData["requestId"] = requestId++;

  jsonAns["d"] = jsonAnsData;
  jsonAns["op"] = Request;

  Json::StreamWriterBuilder writerBuilder;

  return Json::writeString(writerBuilder, jsonAns);
}

bool
sendSimpleRequest(client* c,
		  websocketpp::connection_hdl hdl,
		  std::string& reqMsg) {
  websocketpp::lib::error_code ec;
  bool retVal = true;

  c->send(hdl, reqMsg, websocketpp::frame::opcode::text, ec);

  if (ec) {
    bool retVal = false;
    std::cerr << "Error sending because: " << ec.message() <<  std::endl;
  }

  return retVal;
}

struct OBSConnection {
  client c;
  pid_t child;
  std::thread *pThr;
  websocketpp::connection_hdl hdl;
  std::string port;
  std::string pass;
  std::string ipohost;
  std::string proto;
  std::string scene;
  std::string profile;
  bool debug;

  OBSConnection(std::string& port,
		std::string& pass,
		std::string& scene,
		std::string& profile,
		const std::string& ipohost = localhost,
		const std::string& proto = wsproto,
		bool debug = false) :
    c(),
    port(port),
    pass(pass),
    scene(scene),
    profile(profile),
    ipohost(ipohost),
    proto(proto),
    debug(debug) {
    pThr = new std::thread([this]{ this->run(); });
  }

  static void on_message(OBSConnection* obscon, client* c,
			 websocketpp::connection_hdl hdl,
			 message_ptr msg) {

    std::cout << "[obscontrol] " << msg->get_payload() << std::endl;

    std::string rawJson(msg->get_payload());
    int rawJsonLength = static_cast<int>(rawJson.length());
    Json::Value jsonMsg;
    JSONCPP_STRING err;

    Json::CharReaderBuilder readBuilder;
    const std::unique_ptr<Json::CharReader> reader(readBuilder.newCharReader());

    if (!reader->parse(rawJson.c_str(),
		       rawJson.c_str() + rawJsonLength,
		       &jsonMsg, &err)) {

      std::cerr << "Json Parser: " << msg->get_payload() << std::endl;
      ::exit(EXIT_FAILURE); // TODO This is too drastic. Mananged it!
    }

    WebSocketOpCode opCode = static_cast<WebSocketOpCode>(jsonMsg["op"].asInt());

    switch (opCode) {
    case Hello:
      {
	const int rpcVersion = jsonMsg["d"]["rpcVersion"].asInt();

	if (jsonMsg["d"].isMember("authentication")) {
	  std::string salt(jsonMsg["d"]["authentication"]["salt"].asString());
	  // std::string password();

	  QString passwordAndSalt = "";
	  passwordAndSalt += QString::fromStdString(obscon->pass);
	  passwordAndSalt += QString::fromStdString(salt);

	  auto passwordAndSaltHash = QCryptographicHash::hash(passwordAndSalt.toUtf8(),
							      QCryptographicHash::Algorithm::Sha256);

	  std::string base64_secret = passwordAndSaltHash.toBase64().toStdString();
	  std::string challenge(jsonMsg["d"]["authentication"]["challenge"].asString());

	  QString secretAndChallenge = "";
	  secretAndChallenge += QString::fromStdString(base64_secret);
	  secretAndChallenge += QString::fromStdString(challenge);

	  auto secretAndChallengeHash = QCryptographicHash::hash(secretAndChallenge.toUtf8(),
								 QCryptographicHash::Algorithm::Sha256);

	  std::string authentication = secretAndChallengeHash.toBase64().toStdString();

	  std::cout << authentication << std::endl;

	  std::string retMsg = getMsgIdentify(authentication,
					      rpcVersion,
					      true,
					      All);

	  sendSimpleRequest(c, hdl, retMsg);
	}
	else {
	  std::string authentication = "";
	  std::string retMsg = getMsgIdentify(authentication,
					      rpcVersion,
					      true,
					      All);

	  sendSimpleRequest(c, hdl, retMsg);
	}
      }
      break;

    case Identified:
      {
      }
      break;

    case Event:
      {
      }
      break;

    case RequestResponse:
      {
      }
      break;

    case RequestBatchResponse:
      {
      }
      break;
    }
  }

  static void on_open(OBSConnection* obscon, websocketpp::connection_hdl hdl) {
    obscon->hdl = hdl;
  }

  void join() {
    pThr->join();
  }

private:
  std::string getURI() const {
    std::string ret { proto + ":" + "//" + ipohost + ":" + port };
    return ret;
  }

  std::string getDebugStr() const {
    std::ostringstream oss;
    oss << debug;
    return oss.str();
  }

  void run() {
    // int pobsout[2];
    // int pobserr[2];
    // ::pipe(pobsout);
    // ::pipe(pobserr);

    // int fdout = ::open("/dev/null", O_WRONLY);
    // int fderr = ::open("/dev/null", O_WRONLY);

    // std::string port_1 { PORT_1 };
    // std::string pass_1 { PASS_1 };
    // OBSConnection *pObsCon = new OBSConnection(port, pass);

    child = ::fork();

    if (child == 0) {
      // ::dup2(pobsout[STDOUT_FILENO], STDOUT_FILENO);
      // ::dup2(fdout, pobsout[STDIN_FILENO]);
      // ::dup2(pobserr[STDOUT_FILENO], STDERR_FILENO);
      // ::dup2(fderr, pobserr[STDIN_FILENO]);
      // ::close(pobsout[STDOUT_FILENO]);
      // ::close(fdout);
      // ::close(pobserr[STDOUT_FILENO]);
      // ::close(fderr);

      // TODO update parameters and creation of this
      ::execlp("obs", "obs",
	       "--scene", scene.c_str(),
	       "--profile", profile.c_str(),
	       "--websocket_port", port.c_str(),
	       "--websocket_password", pass.c_str(),
	       "--websocket_debug", getDebugStr().c_str(),
	       "--multi", nullptr);
      ::exit(100);
    }

    // ::close(fderr);
    // ::close(fdout);
    // ::close(pobsout[STDIN_FILENO]);
    // ::close(pobsout[STDOUT_FILENO]);
    // ::close(pobserr[STDIN_FILENO]);
    // ::close(pobserr[STDOUT_FILENO]);

    ::sleep(WAIT_TIME);

    // client c;

    // std::string port1 = PORT_1;
    std::string uri = getURI(); // "ws://localhost:" + port1; // "ws://localhost:4888";
    std::cout << "[obscontrol] " << uri << std::endl;
    std::thread* watchThread;

    try {
      // Set logging to be pretty verbose (everything except message payloads)
      c.set_access_channels(websocketpp::log::alevel::all);
      c.clear_access_channels(websocketpp::log::alevel::frame_payload);
      c.set_error_channels(websocketpp::log::elevel::all);

      // Initialize ASIO
      c.init_asio();

      // Register our message handler
      c.set_message_handler(bind(OBSConnection::on_message,this,&c,::_1,::_2));
      c.set_open_handler(bind(OBSConnection::on_open,this,::_1));

      websocketpp::lib::error_code ec;
      client::connection_ptr con = c.get_connection(uri, ec);

      if (ec) {
	std::cout << "[obscontrol] could not create connection because: " << ec.message() << std::endl;
	return;
      }

      // Note that connect here only requests a connection. No network messages are
      // exchanged until the event loop starts running in the next line.
      c.connect(con);

      // Start the ASIO io_service run loop
      // this will cause a single connection to be made to the server. c.run()
      // will exit when this connection is closed.
      std::cout << "[obscontrol] running..." << std::endl;
      // std::cout << "Starting thread..." << std::endl;

      // watchThread = new std::thread(watchInput, &c);
      c.run();
    } catch (websocketpp::exception const & e) {
      std::cout << "Error connecting" << std::endl;
      std::cout << "[obscontrol] " << e.what() << std::endl;
    }

  }
};

void watchInput(std::vector<OBSConnection*>& obsCons) {

  struct termios state1, state2;
  char c;
  int p;

  if (!::isatty(STDOUT_FILENO)) {
    fprintf(stderr, "Error output is not standard\n");
    exit(2);
  }

  ::ioctl(STDOUT_FILENO, TIOCGPGRP, &p);

  if (::getpgid(STDIN_FILENO) != p) {
    fprintf(stderr, "Process in background\n");
    exit(2);
  }

  if (!::isatty(STDIN_FILENO)) {
    fprintf(stderr, "Error input is not standard");
    exit(2);
  }

  if (::ioctl(STDIN_FILENO, TCGETS, &state1) == -1) {
    perror("iotcl\n");
    exit(4);
  }

  state2 = state1;
  state2.c_cc[VMIN] = 1;
  state2.c_lflag &= ~(ECHO | ISIG | ICANON);
  // state2.c_oflag |= OLCUC;

  ::ioctl(STDIN_FILENO, TCSETS, &state2);

  enum class OBSState { Stopped, Recording };
  OBSState curr = OBSState::Stopped;
  std::string requestType;
  std::string retMsg;

  do {
    ::read(STDIN_FILENO, &c, 1);

    if (::isblank(c)) {

      switch(curr) {
      case OBSState::Stopped:
	requestType = "StartRecord";
	retMsg = getMsgRequest(requestType);
	curr = OBSState::Recording;
	break;
      case OBSState::Recording:
	requestType = "StopRecord";
	retMsg = getMsgRequest(requestType);
	curr = OBSState::Stopped;
	break;
      }

      for (auto pObsCon : obsCons) {
	std::ostringstream oss;

	oss << "Request: "
	    << requestType
	    << " to "
	    << pObsCon->scene
	    << std::endl;

	sendSimpleRequest(&pObsCon->c,
			  pObsCon->hdl,
			  retMsg);
	std::string msgState = oss.str();
	::write(STDOUT_FILENO, msgState.c_str(), msgState.size());
      }
    }
  } while (c != state2.c_cc[VINTR] &&
	   c != state2.c_cc[VQUIT] &&
	   c != state2.c_cc[VEOF]);

  if (curr == OBSState::Recording) {
    requestType = "StopRecord";
    retMsg = getMsgRequest(requestType);
    curr = OBSState::Stopped;
   
    for (auto pObsCon : obsCons) {
	std::ostringstream oss;

	oss << "Request: "
	    << requestType
	    << " to "
	    << pObsCon->scene
	    << std::endl;

	sendSimpleRequest(&pObsCon->c,
			  pObsCon->hdl,
			  retMsg);
	std::string msgState = oss.str();
	::write(STDOUT_FILENO, msgState.c_str(), msgState.size());
    }
  }

  ioctl(STDIN_FILENO, TCSETS, &state1);
  putchar('\n');

  for (auto pObsCon : obsCons)
    kill(pObsCon->child, SIGKILL);
  return;
}

int
main(int argc, char *argv[]) {

  std::string port;
  std::string pass;
  std::string scene;
  std::string profile;
  port = PORT_1;
  pass = PASS_1;
  scene = "Docente";
  profile = "Docente";

  std::vector<OBSConnection*> obsCons;

  obsCons.push_back(new OBSConnection(port,
				      pass,
				      scene,
				      profile
				      ));
  
  ::sleep(WAIT_TIME);

  port = PORT_2;
  pass = PASS_2;
  scene = "Console";
  profile = "Console";

  obsCons.push_back(new OBSConnection(port,
				      pass,
				      scene,
				      profile
				      ));

  watchInput(obsCons);
  
  int status;

  for (auto pObsCon : obsCons) {
    pObsCon->join();
    ::waitpid(pObsCon->child, &status, 0);
    //watchThread->join();

    std::cout << "[obscontrol] Ending obs " << pObsCon->scene
	      << " with status: "
	      << status << std::endl;
  }

  return EXIT_SUCCESS;
}
