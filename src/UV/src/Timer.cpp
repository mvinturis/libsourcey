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


#include "Sourcey/UV/Timer.h"
#include "Sourcey/Logger.h"
#include "assert.h"


using namespace std;


namespace scy {
namespace UV {


Timer::Timer(uv_loop_t* loop) :
	Base(loop),
	_active(false),
	_count(0)
{
	LogTrace("Timer", this) << "Creating" << endl;

    int r = uv_timer_init(_loop, &_handle);
    assert(r == 0);

    _handle.data = this;
	
	// TODO: reenable
    // Only increase the ref count of the
	// loop when the timer is active.
    uv_unref((uv_handle_t*)&_handle);
}


Timer::~Timer()
{
	LogTrace("Timer", this) << "Destroying" << endl;

	// TODO: reenable
	//uv_unref((uv_handle_t*)&_handle);
    uv_close((uv_handle_t*)&_handle, UV::defaultClose);
}


bool Timer::start(Int64 timeout, Int64 interval) 
{
	LogTrace("Timer", this) << "Starting: " << timeout << ": " << interval << endl;
    int r = uv_timer_start(&_handle, UV::timerCallback, timeout, interval);
    if (r) setErrno(uv_last_error(_loop));
	updateState();
    return r == 0;
}


bool Timer::stop() 
{
	LogTrace("Timer", this) << "Stoping" << endl;
    int r = uv_timer_stop(&_handle);
    if (r) setErrno(uv_last_error(_loop));
	updateState();
    return r == 0;
}


bool Timer::again() 
{
	LogTrace("Timer", this) << "Again" << endl;
    int r = uv_timer_again(&_handle);
    if (r) setErrno(uv_last_error(_loop));
	updateState();
    return r == 0;
}


void Timer::setInterval(Int64 interval)
{
	LogTrace("Timer", this) << "Set Interval: " << interval << endl;
    uv_timer_set_repeat(&_handle, interval);
}


Int64 Timer::getInterval()
{	
    Int64 interval = uv_timer_get_repeat(&_handle);
    if (interval < 0) 
		setErrno(uv_last_error(_loop));
	return interval;
}


/*
void Timer::setErrno(const uv_err_t& err)
{
	LogTrace("error") << "[Timer:" << this << "] Error: " << uv_strerror(err) << endl;	
}
*/


void Timer::updateState()
{
	_count = 0;
	bool wasActive = _active;
	_active = !!uv_is_active((uv_handle_t*) &_handle);
		
	LogTrace("Timer", this) << "Update State: " << _active << endl;
	if (!wasActive && _active) {
		// If our state is changing from inactive to active, we
		// increase the loop's reference count.
		// TODO: reenable
		uv_ref((uv_handle_t*)&_handle);
	} 
	else if (wasActive && !_active) {
		// If our state is changing from active to inactive, we
		// decrease the loop's reference count.
		// TODO: reenable
		uv_unref((uv_handle_t*)&_handle);
	}
}


Int64 Timer::count()
{
	return _count;
}



//
// Callbacks
//
void Timer::timerCallback(int status)
{
	_count++;
	OnTimeout.emit(this, _count);
}


} } // namespace scy::UV
