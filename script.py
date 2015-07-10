#!/usr/bin/env python3

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
    assert(len(t1) == len(t2))
    c[signal_id] = [(j - i)*(j>i) for i,j in zip(t1,t2)]

s = 0
for signal_id in c:
    s += sum(c[signal_id])

print('total delay = {0}, average delay = {1}'.format(s, s/len(C)))
