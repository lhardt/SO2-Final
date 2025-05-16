#!/usr/bin/env bash
set -euo pipefail

opt=""
env_target=${TARGET:-""}

if [[ $env_target != '' ]] ; then
	opt=$env_target

elif [[ $# -ge 1 ]] ; then
	opt= $1
else 
	printf 'Which executable to run? [server/client]\n>>>\t' 
	read opt
fi

if [[ $opt == 'server' ]]; then
	./bin/server
elif [[ $opt == 'client' ]]; then
	./bin/client username 127.0.0.1 4000
else 
	echo "Invalid option" $opt
fi
