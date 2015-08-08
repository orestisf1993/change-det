#!/usr/bin/env python3
"""This module parses the output log of pace and prints various statistical data.

    Attributes:
        OUTPUT_NAME: name of the log file to be read.
        TYPE_IDX: index of the signal type ("C"/"D").
        SID_IDX: index of the signal id.
        TIME_IDX: index of the time of the signal change/detection.
"""

from operator import itemgetter
import re
# ~ from sys import stderr
from itertools import groupby

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


def main():
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
        sorted(log_splitted, key=itemgetter(SID_IDX)),
        itemgetter(SID_IDX)
    )
    time_sum = 0
    errors_sum = 0
    for _, flow in log_grouped:
        flow = sorted(flow,
                      key=lambda x: 10 * x[TIME_IDX] - (x[TYPE_IDX] == 'C'))
        valid_idx = find_valid(flow)

        total_errors = find_errors(valid_idx, flow)
        diff = [flow[i + 1][TIME_IDX] - flow[i][TIME_IDX] for i in valid_idx]

        time_sum += sum(diff)
        errors_sum += total_errors
    print(time_sum)
    print(errors_sum)


if __name__ == "__main__":
    main()
