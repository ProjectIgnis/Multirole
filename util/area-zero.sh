#!/bin/sh

MULTIROLE_PID=0

ts_echo() {
	date "+[%Y-%m-%d %T.???] $1"
}

launch_multirole() {
	ts_echo "Launching Multirole..."
	./multirole &
	MULTIROLE_PID=$!
}

launch_multirole_if_not_running() {
	kill -s 0 $MULTIROLE_PID 2>/dev/null && return
	ts_echo "Multirole exited without my signaling! Relaunching..."
	launch_multirole
}

term_and_launch_multirole() {
	ts_echo "Signaling Multirole..."
	save_traps=$(trap)
	[ $MULTIROLE_PID -ne 0 ] && kill $MULTIROLE_PID >/dev/null
	launch_multirole
	eval "$save_traps"
}

launch_multirole
trap "term_and_launch_multirole" TERM
trap "launch_multirole_if_not_running" CHLD

while true; do
	sleep 1
done
