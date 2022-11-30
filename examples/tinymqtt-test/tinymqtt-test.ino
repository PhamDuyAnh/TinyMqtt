// vim: ts=2 sw=2 expandtab smartindent
#define TINY_MQTT_DEBUG
#include <TinyConsole.h>
#include <TinyMqtt.h>       // https://github.com/hsaturn/TinyMqtt
#include <MqttStreaming.h>
#if defined(ESP8266)
  #include <ESP8266mDNS.h>
#elif defined(ESP32)
  #include <WiFi.h>
  #include <ESPmDNS.h>
#else
  #error Unsupported platform
#endif

#include <sstream>
#include <map>

bool echo_on = true;
auto green = TinyConsole::green;
auto red = TinyConsole::red;
auto white = TinyConsole::white;
auto cyan = TinyConsole::cyan;

/** Very complex example
  * Console allowing to make any kind of test,
	* even some stress tests.
  *
	* Upload the sketch, the use the terminal.
	* Press H for mini help.
	*
  * tested with mqtt-spy-0.5.4
	* TODO examples of scripts
  */

const char* ssid = "Freebox-786A2F";
const char* password = "usurpavi8dalum64lumine?";

std::string topic="sensor/temperature";

void onPublish(const MqttClient* srce, const Topic& topic, const char* payload, size_t length)
{
	Console << cyan << "--> " << srce->id().c_str() << ": received " << topic.c_str() << white;
	if (payload)
	{
		Console << ", payload[" << length << "]=[";
		while(length--)
		{
			const char c=*payload++;
			if (c<32)
				Console << '?';
			else
				Console << c;
		}
		Console << ']' << endl;
	}
}

std::map<std::string, MqttClient*> clients;
std::map<std::string, MqttBroker*> brokers;

void setup()
{
	WiFi.persistent(false); // https://github.com/esp8266/Arduino/issues/1054
  Serial.begin(115200);
	Console.begin(Serial);
	Console.setPrompt("> ");
	Console.setCallback(onCommand);
	delay(500);

	Console.cls();
  Console << endl << endl;
  Console << yellow
			    << "***************************************************************" << endl;
  Console << "*  Welcome to the TinyMqtt console" << endl;
  Console << "*  The console allows to test all features of the libraries." << endl;
	Console << "*  Enter help to view the list of commands." << endl;
  Console << "***************************************************************" << endl;
  Console << endl;
  if (strlen(ssid)==0)
    Console << red << "*  ERROR: You must modify ssid/password in order" << endl
		       << "            to be able to connect to your Wifi network." << endl;
  Console << endl << white;

	Console << "Connecting to '" << ssid << "' ";

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
  { Console << '-'; delay(500); }

  Console << endl << "Connected to " << ssid << "IP address: " << WiFi.localIP() << endl;

  const char* name="tinytest";
  Console << "Starting MDNS, name= " << name;
  if (!MDNS.begin(name))
    Console << "  error, not available." << endl;
  else
    Console << " ok." << endl;


	MqttBroker* broker = new MqttBroker(1883);
	broker->begin();
	brokers["broker"] = broker;
}

std::string getword(std::string& str, const char* if_empty=nullptr, char sep=' ');

int getint(std::string& str, const int if_empty=0)
{
	std::string str2=str;
	std::string sword = getword(str);
	if (sword[0] and isdigit(sword[0]))
	{
		int ret=atoi(sword.c_str());
		while(isdigit(sword[0]) or sword[0]==' ') sword.erase(0,1);
		if (sword.length()) str = sword+' '+str;
	  return ret;
	}
	str=str2;
  return if_empty;
}

std::string getword(std::string& str, const char* if_empty/*=nullptr*/, char sep/*=' '*/)
{
	char quote=(str[0]=='"' or str[0]=='\'' ? str[0] : 0);
	if (quote) str.erase(0,1);
	std::string sword;
	while(str.length() and (str[0]!=sep or quote))
	{
		if (str[0]==quote)
		{
			str.erase(0,1);
			break;
		}
		sword += str[0];
		str.erase(0,1);
	}
	while(str[0]==sep) str.erase(0,1);
	if (if_empty and sword.length()==0) return if_empty;
	if (quote==false and sword.length()>=4 and sword.substr(0,3)=="rnd")
	{
		sword.erase(0,3);
		if (sword[0]=='(')
		{
			int to = 100;
			sword.erase(0,1);
			int from=getint(sword);
			if (sword[0]==',')
			{
				sword.erase(0,1);
				to = getint(sword);
				if (sword[0]!=')') Console << "Missing ')'" << endl;
			}
			else
			{
				to=from;
				from=0;
			}
			return String(random(from,to)).c_str();
		}
		else
		{
			Console << "Missing '('" << endl;
		}
	}
	while(str[0]==' ') str.erase(0,1);
	return sword;
}

bool isaddr(std::string s)
{
	if (s.length()==0 or s.length()>3) return false;
	for(char c: s)
		if (c<'0' or c>'9') return false;
	return true;
}

std::string getip(std::string& str, const char* if_empty=nullptr, char sep=' ')
{
	std::string addr=getword(str, if_empty, sep);
	std::string ip=addr;
	std::vector<std::string> build;
	while(ip.length())
	{
		std::string b=getword(ip,nullptr,'.');
		if (isaddr(b) && build.size()<4)
		{
			build.push_back(b);
		}
		else
			return addr;
	}
	IPAddress local=WiFi.localIP();
	addr.clear();
	while(build.size()!=4)
	{
		std::stringstream b;
		b << (int)local[3-build.size()];
		build.insert(build.begin(), b.str());
	}
	for(std::string s: build)
	{
		if (addr.length()) addr += '.';
		addr += s;
	}
	return addr;
}

std::map<std::string, std::string> vars;

std::set<std::string> commands = {
	"auto", "broker", "blink", "client", "connect",
	"create", "delete", "help", "interval",
	"ls", "ip", "off", "on", "set",
	"publish", "reset", "subscribe", "unsubscribe", "view", "echo", "every"
};

void convertToCommand(std::string& search)
{
	while(search[0]==' ') search.erase(0,1);
	if (search.length()==0) return;
	std::string matches;
	int count=0;
	for(std::string cmd: commands)
	{
		if (cmd.substr(0, search.length()) == search)
		{
			if (count) matches +=", ";
			count++;
			matches += cmd;
		}
	}
	if (count==1)
		search = matches;
	else if (count>1)
	{
		Console << "Ambiguous command: " << matches << endl;
		search.clear();
	}
}

void replace(const char* d, std::string& str, std::string srch, std::string to)
{
	if (d[0] && d[1])
	{
		srch=d[0]+srch+d[1];
		to=d[0]+to+d[1];

		size_t pos = 0;
		while((pos=str.find(srch, pos)) != std::string::npos)
		{
			str.erase(pos, srch.length());
			str.insert(pos, to);
			pos += to.length()-1;
		}
	}
}

void replaceVars(std::string& cmd)
{
	cmd = ' '+cmd+' ';

	for(auto it: vars)
	{
		replace("..", cmd, it.first, it.second);
		replace(". ", cmd, it.first, it.second);
		replace(" .", cmd, it.first, it.second);
		replace("  ", cmd, it.first, it.second);
	}
	cmd.erase(0, cmd.find_first_not_of(" "));
	cmd.erase(cmd.find_last_not_of(" ")+1);

}

// publish at regular interval
class Automatic
{
	public:
		Automatic(MqttClient* clt, uint32_t intervl)
		: client(clt), topic_(::topic)
		{
			interval(intervl);
		  autos[clt] = this;
		}

		void interval(uint32_t new_interval)
		{
			interval_ = new_interval;
			if (interval_<1000) interval_=1000;
			timer_ = millis() + interval_;
		}

		void loop_()
		{
			if (!bon) return;
			if (interval_ && millis() > timer_)
			{
				Console << "AUTO PUBLISH " << interval_ << endl;
				timer_ += interval_;
				client->publish(topic_, std::string(String(15+millis()%10).c_str()));
			}
		}

		void topic(std::string new_topic) { topic_ = new_topic; }

		static void loop()
		{
			for(auto it: autos)
				it.second->loop_();
		}

		static void command(MqttClient* who, std::string cmd)
		{
			Automatic* autop = nullptr;
			if (autos.find(who) != autos.end())
			{
				autop=autos[who];
			}
			std::string s = getword(cmd);
			if (compare(s, "create"))
			{
				std::string seconds=getword(cmd, "10000");
				if (autop) delete autop;
				std::string top = getword(cmd, ::topic.c_str());
				autos[who] = new Automatic(who, atol(seconds.c_str()));
				autos[who]->topic(top);
				autos[who]->bon=true;
				Console << "New auto (" << seconds.c_str() << " topic:" << top.c_str() << ')' << endl;
			}
			else if (autop)
			{
				while(s.length())
				{
					if (s=="on")
					{
						autop->bon = true;
						autop->interval(autop->interval_);
					}
					else if (s=="off")
						autop->bon=false;
					else if (s=="interval")
					{
						int32_t i=getint(cmd);
						if (i)
							autop->interval(atol(s.c_str()));
						else
							Console << "Bad value" << endl;
					}
				  else if (s=="view")
					{
						Console << "  Automatic "
							<< (int32_t)autop->client
							<< " interval " << autop->interval_
							<< (autop->bon ? " on" : " off") << endl;
					}
					else
					{
						Console << "Unknown auto command (" << s.c_str() << ")" << endl;
						break;
					}
					s=getword(cmd);
				}
			}
			else if (who==nullptr)
			{
				for(auto it: autos)
					command(it.first, s+' '+cmd);
			}
			else
				Console << "what ? (" << s.c_str() << ")" << endl;
		}

		static void help()
		{
					Console << "    auto [$id] on/off" << endl;
					Console << "    auto [$id] view" << endl;
					Console << "    auto [$id] interval [s]" << endl;
					Console << "    auto [$id] create [millis] [topic]" << endl;
		}

	private:
		MqttClient* client;
		uint32_t interval_;
		uint32_t timer_;
		std::string topic_;
		bool bon=false;
		static std::map<MqttClient*, Automatic*> autos;
		float temp=19;
};
std::map<MqttClient*, Automatic*> Automatic::autos;

bool compare(std::string s, const char* cmd)
{
	uint8_t p=0;
	while(s[p++]==*cmd++)
	{
		if (*cmd==0 or s[p]==0) return true;
		if (s[p]==' ') return true;
	}
	return false;
}

using ClientFunction = void(*)(std::string& cmd, MqttClient* publish);

struct Every
{
	std::string cmd;
	uint32_t ms;
	uint32_t next;
	uint32_t underrun=0;
	bool active=true;

	void dump()
	{
		if (active)
			Console << green << "enabled";
		else
			Console << red << "disabled";

    Console << white << 
		auto mill=millis();
		Console << ms << "ms [" << cmd << "] next in ";
		if (mill > next)
			Console << "now";
		else
			Console << next-mill << "ms";
	}
};

uint32_t blink_ms_on[16];
uint32_t blink_ms_off[16];
uint32_t blink_next[16];
bool blink_state[16];
int16_t blink;

std::vector<Every> everies;

void onCommand(const std::string& command)
{
	std::string cmd=command;
	if (cmd.substr(0,3)!="set") replaceVars(cmd);
	eval(cmd);
}

void eval(std::string& cmd)
{
	while(cmd.length())
	{
		MqttError retval = MqttOk;

		std::string s;
		MqttBroker* broker = nullptr;
		MqttClient* client = nullptr;

		// client.function notation
		if (cmd.find('.') != std::string::npos &&
				cmd.find('.') < cmd.find(' '))
		{
			s=getword(cmd, nullptr, '.');

			if (s.length())
			{
				if (clients.find(s) != clients.end())
				{
					client = clients[s];
				}
				else if (brokers.find(s) != brokers.end())
				{
					broker = brokers[s];
				}
				else
				{
					Console << red << "Unknown class (" << s.c_str() << ")" << white << endl;
					cmd.clear();
				}
			}
		}

		s = getword(cmd);
		if (s.length()) convertToCommand(s);
		if (s.length()==0)
		{}
		else if (compare(s, "delete"))
		{
			if (client==nullptr && broker==nullptr)
			{
				s = getword(cmd);
				if (clients.find(s) != clients.end())
				{
					client = clients[s];
				}
				else if (brokers.find(s) != brokers.end())
				{
					broker = brokers[s];
				}
				else
					Console << red << "Unable to find (" << s.c_str() << ")" << << white << endl;
			}
			if (client)
			{
				for (auto it: clients)
				{
					if (it.second != client) continue;
					Console << "deleted" << endl;
					delete (it.second);
					clients.erase(it.first);
					break;
				}
				cmd += " ls";
			}
			else if (broker)
			{
				for(auto it: brokers)
				{
					if (broker != it.second) continue;
					Console << "deleted" << endl;
					delete (it.second);
					brokers.erase(it.first);
					break;
				}
				cmd += " ls";
			}
			else
				Console << "Nothing to delete" << endl;
		}
		else if (broker)
		{
			if (compare(s,"connect"))
			{
				Console << "NYI" << endl;
			}
			else if (compare(s, "view"))
			{
				broker->dump();
			}
			else
			{
				Console << "Unknown broker command (" << s << ")" << endl;
				s.clear();
			}
		}
		else if (client)
		{
			if (compare(s,"connect"))
			{
				client->connect(getip(cmd,"192.168.1.40").c_str(), getint(cmd, 1883), getint(cmd, 60));
				Console << (client->connected() ? "connected." : "not connected") << endl;
			}
			else if (compare(s,"publish"))
			{
				retval = client->publish(getword(cmd, topic.c_str()), getword(cmd));
			}
			else if (compare(s,"subscribe"))
			{
				client->subscribe(getword(cmd, topic.c_str()));
			}
			else if (compare(s, "unsubscribe"))
			{
				client->unsubscribe(getword(cmd, topic.c_str()));
			}
			else if (compare(s, "view"))
			{
				client->dump();
			}
			else
			{
				Console << "Unknown client command (" << s << ")" << endl;
				s.clear();
			}
		}
		else if (compare(s, "on"))
		{
			uint8_t pin=getint(cmd, 2);
			pinMode(pin, OUTPUT);
			digitalWrite(pin, HIGH);
		}
		else if (compare(s, "off"))
		{
			uint8_t pin=getint(cmd, 2);
			pinMode(pin, OUTPUT);
			digitalWrite(pin, LOW);
		}
		else if (compare(s, "echo"))
		{
			s=getword(cmd);
			if (s=="on")
				echo_on = true;
			else if (s=="off")
				echo_on = false;
			else
			{
				Console << s << ' ';
				while(cmd.length())
				{
					Console << getword(cmd) << ' ';
				}
			}
		}
		else if (compare(s, "every"))
		{
			uint32_t ms = getint(cmd, 0);
			if (ms)
			{
				if (cmd.length())
				{
					Every every;
					every.ms=ms;
					every.cmd=cmd;
					every.next=millis()+ms;
					everies.push_back(every);
					every.dump();
					Console << endl;
					cmd.clear();
				}
			}
			else if (compare(cmd, "off") or compare(cmd, "on"))
			{
				bool active=getword(cmd)=="on";
				uint8_t ever=getint(cmd, 100);
				uint8_t count=0;
				for(auto& every: everies)
				{
					if (count==ever or (ever==100))
					{
						if (every.active != active)
						{
							Console << "every #" << count << (active ? " on" :" off") << endl;
							every.active = active;
							every.underrun = 0;
						}
					}
					count++;
				}
			}
			else if (compare(cmd, "list") or cmd.length()==0)
			{
				getword(cmd);
				Console << "List of everies (ms=" << millis() << ")" << endl;
				uint8_t count=0;
				for(auto& every: everies)
				{
					Console << count << ": ";
					every.dump();
					Console << endl;
					count++;
				}
			}
			else if (compare(cmd, "remove"))
			{
				Console << "Removing..." << endl;
				getword(cmd);
				int8_t every=getint(cmd, -1);
				if (every==-1 and compare(cmd, "last") and everies.size())
				{
					getword(cmd);
					everies.erase(everies.begin()+everies.size()-1);
				}
				else if (every==-1 and compare(cmd, "all"))
				{
					getword(cmd);
					everies.clear();
				}
				else if (everies.size() > (uint8_t)every)
				{
					everies.erase(everies.begin()+every);
				}
				else
					Console << "Bad colmmand" << endl;
			}
			else
				Console << "Bad command" << endl;
		}
		else if (compare(s, "blink"))
		{
			int8_t blink_nr = getint(cmd, -1);
			if (blink_nr >= 0)
			{
				blink_ms_on[blink_nr]=getint(cmd, blink_ms_on[blink_nr]);
				blink_ms_off[blink_nr]=getint(cmd, blink_ms_on[blink_nr]);
				pinMode(blink_nr, OUTPUT);
				blink_next[blink_nr] = millis();
				Console << "Blink " << blink_nr << ' ' << (blink_ms_on[blink_nr] ? "on" : "off") << endl;
				if (blink_ms_on[blink_nr])
					blink |= 1<< blink_nr;
				else
				{
					blink &= ~(1<< blink_nr);
				}
			}
		}
		else if (compare(s, "auto"))
		{
			Automatic::command(client, cmd);
			if (client == nullptr)
				cmd.clear();
		}
		else if (compare(s, "broker"))
		{
			std::string id=getword(cmd);
      if (clients.find(id) != clients.end())
      {
        Console << "A client already have that name" << endl;
        cmd.clear();
      }
			else if (id.length() or brokers.find(id)!=brokers.end())
			{
				int port=getint(cmd, 0);
				if (port)
				{
					MqttBroker* broker = new MqttBroker(port);
					broker->begin();

					brokers[id] = broker;
					Console << "new broker (" << id.c_str() << ")" << endl;
				}
				else
        {
					Console << "Missing port" << endl;
          cmd.clear();
        }
			}
			else
      {
				Console << "Missing or existing broker name (" << id.c_str() << ")" << endl;
        cmd.clear();
      }
		}
		else if (compare(s, "client"))
		{
			std::string id=getword(cmd);
      if (brokers.find(id) != brokers.end())
      {
				Console << "A broker have that name" << endl;
				cmd.clear();
      }
			else if (id.length() or clients.find(id)!=clients.end())
			{
				s=getword(cmd);	// broker name
				if (s=="" or brokers.find(s) != brokers.end())
				{
					MqttBroker* broker = nullptr;
					if (s.length()) broker = brokers[s];
					MqttClient* client = new MqttClient(broker);
					client->id(id);
					clients[id]=client;
					client->setCallback(onPublish);
					client->subscribe(topic);
					Console << "new client (" << id.c_str() << ", " << s.c_str() << ')' << endl;
				}
				else if (s.length())
				{
					Console << " not found." << endl;
				  cmd.clear();
				}
			}
			else
      {
				Console << "Missing or existing client name" << endl;
        cmd.clear();
 			}
		}
		else if (compare(s, "set"))
		{
			std::string name(getword(cmd));
			if (name.length()==0)
			{
				for(auto it: vars)
				{
					Console << "  " << it.first << " -> " << it.second << endl;
				}
			}
			else if (commands.find(name) != commands.end())
			{
				Console << "Reserved keyword (" << name << ")" << endl;
				cmd.clear();
			}
			else
			{
				if (cmd.length())
				{
					vars[name] = cmd;
					cmd.clear();
				}
				else if (vars.find(name) != vars.end())
					vars.erase(vars.find(name));
			}
		}
		else if (compare(s, "ls") or compare(s, "view"))
		{
			Console << "--< " << clients.size() << " client/s. >--" << endl;
			for(auto it: clients)
			{
				it.second->dump("  ");
			}

			Console << "--< " << brokers.size() << " brokers/s. >--" << endl;
			for(auto it: brokers)
			{
				Console << "  +-- '" << it.first.c_str() << "' " << it.second->clientsCount() << " client/s."<< endl;
				it.second->dump("     ");
			}
		}
		else if (compare(s, "reset"))
			ESP.restart();
		else if (compare(s, "ip"))
			Console << "IP: " << WiFi.localIP() << endl;
		else if (compare(s,"help"))
		{
			Console << "syntax:" << endl;
			Console << "  MqttBroker:" << endl;
			Console << "    broker {broker_name} {port} : create a new broker" << endl;
      Console << "      broker_name.delete : delete a broker (buggy)" << endl;
      Console << "      broker_name.view   : dump a broker" << endl;
			Console << endl;
			Console << "  MqttClient:" << endl;
			Console << "    client {name} {parent broker} : create a client then" << endl;
			Console << "      name.connect  [ip] [port] [alive]" << endl;
			Console << "      name.[un]subscribe [topic]" << endl;
			Console << "      name.publish [topic][payload]" << endl;
			Console << "      name.view" << endl;
			Console << "      name.delete" << endl;
			Console << endl;

			Automatic::help();
			Console << endl;
			Console << "  help" << endl;
			Console << "  blink [Dx on_ms off_ms]    : make pin blink" << endl;
			Console << "  ls / ip / reset" << endl;
			Console << "  set [name][value]" << endl;
			Console << "  !  repeat last command" << endl;
			Console << endl;
			Console << "  echo [on|off] or strings" << endl;
			Console << "  every ms [command]; every list; every remove [nr|all]; every (on|off) [#]" << endl;
			Console << "  on {output}; off {output}" << endl;
			Console << "  $id : name of the client." << endl;
			Console << "  rnd[(min[,max])] random number." << endl;
			Console << "  default topic is '" << topic.c_str() << "'" << endl;
			Console << endl;
		}
		else
		{
			while(s[0]==' ') s.erase(0,1);
			if (s.length())
				Console << "Unknown command (" << s.c_str() << ")" << endl;
		}

		if (retval != MqttOk)
		{
			Console << "# MQTT ERROR " << retval << endl;
		}
	}
}

void loop()
{
	auto ms=millis();
	int8_t out=0;
	int16_t blink_bits = blink;
	uint8_t e=0;

	for(auto& every: everies)
	{
		if (not every.active) continue;
		if (every.ms && every.cmd.length() && ms > every.next)
		{
			std::string cmd(every.cmd);
			eval(cmd);
			every.next += every.ms;
			if (ms > every.next and ms > every.underrun)
			{
				Console << "Underrun every #" << e << ", " <<  (ms - every.next) << "ms late" << endl;
				every.underrun = ms+5000;
			}
		}
		e++;
	}

	while(blink_bits)
	{
		if (blink_ms_on[out] and ms > blink_next[out])
		{
			if (blink_state[out])
			{
				blink_next[out] += blink_ms_on[out];
				digitalWrite(out, LOW);
			}
			else
			{
				blink_next[out] += blink_ms_off[out];
				digitalWrite(abs(out), HIGH);
			}
			blink_state[out] = not blink_state[out];
		}
		blink_bits >>=1;
		out++;
	}

	static long count;
  #if defined(ESP9266)
  MDNS.update();
  #endif

	for(auto it: brokers)
		it.second->loop();

	for(auto it: clients)
		it.second->loop();

	Automatic::loop();
	Console::loop();
}
