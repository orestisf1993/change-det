#!/usr/bin/env python3
from sys import stderr

with open('out.log') as f:
    s = f.read()
l = s.split('\n')
# remove empty lines
l = list(filter(bool, l))

# If the execution was cut before a signal detection, delete the last line
if (len(l) % 2):
    del l[-1]

C = []
D = []
for line in l:
    v = tuple(map(int, line[2:].split()))
    if line[0] == 'C':
        C.append(v)
    elif line[0] == 'D':
        D.append(v)

a = {}
for signal_id, time in C:
    if signal_id not in a:
        a[signal_id] = []
    a[signal_id].append(time)

b = {}
for signal_id, time in D:
    if signal_id not in b:
        b[signal_id] = []
    b[signal_id].append(time)

assert(len(a) == len(b))
c = {}
for signal_id in a:
    assert(signal_id in b)
    t1 = a[signal_id]
    t2 = b[signal_id]
    if(len(t1) != len(t2)):
        print("len() error! signal id = {0} a = {1} b = {2}".format(signal_id, len(t1), len(t2)), file=stderr)
        continue
    c[signal_id] = [(j - i)*(j>i) for i,j in zip(t1,t2)]
    # ~ for idx,i in enumerate(c[signal_id]):
        # ~ if i > 20000:
            # ~ print("big in {0} {1} => {2}!".format(signal_id,idx,i), file=stderr)

s = sum(sum(c[signal_id]) for signal_id in c)

print('total delay = {0}, average delay = {1}, total signals = {2}'.format(s, s/len(C), len(C)))
