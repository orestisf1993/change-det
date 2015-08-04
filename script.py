#!/usr/bin/env python3
from sys import stderr

with open('out.log') as f:
    s = f.read()
l = s.split('\n')
# remove empty lines
l = list(filter(bool, l))

del s

# If the execution was cut before a signal detection, delete the last line
if (len(l) % 2):
    del l[-1]

# find and delete missing/extra changes in the flow
flow = {}
for line in l:
    v = tuple(line[0]) + tuple(map(int, line[2:].split()))
    sid = v[1]
    if sid not in flow:
        flow[sid] = []
    flow[sid].append(v)

for sid, values in flow.items():
    values.sort(key=lambda x: 10 * x[2] - (x[0]=='C'))
    for idx in range(1, len(values)):
        if values[idx][0] == values[idx-1][0]:
            print(values[idx], values[idx-1])

            original_string = (str(x) for x in values[idx])
            idx_to_del = l.index(' '.join(original_string))
            del l[idx_to_del]

del flow

C = []
D = []
for line in l:
    v = tuple(map(int, line[2:].split()))
    if line[0] == 'C':
        C.append(v)
    elif line[0] == 'D':
        D.append(v)

del l

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

total_elements = len(C)

del C
del D

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

print('total delay = {0}, average delay = {1}, total signals = {2}'.format(s, s/total_elements, total_elements))
