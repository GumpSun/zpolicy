#!/bin/bash

gcc policyWatcher.c -I /usr/local/include/zookeeper/ /usr/local/lib/libzookeeper_mt.so cJSON.c utility.c parse.c -lm -g -w -o  policyWatcher

