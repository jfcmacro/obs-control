#include <iostream>
#include <QString>
#include <QByteArray>
#include <QCryptographicHash>
#include <string>
#include <cstdlib>
#include <thread>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
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
#define WAIT_TIME 20

typedef websocketpp::client<websocketpp::config::asio_client> client;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

websocketpp::connection_hdl global_hdl;
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
  std::thread* thr;
  websocketpp::connection_hdl hdl;
  std::string port;
  std::string pass;
  std::string ipohost;
  std::string proto;
  bool debug;

  OBSConnection(std::string& port,
		std::string& pass,
		const std::string& ipohost = localhost,
		const std::string& proto = wsproto,
		bool debug = false) :
    c(),
    port(port),
    pass(pass),
    ipohost(ipohost),
    proto(proto),
    debug(debug) { } // TODO Check local o remote

  void on_message(client* c,
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
	  passwordAndSalt += QString::fromStdString(pass);
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

  void on_open(websocketpp::connection_hdl hdl) {
    this->hdl = hdl;
  }

  std::string getURI() const {
    std::string ret { proto + ":" + "//" + ipohost + ":" + port };
    return ret;
  }
};

void on_open(websocketpp::connection_hdl hdl) {
  global_hdl = hdl;
}



void on_message(client* c,
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
	std::string password(PASS_1);

	QString passwordAndSalt = "";
	passwordAndSalt += QString::fromStdString(password);
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

void watchInput(client *c) {
  bool toRecord = true;

  for (;;) {

    std::string str;
    std::cin >> str;

    if (!std::cin) break;

    if (toRecord) {

      std::cout << "Recording: " << std::endl;
      std::string requestType = "StartRecord";

      std::string retMsg = getMsgRequest(requestType);
      sendSimpleRequest(c, global_hdl, retMsg);
    }
    else {
      std::cout << "No recording" << std::endl;
      std::string requestType = "StopRecord";

      std::string retMsg = getMsgRequest(requestType);
      sendSimpleRequest(c, global_hdl, retMsg);
    }

    toRecord = !toRecord;
  }

  if (toRecord) {
    std::cout << "No recording" << std::endl;
    std::string requestType = "StopRecord";

    std::string retMsg = getMsgRequest(requestType);
    sendSimpleRequest(c, global_hdl, retMsg);
  }

  return;
}

int
main(int argc, char *argv[]) {

  // int pobsout[2];
  // int pobserr[2];
  // ::pipe(pobsout);
  // ::pipe(pobserr);

  // int fdout = ::open("/dev/null", O_WRONLY);
  // int fderr = ::open("/dev/null", O_WRONLY);

  std::string port_1 { PORT_1 };
  std::string pass_1 { PASS_1 };
  OBSConnection *pObsCon = new OBSConnection(port_1, pass_1);

  pObsCon->child = ::fork();

  if (pObsCon->child == 0) {
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
	     "--scene", "Docente",
	     "--profile", "Docente",
	     "--websocket_port", pObsCon->port.c_str(),
	     "--websocket_password", pObsCon->pass.c_str(),
	     // "--websocket_debug", "false",
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
  std::string uri = pObsCon->getURI(); // "ws://localhost:" + port1; // "ws://localhost:4888";
  std::cout << "[obscontrol] " << uri << std::endl;
  std::thread* watchThread;

  try {
    // Set logging to be pretty verbose (everything except message payloads)
    pObsCon->c.set_access_channels(websocketpp::log::alevel::all);
    pObsCon->c.clear_access_channels(websocketpp::log::alevel::frame_payload);
    pObsCon->c.set_error_channels(websocketpp::log::elevel::all);

    // Initialize ASIO
    pObsCon->c.init_asio();

    // Register our message handler
    pObsCon->c.set_message_handler(bind(&on_message,&pObsCon->c,::_1,::_2));
    pObsCon->c.set_open_handler(bind(&on_open,::_1));

    websocketpp::lib::error_code ec;
    client::connection_ptr con = pObsCon->c.get_connection(uri, ec);

    if (ec) {
      std::cout << "[obscontrol] could not create connection because: " << ec.message() << std::endl;
      return 0;
    }

    // Note that connect here only requests a connection. No network messages are
    // exchanged until the event loop starts running in the next line.
    pObsCon->c.connect(con);

    // Start the ASIO io_service run loop
    // this will cause a single connection to be made to the server. c.run()
    // will exit when this connection is closed.
    std::cout << "[obscontrol] running..." << std::endl;
    // std::cout << "Starting thread..." << std::endl;

    // watchThread = new std::thread(watchInput, &c);
    pObsCon->c.run();
  } catch (websocketpp::exception const & e) {
    std::cout << "[obscontrol] " << e.what() << std::endl;
  }

  int status;

  ::waitpid(pObsCon->child, &status, 0);
  //watchThread->join();

  std::cout << "[obscontrol] Ending with status: "
	    << status << std::endl;

  return EXIT_SUCCESS;
}
