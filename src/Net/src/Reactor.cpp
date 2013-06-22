//
// LibSourcey
// Copyright (C) 2005, Sourcey <http://sourcey.com>
//
// LibSourcey is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// LibSourcey is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//


#include "Sourcey/Net/Reactor.h"
#include "Sourcey/Logger.h"

#include "Poco/Net/NetException.h"
#include "Poco/SingletonHolder.h"


using namespace std;
using namespace Poco;


namespace scy {
namespace Net {


Reactor::Reactor(int timeout) : 
	_thread("Reactor"),
	_timeout(timeout),
	_stop(false)	
{
	_thread.start(*this);
}


Reactor::~Reactor()
{
	stop();
}


void Reactor::run()
{	
	Poco::Net::Socket::SocketList readable;
	Poco::Net::Socket::SocketList writable;
	Poco::Net::Socket::SocketList error;
	
	while (!_stop)
	{		
		try
		{
			readable.clear();
			writable.clear();
			error.clear();

			if (_delegates.empty()) {				
				log("trace") << "Sleeping" << endl;
				_wakeUp.wait();		
				log("trace") << "Waking up" << endl;
			}
			{
				FastMutex::ScopedLock lock(_mutex);
				for (DelegateList::iterator it = _delegates.begin(); it != _delegates.end(); ++it)
				{
					switch ((*it)->event) {
					case SocketReadable:
						readable.push_back((*it)->socket);
						break;		
					case SocketWritable:
						writable.push_back((*it)->socket);
						break;
					case SocketError:
						error.push_back((*it)->socket);
						break;
					default:
						assert(false);
						break;
					}
				}
			}
				
			if (Poco::Net::Socket::select(readable, writable, error, _timeout)) 
			{
				for (Poco::Net::Socket::SocketList::iterator it = readable.begin(); it != readable.end(); ++it) {
					emit(*it, SocketReadable);
				}
				for (Poco::Net::Socket::SocketList::iterator it = writable.begin(); it != writable.end(); ++it) {
					emit(*it, SocketWritable);
				}
				for (Poco::Net::Socket::SocketList::iterator it = error.begin(); it != error.end(); ++it)
					emit(*it, SocketError);
			} 

			Thread::sleep(5); // gulp!
		}
		catch (Exception& exc)
		{
			handleException(exc);
		}
		catch (exception& exc)
		{
			handleException(exc);
		}
		catch (...)
		{
			log("error") << "Unknown Error" << endl;
#ifdef _DEBUG	
			throw 0;
#endif
		}
	}
	
	log("trace") << "Shutdown" << endl;
	Shutdown.emit(this);
	log("trace") << "Exiting" << endl;
}


void Reactor::handleException(const Exception& exc)
{
	log("error") << "Error " << exc.displayText() << endl;

#ifdef _DEBUG	
	exc.rethrow();
#endif
}


void Reactor::handleException(const exception& exc)
{
	log("error") << "Error " << exc.what() << endl;

#ifdef _DEBUG	
	throw exc;
#endif
}

	
void Reactor::stop()
{
	log("trace") << "Stopping" << endl;

	_stop = true;
	_wakeUp.set();
	_thread.join();
}


void Reactor::attach(const Poco::Net::Socket& socket, const ReactorDelegate& delegate)
{
	log("trace") << "Attach: " << socket.impl() << ": " << delegate.event << endl;

	detach(socket, delegate);

	FastMutex::ScopedLock lock(_mutex);	
	
	ReactorDelegate* d = delegate.clone();
	d->socket = Poco::Net::Socket(socket);
	_delegates.push_back(d);
	
	_wakeUp.set();	
	
	/*
	//d->socket = &socket;
	switch (delegate.event) {
	case SocketReadable:
		_readable.push_back(socket);
		break;		
	case SocketWritable:
		_writable.push_back(socket);
		break;
	case SocketError:
		_error.push_back(socket);
		break;
	default:
		assert(false);
		break;
	}
	*/

	//log("trace") << "Attach: OK: " << socket.impl() << ": " << delegate.event << endl;
}


void Reactor::detach(const Poco::Net::Socket& socket, const ReactorDelegate& delegate) 
{	
	//log("trace") << "Detach: " << socket.impl() << ": " << delegate.event << endl;
	FastMutex::ScopedLock lock(_mutex);
	
	for (DelegateList::iterator it = _delegates.begin(); it != _delegates.end(); ++it) {
		if ((*it)->socket == socket && delegate.equals(*it)) {
			delete *it;
			_delegates.erase(it);
			break;
		}
	}
}


void Reactor::detach(const Poco::Net::Socket& socket)
{
	//log("trace") << "Detach: " << socket.impl() << endl;
	FastMutex::ScopedLock lock(_mutex);

	for (DelegateList::iterator it = _delegates.begin(); it != _delegates.end(); ++it) {
		if ((*it)->socket == socket) {
			delete *it;
			it = _delegates.erase(it);
		}
		else ++it;
	}
}


void Reactor::emit(const Poco::Net::Socket& socket, SocketEvent event)
{
	//log("trace") << "Dispatch: " << socket.impl() << endl;
	ReactorDelegate* delegate = NULL;
	{
		FastMutex::ScopedLock lock(_mutex);	
		for (DelegateList::iterator it = _delegates.begin(); it != _delegates.end(); ++it) {
			if ((*it)->socket == socket && (*it)->event == event) {
				delegate = *it;
				break;
			}
		}
	}

	if (delegate)
		delegate->emit(this, *this, socket, event, 0);
}


Reactor& Reactor::getDefault() 
{
	static Poco::SingletonHolder<Reactor> sh;
	return *sh.get();
}


} } // namespace scy::Net