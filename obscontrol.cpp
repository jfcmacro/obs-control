#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
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
// #include <netinet/in.h>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>
#include <json/json.h>
#include <getopt.h>
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
int requestId = 0;
const std::string localhost { "localhost" }; // TODO Change this to local
const std::string default_profile { "default" };
const std::string default_scene { "default" };
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

  jsonAns["op"] = Identify;
  jsonAns["d"] = jsonAnsData;

  Json::StreamWriterBuilder writerBuilder;
  return Json::writeString(writerBuilder, jsonAns);
}

std::string
getMsgRequest(std::string& requestType) {
  Json::Value jsonAns;
  Json::Value jsonAnsData;

  jsonAnsData["requestType"] = requestType;
  jsonAnsData["requestId"] = requestId;

  jsonAns["op"] = Request;
  jsonAns["d"] = jsonAnsData;

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

enum class OBSState { Stopped,
		      Recording
                    };

struct OBSConnection {
  client c;
  pid_t child;
  std::thread *pThr;
  websocketpp::connection_hdl hdl;
  std::string port;
  std::string pass;
  std::string ipohost;
  std::string proto;
  std::string profile;
  std::string scene;
  bool runProcess;
  bool debug;
  OBSState curr;

  OBSConnection(std::string& port,
		std::string& pass,
		const std::string& profile = default_profile,
		const std::string& scene = default_scene,
		const std::string& ipohost = localhost,
		bool runProcess = true,
		const std::string& proto = wsproto,
		bool debug = false) :
    c(),
    port(port),
    pass(pass),
    profile(profile),
    scene(scene),
    ipohost(ipohost),
    runProcess(runProcess),
    proto(proto),
    debug(debug),
    curr(OBSState::Stopped) {
    // Only creates a new process is this is local and user wants
    if (not (this->runProcess and ipohost == localhost)) runProcess = false;

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

    if (jsonMsg.isMember("error")) {
      std::cout << "Error: " << jsonMsg["error"].asString() << std::endl;
      std::cout << "Status: " << jsonMsg["status"].asString() << std::endl;
      ::exit(EXIT_FAILURE);
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
	Json::Value jsonStatus = jsonMsg["d"]["requestStatus"];
	if (jsonStatus["result"].asBool()) {
	  if (jsonMsg["d"]["requestType"].asString() == "StartRecord")
	    obscon->curr = OBSState::Recording;
	  if (jsonMsg["d"]["requestType"].asString() == "StopRecord")
	    obscon->curr = OBSState::Stopped;
	}
	else {
	  std::cout << "Error code: "
		    << jsonStatus["code"].asInt()
		    << std::endl;
	  if (jsonStatus.isMember("comment")) {
	    std::cout << "Error message: "
		      << jsonStatus["comment"]
		      << std::endl;
	  }
	}
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

    if (runProcess) {

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
		 "--profile", profile.c_str(),
		 "--scene", scene.c_str(),
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
    }
    else {
      child = -1;
    }

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
      std::cout << "Initialize ASIO" << std::endl;
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
      std::cout << "c.connect is next" << std::endl;
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

  if (not ::isatty(STDOUT_FILENO)) {
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

  std::string requestType;
  std::string retMsg;

  do {
    ::read(STDIN_FILENO, &c, 1);

    switch(c) {
    case 'r':
      requestType = "StartRecord";
      break;
    case 's':
      requestType = "StopRecord";
      break;
    }

    retMsg = getMsgRequest(requestType);

    for (auto pObsCon : obsCons) {
      std::ostringstream oss;

      oss << "Sending Request: "
		<< requestType
	  << std::endl
	  << " Encoding as: "
	  << retMsg
	  << std::endl;
      sendSimpleRequest(&pObsCon->c,
			  pObsCon->hdl,
			retMsg);


      std::string msgState = oss.str();
      ::write(STDOUT_FILENO, msgState.c_str(), msgState.size());
    }

  } while (c != state2.c_cc[VINTR] &&
	   c != state2.c_cc[VQUIT] &&
	   c != state2.c_cc[VEOF]);

  for (auto pObsCon : obsCons) {
    std::ostringstream oss;

    if (pObsCon->curr == OBSState::Recording) {
      requestType = "StopRecord";
      retMsg = getMsgRequest(requestType);

      sendSimpleRequest(&pObsCon->c,
			pObsCon->hdl,
			retMsg);
      oss << "Stopping " << std::endl;
      std::string msgState = oss.str();

      ::write(STDOUT_FILENO, msgState.c_str(), msgState.size());
    }
  }

  ioctl(STDIN_FILENO, TCSETS, &state1);
  putchar('\n');

  for (auto pObsCon : obsCons) {
    if (pObsCon->runProcess and pObsCon->child != -1)
      kill(pObsCon->child, SIGKILL);
  }
  return;
}

static void
processOptions(int argc, char *argv[],
	       std::string& cfilename) {
  bool version = false;

  static struct option long_options[] = {
    {"config", 1, 0, 'c'},
    {"help", 0, 0, 'h'},
    {"version", 0, 0, 'v'},
    {0, 0, 0, 0}
  };

  int c;
  int option_index;

  while ((c = getopt_long(argc, argv, "c:hv",
			  long_options, &option_index)) != -1) {
    switch (c) {
    case 'c':
      cfilename.clear();
      cfilename += optarg;
      break;

    case 'v':
      std::cout << "obscontrol version: "
		<< OBSCONTROL_VERSION
		<< std::endl;
      exit(EXIT_SUCCESS);
      break;

    case 'h':
      std::cout << "obscontrol [(-c|--config) <config-path-name>]"
		<<  " # control several obs instances" << std::endl;
      std::cout << "obscontrol (-h|--help) # Show this message"
		<< std::endl;
      std::cout << "obscontrol (-v|--version) # Show the current version"
		<< std::endl;
      exit(EXIT_SUCCESS);
      break;
    }
  }
}

bool
readingConfigFile(const std::string &configName, Json::Value &config) {

  std::ifstream cf(configName);

  if (!cf) return false;

  cf >> config;
  return true;
}

void
testDefaultConfigFile(std::string& defaultConfigFile) {

  std::ifstream cf(defaultConfigFile);

  if (!cf) {
    cf.close();
    std::filesystem::path configPath { ::getenv("HOME") };

    configPath /= ".config";
    configPath /= "obscontrol";

    if (!std::filesystem::exists(configPath)) create_directory(configPath);

    std::ofstream ocf(defaultConfigFile);

    Json::Value jarray(Json::arrayValue);
    Json::Value root(Json::objectValue);
    Json::Value values(Json::objectValue);

    values["enable"] = true;
    values["address"] = "127.0.0.1";
    values["port"] = "4444";
    values["password"] = "abcd1234";
    values["init_profile"] = "default_profile";
    values["init_scene"] = "default_scene";

    root["default"] = values;
    jarray[0] = root;

    ocf << jarray << std::endl;
  }
  else {
    cf.close();
  }
}

int
main(int argc, char *argv[]) {

  std::string defaultConfigFile = { ::getenv("HOME") };
  defaultConfigFile += "/.config/obscontrol/config.json";

  testDefaultConfigFile(defaultConfigFile);

  // ::exit(EXIT_SUCCESS);

  std::string configFile { defaultConfigFile };

  processOptions(argc, argv, configFile);
  Json::Value config;

  if (readingConfigFile(configFile, config)) {

    std::vector<OBSConnection*> obsCons;

    for (Json::Value v : config) {
      std::string remoteOBSName { v.getMemberNames()[0] };

      if (v[remoteOBSName]["enable"].asBool()) {
	std::string port    { v[remoteOBSName]["port"].asString() };
	std::string pass    { v[remoteOBSName]["password"].asString() } ;
	std::string profile { v[remoteOBSName]["init_scene"].asString() };
	std::string scene   { v[remoteOBSName]["init_scene"].asString() };
	std::string host    { v[remoteOBSName]["address"].asString() };

	if (!v[remoteOBSName]["local"].isNull()) {

	  if (v[remoteOBSName]["local"]["launch"].asBool()) {
	    obsCons.push_back(new OBSConnection(port,
						pass,
						profile,
						scene
						));
	  }
	  else {
	    obsCons.push_back(new OBSConnection(port,
						pass,
						profile,
						scene,
						host,
						false
						));
	  }

	  ::sleep(WAIT_TIME);
	}
	else {

	  obsCons.push_back(new OBSConnection(port,
					      pass,
					      profile,
					      scene,
					      host,
					      false
					      ));
	}
      }
    }

    watchInput(obsCons);

    int status;

    for (auto pObsCon : obsCons) {
      pObsCon->join();
      if (pObsCon->runProcess and pObsCon->child == -1) {
	::waitpid(pObsCon->child, &status, 0);
	//watchThread->join();

	std::cout << "[obscontrol] Ending obs "
		  << pObsCon->scene
		  << " with status: "
		  << status << std::endl;
      }
      else {
	std::cout << "[obscontrol] Ending obs "
		  << pObsCon->scene
		  << std::endl;
      }
    }
  }
  else {
    std::cerr << "Cannot open configuration file"
	      << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
