
#include "myspace/socket/socket.h"
#include "myspace/error/error.h"
#include "myspace/memory/memory.h"
#include "myspace/select/select.h"
#include "myspace/strings/strings.h"

myspace_begin

Socket::Socket(int sock)
{
	_sock = sock;
}

Socket::Socket(const string& addr, high_resolution_clock::duration timeout) 
{
	set_addr(addr.c_str()).connect(timeout);
}

Socket::Socket(const string& addr, uint16_t port, high_resolution_clock::duration timeout)
{
	set_addr(addr.c_str()).set_port(port).connect(timeout);
}

Socket::~Socket() 
{
	close();
}

size_t Socket::send(const string& data, high_resolution_clock::duration timeout) 
{
	size_t sendn = 0;

	for (auto this_time = high_resolution_clock::now(), begin_time = this_time;
		sendn < data.size() && this_time - begin_time <= timeout
		; this_time = high_resolution_clock::now())
	{
		auto n = ::send(_sock, data.c_str() + sendn, int(data.size() - sendn), 0);

		if (n > 0)
			sendn += n;

		else if (n == 0)
			break;

		else 
		{
			auto e = Error::lastNetError();

			if (e == Error::wouldblock || e == Error::intr || e == Error::inprogress)
			{
				auto sel = new_shared<Select>();
				sel->push(this, Select::detect_write);
				sel->wait(timeout - (this_time - begin_time));
				continue;
			}

			break;
		}
	}
	return sendn;
}

string Socket::recv(high_resolution_clock::duration timeout)
{
	string data;

	size_t buflen = 4096;

	auto buf = new_unique<char[]>(buflen);

	for (auto begin_time = high_resolution_clock::now(), this_time = begin_time;
		this_time - begin_time <= timeout;
		this_time = high_resolution_clock::now())
	{
		auto n = ::recv(_sock, buf.get(), (int)buflen, 0);

		if (n > 0)
			data.append(buf.get(), n);

		else if (n == 0)
			break;

		else 
		{
			auto e = Error::lastNetError();

			if (e == Error::wouldblock || e == Error::intr || e == Error::inprogress)
			{
				auto sel = new_shared<Select>();
				sel->push(this);
				sel->wait(timeout - (this_time - begin_time));
				continue;
			}

			break;
		}
	}
	return move(data);
}

string Socket::recv(size_t len, high_resolution_clock::duration timeout) 
{
	if (len == 0)
		return "";

	size_t recvn = 0;

	auto buf = new_unique<char[]>(len);

	for (auto this_time = high_resolution_clock::now(), begin_time = this_time;
		recvn < len && this_time - begin_time <= timeout;
		this_time = high_resolution_clock::now())
	{
		auto n = ::recv(_sock, buf.get() + recvn, int(len - recvn), 0);
		
		if (n > 0) 
		{
			recvn += n;
		}

		else if (n == 0)
			break;

		else 
		{
			auto e = Error::lastNetError();
			if (e == Error::wouldblock || e == Error::intr || e == Error::inprogress)
			{
				auto sel = new_shared<Select>();
				sel->push(this);
				sel->wait(timeout - (this_time - begin_time));
				continue;
			}
			break;
		}
	}
	return move(string(buf.get(), recvn));
}

string Socket::recv_until(const string& delm, high_resolution_clock::duration timeout)
{
	if (delm.empty())
		return move(recv(timeout));

	size_t recvn = delm.size();

	string ret;

	auto buf = new_unique<char[]>(recvn);

	for (auto this_time = system_clock::now(), begin_time = this_time;
		(ret.size() < delm.size() || ret.rfind(delm, ret.size() - delm.size()) == ret.npos)
		&& this_time - begin_time <= timeout;
		this_time = system_clock::now())
	{
		auto n = ::recv(_sock, buf.get(), int(recvn), 0);

		if (n == 0)
			break;

		if (n < 0) 
		{
			auto e = Error::lastNetError();

			if (e == Error::wouldblock || e == Error::intr || e == Error::inprogress)
			{
				auto sel = new_shared<Select>();
				sel->push(this);
				sel->wait(timeout - (this_time - begin_time));
				continue;
			}
			break;
		}

		ret.append(buf.get(), n);
		recvn = 0;
		for (size_t maxmatchn = std::min(delm.size(), ret.size()); maxmatchn; --maxmatchn)
		{
			if (ret.compare(ret.size() - maxmatchn, maxmatchn, delm, 0, maxmatchn) == 0)
			{
				recvn = delm.size() - maxmatchn;
				break;
			}
		}
		if (!recvn) recvn = delm.size();
	}
	return move(ret);
}

Socket& Socket::setblock() 
{
	return setblock(true);
}

Socket& Socket::setnonblock()
{
	return this->setblock(false);
}

bool Socket::is_blocked()
{
	return _isblocked;
}

Socket::operator bool()
{
	return is_connected();
}

int Socket::get_fd()
{
	return _sock;
}

bool Socket::is_connected()
{
again:
	char c;
	setnonblock();
	auto n = ::recv(_sock, &c, 1, MSG_PEEK);
	if (n == 0)
		return false;
	if (n > 0)
		return true;
	auto e = Error::lastNetError();

	switch (e) {
	case Error::wouldblock:
	case Error::inprogress:
		return true;
	case Error::intr:
		goto again;
	default:
		return false;
	}
}

void Socket::close()
{
	if (_sock >= 0) {
		setblock(true);
		close_socket(_sock);
		_sock = -1;
	}
}

Socket& Socket::set_addr(const char* ipport)
{
	auto tokens = Strings::split(ipport, ':');

	if (tokens.size() == 1)
	{
		_ip = Strings::strip(tokens[0]);
	}
	else if (tokens.size() >= 2)
	{
		_ip = Strings::strip(tokens[0]);
		_port = StringStream(Strings::strip(tokens[1])) ;
	}
	return *this;
}

Socket& Socket::set_port(uint16_t port)
{
	_port = port; return *this;
}

Socket& Socket::connect(high_resolution_clock::duration timeout)
{
	if (!_ip.empty() || _port != 0)
	{

		this->close();

		_sock = (int)::socket(AF_INET, SOCK_STREAM, 0);

		setnonblock();

		for (auto this_time = high_resolution_clock::now(), begin_time = this_time;
			!is_connected() && this_time - begin_time <= timeout;
			this_time = high_resolution_clock::now())
		{

			sockaddr_in addr = { 0 };
			addr.sin_family = AF_INET;
			addr.sin_port = htons(_port);
			inet_pton(AF_INET, _ip.c_str(), &addr.sin_addr.s_addr);

			auto n = ::connect(_sock, (sockaddr*)&addr, sizeof(addr));

			if (n == 0)
				return *this;

			auto e = Error::lastNetError();

			switch (e)
			{
			case Error::isconn:
				return *this;
			case Error::already:
			case Error::inprogress:
			case Error::wouldblock:
			case Error::intr:
			{
				if (is_connected())
					return *this;

				auto sel = new_shared<Select>();

				sel->push(this);

				sel->wait(timeout - (this_time - begin_time));
				break;
			}
			default:
			{
				this->close();
				_sock = (int)::socket(AF_INET, SOCK_STREAM, 0);
				setnonblock();
				this_thread::sleep_for(std::min(milliseconds(100), duration_cast<milliseconds>(timeout - (this_time - begin_time))));
				break;
			}
			}
		}
	}
	return *this;
}

Socket& Socket::setblock(bool f) 
{
#ifdef this_platform_windows
	unsigned long ul = (f ? 0 : 1);
	set_block(_sock, f);
	_isblocked = f;
#endif
	return *this;
}

int Socket::getsockerror()
{
	int err = 0;
#if defined(this_platform_windows)
	int len = sizeof(err);
#else
	socklen_t len = sizeof(err);
#endif
	::getsockopt(_sock, SOL_SOCKET, SO_ERROR, (char*)&err, &len);
	return err;
}


void set_block(int fd, bool f)
{
#ifdef this_platform_windows
	unsigned long ul = f ? 1 : 0;
	::ioctlsocket(fd, FIONBIO, (unsigned long *)&ul);
#else
	auto flag = fcntl(fd, F_GETFL, 0);
	if (f) flag |= O_NONBLOCK;
	else flag = flag & ~O_NONBLOCK;
	fcntl(fd, F_SETFL, flag);
#endif
}

void close_socket(int sock)
{
#ifdef this_platform_windows
	::closesocket(sock);
#else
	::close(sock);
#endif
}


myspace_end
