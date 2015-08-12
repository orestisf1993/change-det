#!/usr/bin/env python3
"""This module parses the output log of pace and prints various statistical data.

    Attributes:
        output_name: name of the log file to be read.
        TYPE_IDX: index of the signal type ("C"/"D").
        SID_IDX: index of the signal id.
        TIME_IDX: index of the time of the signal change/detection.
"""

import operator
import re
from sys import stderr
import itertools
import collections
import numpy as np

TYPE_IDX = 0
SID_IDX = 1
TIME_IDX = 2
PRINT_ERRORS = False
CHECK_FILE_EXISTS = True

def find_valid(flow):
    """Finds correct "CD" sub-sequences of changed and detected signals.

    Args:
        flow(iterable): The whole sequence for a specific signal id, sorted by time.

    Returns:
        An iterable of the indeces of the correct sequences.

    """

    string_flow = ''.join([line[0] for line in flow])
    valid = re.finditer("CD", string_flow)
    # ~ return (x.start() for x in valid)
    return [x.start() for x in valid]


def find_errors(valid_idx, flow):
    """Finds the errors according to valid_idx and the flow

    Args:
        flow(iterable): The whole sequence for a specific signal id, sorted by time.

    Returns:
        int: The number of errors found.

    """

    # valid_idx is empty but flow isn't
    if not valid_idx:
        if PRINT_ERRORS:
            for element in flow:
                print(element)
        return len(flow)

    # valid_idx[0] isn't 0
    total_errors = valid_idx[0]
    if PRINT_ERRORS:
        for error in range(total_errors):
            print(flow[error])

    # all lost errors in between
    for i in range(1, len(valid_idx)):
        errors = valid_idx[i] - valid_idx[i - 1] - 2
        total_errors += errors
        for error in range(1, errors + 1):
            error_idx = valid_idx[i - 1] + 2 * error
            if PRINT_ERRORS:
                print(flow[error_idx])

    # last elements in flow not found
    for error in flow[valid_idx[-1] + 2:]:
        total_errors += 1
        if PRINT_ERRORS:
            print(error)

    return total_errors

def set_filtered(get_fun, set_fun, op=operator.ge, compare_arg=0):
    data = get_fun()
    data = data[op(data, compare_arg)]
    set_fun(data)
    

def plot_data(data):
    """Plots the data collected by out.log

    Args:
        data(SignalData): The data containing the diffs.

    """

    y = np.array([i.total_errors / i.total_elements * 100 for i in data])
    x = range(len(y))

    print("average: {0}, max: {1} most elements: {2}".format(np.average(y), max(y), max(i.total_elements for i in data)))
    
    import matplotlib.pyplot as plt
    import matplotlib as mpl
    import latexify

    latexify.latexify(mpl)

    t = plt.subplots(nrows=1)
    fig = t[0]
    ax = t[1]

    ax.set_xlabel("Signal ID")
    ax.set_ylabel("Error percentage")
    ax.set_title("Error percentage for each signal id")

    
    # ~ ax.plot(x, y, 'ro', linewidth=0.1)
    ax.scatter(x,y,s=0.1)
    set_filtered(ax.get_yticks, ax.set_yticks)
    set_filtered(ax.get_xticks, ax.set_xticks)
    ax.set_xlim(left=0, right=max(x)*1.01)

    # normal scale
    # ~ ax.set_ylim(bottom=-max(y)/10, top=max(y)*1.01)
    
    # symlog scale
    ax.set_yscale('symlog')
    from math import log
    ax.set_ylim(bottom=-0.1)    
    
    plt.tight_layout()
    latexify.format_axes(ax)
    plt.savefig("image.pdf")
    


def process_output(output_name='out.log'):
    with open(output_name) as log_file:
        log_string = log_file.read().split('\n')
        log_splitted = (
            tuple(line[0]) +    # line[0] is 'C' or 'D'
            tuple(map(int, line[2:].split()))
            # tuple(int(x) for x in line[2:].split()) <- slower
            for line in log_string if line
        )
        del log_string

    # group entries by their signal id
    log_grouped = itertools.groupby(
        sorted(log_splitted, key=operator.itemgetter(SID_IDX)),
        operator.itemgetter(SID_IDX)
    )

    SignalData = collections.namedtuple(    # pylint: disable=C0103
        'SignalData', 'diff total_elements total_errors'
    )

    time_sum = 0
    errors_sum = 0
    signals_sum = 0
    total_changes = 0
    valid_changes = 0
    change_delay = 0
    signal_data = []
    for _, flow in log_grouped:
        flow = sorted(flow,
                      key=lambda x: 10 * x[TIME_IDX] - (x[TYPE_IDX] == 'C'))

        total_elements = len(flow)
        if total_elements:
            valid_idx = find_valid(flow)
    
            total_errors = find_errors(valid_idx, flow)
            diff = np.array([flow[i + 1][TIME_IDX] - flow[i][TIME_IDX]
                             for i in valid_idx])

            signal_data.append(SignalData(diff, total_elements, total_errors))

            if len(diff):
                max_idx = np.argmax(diff)
                if diff[max_idx] > 50:
                    print("{0}: max diff = {1} at {2} => {3}".format(_, diff[max_idx], max_idx, flow[valid_idx[max_idx]]), file=stderr)
            changed = [x for x in flow if x[TYPE_IDX] == 'C']
            
            time_sum += sum(diff)
            errors_sum += total_errors
            signals_sum += total_elements
            total_changes += len(changed)
            valid_changes += len(valid_idx)
            change_delay += sum([changed[i+1][TIME_IDX] - changed[i][TIME_IDX] for i in range(len(changed)-1)])
    change_delay = change_delay / total_changes
    if valid_changes:
        delay = time_sum / valid_changes
    else:
        delay = "No valid detections"
    print(
        "Total delay: {0}\n"
        "Average delay: {1}\n"
        "Total signal changes: {2}\n"
        "Total signal detections: {3}\n"
        "Total errors: {4}\n"
        "Error percentage: {5}%\n"
        "Average delay between changes for the same signal: {6} usec".format(
            time_sum,
            delay,
            total_changes,
            signals_sum - total_changes,
            errors_sum,
            errors_sum / signals_sum * 100,
            change_delay
        )
    )

    return (delay, total_changes, errors_sum / signals_sum * 100, change_delay)
    
def execute_pace(arguments):
    from subprocess import call
    import os.path

    n = arguments['N']
    time_mult = arguments['time_mult']
    nthreads = arguments['nthreads']
    execution_time = arguments['execution_time']
    
    fname = r"results/out{0}_{1}_{2}_{3}.log".format(n, time_mult, execution_time, nthreads)
    if not (CHECK_FILE_EXISTS and os.path.isfile(fname)):
        cmd = r"./pace {0} {1} {2} {3} > {4}".format(n, time_mult, execution_time, nthreads, fname)
        print(cmd, file=stderr)
        try:
            call(cmd, shell=True)
        except:
            # make sure we don't leave a garbage file
            from os import remove
            remove(fname)
            raise
    return fname
    
def execute_all():
    options = {"nthreads": [1, 2, 4, 8],
               "time_mult": [0, 10, 100, 1000, 10000],
               "execution_time": [1],
               'N': list(range(1,8)) + list(range(8, 32, 4)) + [32, 10**2, 10**3, 10**4]
               }
    for t in itertools.product(*options.values()):
        arguments = {key: t[idx] for idx, key in enumerate(options.keys())}
        output_name = execute_pace(arguments)
        print(output_name, file=stderr)
        delay, total_changes, error_p, ch_delay = process_output(output_name)
    
def main():
    from sys import argv
    if argv[1] == 'execute':
        execute_all()
    else:
        output_name = 'out.log'
        process_output(output_name)

if __name__ == "__main__":
    main()
