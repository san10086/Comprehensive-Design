#!/bin/sh
set -x

if [ $# -eq 0 ]; then
    set -- smoke
fi

MODE=$1

case "$MODE" in
    directions|--help|-h) exec ./app_host "$@" ;;
esac
for ARG in "$@"; do
    if [ "$ARG" = "--check" ]; then
        exec ./app_host "$@"
    fi
done

case "$MODE" in
    smoke|audio|record|file) ;;
    *) echo "Unknown mode: $MODE; use ./run.sh --help"; exit 1 ;;
esac

# Reject malformed commands before loading firmware or opening audio devices.
./app_host --check "$@" || exit $?

if [ "$MODE" = "record" ]; then
    ./app_host "$@"
else
    ./slaveloader startup DSP server_dsp.xe674
    SLAVE_STATUS=$?
    if [ $SLAVE_STATUS -ne 0 ]; then
        echo "slaveloader startup failed: $SLAVE_STATUS"
        exit $SLAVE_STATUS
    fi

    ./app_host DSP "$@"
    APP_STATUS=$?
    ./slaveloader shutdown DSP
    SHUTDOWN_STATUS=$?
    if [ $SHUTDOWN_STATUS -ne 0 ]; then
        echo "warning: slaveloader shutdown failed: $SHUTDOWN_STATUS"
    fi
    exit $APP_STATUS
fi
