#include "CongestionControl.h"
#include <iostream>

using namespace std;

CongestionControl::CongestionControl()
{
	this->current_state = new SlowStart( MSS, 65536, new unordered_map<int,int> ,  this);
}

void CongestionControl::change_state(State* new_state)
{
	if (this->current_state != nullptr)
		delete this->current_state;
	this->current_state = new_state;
}

int CongestionControl::get_cwnd_size()
{
	return this->current_state->cwnd_size;
}

void CongestionControl::timeout()
{
	this->current_state->timeout();
}

void CongestionControl::new_ack()
{
	this->current_state->new_ack();
}

int CongestionControl::duplicate_ack(int seqno)
{
	return this->current_state->duplicate_ack(seqno);
}

State::State()
{
	this->cwnd_size = 0;
	this->ssthresh = 0;
	this->controller = nullptr;
	this->map = nullptr ;
}

State::State( int cwnd_size_param, int ssthresh_param,
	unordered_map<int, int >* map_param  , CongestionControl* controller_param)
{
	this->cwnd_size = cwnd_size_param;
	this->ssthresh = ssthresh_param;
	this->controller = controller_param;
	this->map = map_param;
}

void State::timeout()
{
}

int State::duplicate_ack(int seqno)
{
	return 0 ;
}

void State::new_ack()
{
}

SlowStart::SlowStart() : State()
{
}

SlowStart::SlowStart( int cwnd_size_param,
	int ssthresh_param, unordered_map<int, int >* map_param  , CongestionControl* controller_param) :
	State( cwnd_size_param ,  ssthresh_param , map_param , controller_param )
{
}

void SlowStart::timeout()                            
{
	this->ssthresh = this->cwnd_size / 2;
	this->cwnd_size = MSS;
	this->map->clear();
}

int SlowStart::duplicate_ack( int seqno )
{
	if ( ! this->map->contains(seqno) )
		this->map->insert({seqno,1});
	else {
		auto x = this->map->find(seqno) ;
		x->second++;
		if (x->second == 3) {
			this->ssthresh = this->cwnd_size / 2;
			this->cwnd_size = this->ssthresh + 3 * MSS;
			std::cout << "transition from slow start to fast recovery\n" << endl;
			this->controller->change_state(new FastRecovery(this->cwnd_size,
				this->ssthresh, this->map, this->controller));
			return seqno ;
		}
	}
	return -1;
}

void SlowStart::new_ack()
{
	this->cwnd_size += MSS;
	this->map->clear();
	if (this->cwnd_size >= ssthresh) {
		std::cout << "transition from slow start to avoidance\n" << endl;
		this->controller->change_state(new CongestionAvoidance( this->cwnd_size,
			this->ssthresh, this->map, this->controller));
	}
}

FastRecovery::FastRecovery() : State()
{
}

FastRecovery::FastRecovery( int cwnd_size_param,
	int ssthresh_param, unordered_map<int, int >* map_param , CongestionControl* controller_param):
	State( cwnd_size_param, ssthresh_param, map_param , controller_param)
{
}

void FastRecovery::timeout()
{
	this->ssthresh = this->cwnd_size / 2;
	this->cwnd_size = MSS;
	this->map->clear();
	std::cout << "transition from fast recovery to slow start\n" << endl;
	this->controller->change_state(new SlowStart( this->cwnd_size,
		this->ssthresh, this->map, this->controller));
}

int FastRecovery::duplicate_ack(int seqno)
{
	this->cwnd_size += MSS;
	return -1;
}

void FastRecovery::new_ack()
{
	this->cwnd_size = this->ssthresh;
	this->map->clear();
	std::cout << "transition from fast recovery to avoidance\n" << endl;
	this->controller->change_state(new CongestionAvoidance( this->cwnd_size
		, this->ssthresh, this->map, this->controller));
}

CongestionAvoidance::CongestionAvoidance() :State()
{
}

CongestionAvoidance::CongestionAvoidance( int cwnd_size_param,
	int ssthresh_param, unordered_map<int, int >* map_param , CongestionControl* controller_param) :
	State( cwnd_size_param, ssthresh_param, map_param , controller_param)
{
}

void CongestionAvoidance::timeout()
{
	this->ssthresh = this->cwnd_size / 2;
	this->cwnd_size = MSS;
	this->map->clear();
	cout << "transition from avoidance to slow start\n" << endl;
	this->controller->change_state(new SlowStart( this->cwnd_size,
		this->ssthresh, this->map, this->controller));
}

int CongestionAvoidance::duplicate_ack(int seqno)
{
	if (!this->map->contains(seqno))
		this->map->insert({ seqno,1 });
	else {
		auto x = this->map->find(seqno);
		x->second++;
		if (x->second == 3) {
			this->ssthresh = this->cwnd_size / 2;
			this->cwnd_size = this->ssthresh + 3 * MSS;
			cout << "transition from avoidance to fast recovery\n" << endl ;
			this->controller->change_state(new FastRecovery(this->cwnd_size,
				this->ssthresh, this->map, this->controller));
			return seqno;
		}
	}
	return -1;
}

void CongestionAvoidance::new_ack()
{
	this->cwnd_size = this->cwnd_size + MSS * MSS / this->cwnd_size;
	this->map->clear();
}
