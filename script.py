#!/usr/bin/env python3
"""This module parses the output log of pace and prints various statistical data.

    Attributes:
        OUTPUT_NAME: name of the log file to be read.
        TYPE_IDX: index of the signal type ("C"/"D").
        SID_IDX: index of the signal id.
        TIME_IDX: index of the time of the signal change/detection.
"""

import operator
import re
# ~ from sys import stderr
from itertools import groupby
import collections
import numpy as np

OUTPUT_NAME = 'out.log'
TYPE_IDX = 0
SID_IDX = 1
TIME_IDX = 2


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
        for element in flow:
            print(element)
        return len(flow)

    # valid_idx[0] isn't 0
    total_errors = valid_idx[0]
    for error in range(total_errors):
        print(flow[error])

    # all lost errors in between
    for i in range(1, len(valid_idx)):
        errors = valid_idx[i] - valid_idx[i - 1] - 2
        total_errors += errors
        for error in range(1, errors + 1):
            error_idx = valid_idx[i - 1] + 2 * error
            print(flow[error_idx])

    # last elements in flow not found
    for error in flow[valid_idx[-1] + 2:]:
        total_errors += 1
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
    


def main():
# ~ if __name__ == "__main__":
    """Main function."""
    with open(OUTPUT_NAME) as log_file:
        log_string = log_file.read().split('\n')
        log_splitted = (
            tuple(line[0]) +    # line[0] is 'C' or 'D'
            tuple(map(int, line[2:].split()))
            # tuple(int(x) for x in line[2:].split()) <- slower
            for line in log_string if line
        )
        del log_string

    # group entries by their signal id
    log_grouped = groupby(
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

            if diff:
                max_idx = np.argmax(diff)
                if diff[max_idx] > 50:
                    print("{0}: max diff = {1} at {2} => {3}".format(_, diff[max_idx], max_idx, flow[valid_idx[max_idx]]))
    
            time_sum += sum(diff)
            errors_sum += total_errors
            signals_sum += total_elements
            total_changes += sum(x[TYPE_IDX] == 'C' for x in flow)
            valid_changes += len(valid_idx)

    if valid_changes:
        average_delay = time_sum / valid_changes
    else:
        average_delay = "No valid detections"
    print(
        "Total delay: {0}\n"
        "Average delay: {1}\n"
        "Total signal changes: {2}\n"
        "Total signal detections: {3}\n"
        "Total errors: {4}\n"
        "Error percentage: {5}%".format(
            time_sum,
            average_delay,
            total_changes,
            signals_sum - total_changes,
            errors_sum,
            errors_sum / signals_sum * 100
        )
    )
    # ~ plot_data(signal_data)


if __name__ == "__main__":
    main()
