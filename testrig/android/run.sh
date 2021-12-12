#!/bin/bash

go build ../rig.go

./rig -serve &
PID=$!

./gradlew connectedCheck
CODE=$?

kill $PID
wait $PID
exit $CODE
