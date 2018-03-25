
#include "myspace/myspace.hpp"
using namespace myspace;

void test_detector()
{
	auto listenThread = thread([] 
	{
		try
		{
			struct Ctx
			{
				string				_recv;

				string				_write;
			};

			Detector detector;

			unordered_map<shared_ptr<Socket>, Ctx> conns;

			auto l = new_shared<Listener>(6100);
			SocketOpt::setBlock((int)*l, false);

			for (;;)
			{
				detector.add(l);

				for (auto& c : conns)
				{
					if (c.second._write.empty())
					{
						detector.aod(c.first);
					}
					else
					{
						detector.aod(c.first, READ_WRITE);
					}
				}

				logger.debug("before wait");

				auto active = detector.wait();

				logger.debug("active : ", active.size(), ", conns + listener : ", conns.size() + 1);

				for (auto& a : active[READ])
				{
					if (a.is<shared_ptr<Listener>>())
					{
						auto l = a.as<shared_ptr<Listener>>();

						auto s = l->accept(seconds(0));

						if (s)
						{
							SocketOpt::setBlock((int)*s, false);
							conns[s] = Ctx();
							logger.debug("new conn");
						}
					}
					else if (a.is <shared_ptr<Socket>>())
					{
						auto s = a.as<shared_ptr<Socket>>();

						auto& ctx = conns[s];

						logger.debug(" SOCKET = ", s);

						ctx._recv.append(s->recv(seconds(0)));

						if (!ctx._recv.empty())
						{
							logger.debug("client says : ", ctx._recv);

							ctx._write.append(move(ctx._recv));

							ctx._recv.clear();
						}

						if (!ctx._write.empty())
						{
							auto n = s->send(ctx._write, seconds(0));

							logger.debug("RESPONSE: ", ctx._write.substr(0, n));

							ctx._write.erase(0, n);
						}

						if (!s->isConnected())
						{
							conns.erase(s);

							logger.debug("client quit");

							break;
						}
					}
				}
			}
		}
		catch (...)
		{
			logger.error(Exception::dump());
		}
	});

	listenThread.join();
}


void test_any()
{
	try
	{
		deque<Any> q;

		q.push_back(Socket());
		q.push_back(Listener());

		cout << boolalpha;
		cout << q[0].is<Socket>() << endl;
		cout << q[1].is<Listener>() << endl;
	}
	catch (...)
	{
		cout << Exception::dump() << endl;
	}
}

int main()
{
#ifdef MS_WINDOWS
	WSADATA wd;
	WSAStartup(MAKEWORD(1, 1), &wd);
#endif
#if 0
	test_any();
#endif
	test_detector();
	
#ifdef MS_WINDOWS
	system("pause");
#endif
	return 0;
}