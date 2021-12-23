#include <unordered_map>
#pragma once
#define MSS 1460

using namespace std;

class State ;

class CongestionControl {

public:
	CongestionControl();
	State* current_state;
	void change_state(State* new_state);
	int get_cwnd_size();
	void timeout();
	void new_ack();
	int duplicate_ack(int seqno);
};


class State {

public:
	State();
	State( int cwnd_size_param, int ssthresh_param
		, unordered_map<int, int >* map_param , CongestionControl* controller_param);
	virtual void timeout();
	virtual int duplicate_ack(int seqno);
	virtual void new_ack();
	unordered_map<int, int>* map;
	CongestionControl* controller;
	int cwnd_size;
	int ssthresh;
};


class SlowStart : public State {

public:	
	SlowStart();
	SlowStart( int cwnd_size_param, int ssthresh_param
		, unordered_map<int, int >* map_param ,  CongestionControl* controller_param);
	void timeout();
	int duplicate_ack(int seqno);
	void new_ack();
};

class FastRecovery : public State {

public:
	FastRecovery();
	FastRecovery( int cwnd_size_param, int ssthresh_param
		, unordered_map<int, int >* map_param , CongestionControl* controller_param);
	void timeout();
	int duplicate_ack(int seqno);
	void new_ack();

};


class CongestionAvoidance : public State {

public:
	CongestionAvoidance();
	CongestionAvoidance( int cwnd_size_param, int ssthresh_param
		, unordered_map<int, int >* map_param , CongestionControl* controller_param);
	void timeout();
	int duplicate_ack(int seqno);
	void new_ack();

};

