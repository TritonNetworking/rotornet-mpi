#!/usr/bin/env python

import argparse
import sys

parser = argparse.ArgumentParser(description='Process broadcast files')
parser.add_argument("input_file")
args = parser.parse_args()

with open(args.input_file) as f:
	content = f.readlines()

content = [x.strip() for x in content]
print(content[0])

# process start time
(beforetag, beforestr) = content[1].split(":")
assert beforetag == "Before"
before = long(beforestr.strip())

# process all peers
mids = []
for i in range(2, len(content)-1):
	(midtag, midstr) = content[i].split(":")
	mids.append(long(midstr.strip()))

# process end time
(aftertag, afterstr) = content[-1].split(":")
assert aftertag == "After"
after = long(afterstr.strip())

print("master_begin: " + str(before - before))
for i in range(0, len(mids)):
	print("mid_{0}: {1}".format(i, mids[i] - before))
	
print("master_end: " + str(after - before))
print("")
