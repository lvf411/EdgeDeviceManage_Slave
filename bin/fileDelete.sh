#!/bin/bash

mv containerID.txt ../containerID.txt
mv slave_init.json ../slave_init.json

rm -f *_*

mv ../containerID.txt ./containerID.txt
mv ../slave_init.json ./slave_init.json

ls
