#include <iostream>
#include <cstdlib>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include "mqtt/async_client.h"

using namespace std;

const string DFLT_SERVER_ADDRESS{ "broker.hivemq.com:1883" };
const string CLIENT_ID{ "paho_cpp_async_publish" };
const string PERSIST_DIR{ "./persist" };

const string TOPIC{ "Transmit" };

const char* PAYLOAD1 = "Hello World!";
const char* PAYLOAD2 = "Hi there!";
const char* PAYLOAD3 = "Is anyone listening?";
const char* PAYLOAD4 = "Someone is always listening.";

const char* LWT_PAYLOAD = "Last will and testament.";

const int  QOS = 1;

const auto TIMEOUT = std::chrono::seconds(0);

/////////////////////////////////////////////////////////////////////////////

/**
 * A callback class for use with the main MQTT client.
 */
class callback : public virtual mqtt::callback
{
public:
	void connection_lost(const string& cause) override {
		cout << "\nConnection lost" << endl;
		if (!cause.empty())
			cout << "\tcause: " << cause << endl;
	}

	void delivery_complete(mqtt::delivery_token_ptr tok) override {
		cout << "\tDelivery complete for token: "
			<< (tok ? tok->get_message_id() : -1) << endl;
	}
};

/////////////////////////////////////////////////////////////////////////////

/**
 * A base action listener.
 */
class action_listener : public virtual mqtt::iaction_listener
{
protected:
	void on_failure(const mqtt::token& tok) override {
		cout << "\tListener failure for token: "
			<< tok.get_message_id() << endl;
	}

	void on_success(const mqtt::token& tok) override {
		cout << "\tListener success for token: "
			<< tok.get_message_id() << endl;
	}
};

/////////////////////////////////////////////////////////////////////////////

/**
 * A derived action listener for publish events.
 */
class delivery_action_listener : public action_listener
{
	atomic<bool> done_;

	void on_failure(const mqtt::token& tok) override {
		action_listener::on_failure(tok);
		done_ = true;
	}

	void on_success(const mqtt::token& tok) override {
		action_listener::on_success(tok);
		done_ = true;
	}

public:
	delivery_action_listener() : done_(false) {}
	bool is_done() const { return done_; }
};

/////////////////////////////////////////////////////////////////////////////

std::chrono::time_point<std::chrono::system_clock> TimerStart;
std::chrono::time_point<std::chrono::system_clock> TimerEnd;
std::chrono::duration<double> TimeElapsed;

double TimeBetweenMessages = 5;
double SecondsElapsed;

double GetSecondsElapsed()
{
	SecondsElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(TimerEnd - TimerStart).count() / 1000.0;
	return SecondsElapsed;
}

bool TimerFunc()
{
	TimerEnd = std::chrono::system_clock::now();

	if (GetSecondsElapsed() > TimeBetweenMessages)
	{
		return true;
	}

	return false;
}

void StarTimer()
{
	TimerStart = std::chrono::system_clock::now();
	TimerFunc();
}

class Messenger
{
	mqtt::async_client *client;
	callback *cb;

public:
	Messenger(mqtt::async_client* a_Client, callback* a_Callback) : client(a_Client), cb(a_Callback) {}
};

int main(int argc, char* argv[])
{
	string	address = (argc > 1) ? string(argv[1]) : DFLT_SERVER_ADDRESS,
		clientID = (argc > 2) ? string(argv[2]) : CLIENT_ID;

	cout << "Initializing for server '" << address << "'..." << endl;
	//mqtt::async_client client(address, clientID, PERSIST_DIR);
	
	callback cb;
	mqtt::async_client client(address, clientID, PERSIST_DIR);

	Messenger l_Messenger(&client, &cb);
	

	
	client.set_callback(cb);

	auto connOpts = mqtt::connect_options_builder()
		.clean_session()
		.will(mqtt::message(TOPIC, LWT_PAYLOAD, QOS))
		.finalize();

	try {
		cout << "\nConnecting..." << endl;
		mqtt::token_ptr conntok = client.connect(connOpts);
		cout << "Waiting for the connection..." << endl;
		conntok->wait();
		cout << "  ...OK" << endl;

		StarTimer();

		while (1)
		{
			if (TimerFunc())
			{
				// First use a message pointer.

				cout << "\nSending message..." << endl;
				mqtt::message_ptr pubmsg = mqtt::make_message(TOPIC, PAYLOAD1);
				pubmsg->set_qos(QOS);
				client.publish(pubmsg)->wait_for(TIMEOUT);
				//cout << "  ...OK" << endl;

				// Now try with itemized publish.

				//cout << "\nSending next message..." << endl;
				mqtt::delivery_token_ptr pubtok;
				pubtok = client.publish(TOPIC, PAYLOAD2, strlen(PAYLOAD2), QOS, false);
				//cout << "  ...with token: " << pubtok->get_message_id() << endl;
				//cout << "  ...for message with " << pubtok->get_message()->get_payload().size()
				//	<< " bytes" << endl;
				pubtok->wait_for(TIMEOUT);
				//cout << "  ...OK" << endl;

				// Now try with a listener

				//cout << "\nSending next message..." << endl;
				action_listener listener;
				pubmsg = mqtt::make_message(TOPIC, PAYLOAD3);
				pubtok = client.publish(pubmsg, nullptr, listener);
				pubtok->wait();
				//cout << "  ...OK" << endl;

				// Finally try with a listener, but no token

				//cout << "\nSending final message..." << endl;
				delivery_action_listener deliveryListener;
				pubmsg = mqtt::make_message(TOPIC, PAYLOAD4);
				client.publish(pubmsg, nullptr, deliveryListener);

				while (!deliveryListener.is_done()) {
					this_thread::sleep_for(std::chrono::milliseconds(100));
				}
				//cout << "OK" << endl;

				TimerStart = std::chrono::system_clock::now();
				//this_thread::sleep_for(std::chrono::milliseconds(20000));
			}

			printf("Next Broadcast: [%d/%d] Seconds.\r", (int)SecondsElapsed, (int)TimeBetweenMessages);
		}

		// Double check that there are no pending tokens

		auto toks = client.get_pending_delivery_tokens();
		if (!toks.empty())
			cout << "Error: There are pending delivery tokens!" << endl;

		// Disconnect
		cout << "\nDisconnecting..." << endl;
		client.disconnect()->wait();
		cout << "  ...OK" << endl;
	}
	catch (const mqtt::exception& exc) {
		cerr << exc.what() << endl;
		return 1;
	}

	return 0;
}

