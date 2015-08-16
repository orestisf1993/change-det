#!/usr/bin/env python3
"""This module parses the output log of pace and prints various statistical data.

    Attributes:
        TYPE_IDX: index of the signal type ("C"/"D").
        SID_IDX: index of the signal id.
        TIME_IDX: index of the time of the signal change/detection.
        PRINT_ERRORS: If True errors will be printed in stderr.
        CHECK_FILE_EXISTS: If True check if output/pickle files are already there.
        result_fold: Where to save results.
        OPTIONS: Contains the list of arguments to be used for the program calls.
"""

import operator
import re
from sys import stderr
import itertools
import numpy as np
import pickle
import os.path

TYPE_IDX = 0
SID_IDX = 1
TIME_IDX = 2
PRINT_ERRORS = False
CHECK_FILE_EXISTS = True
RESULTS_FOLD = 'results'
OPTIONS = {"nthreads": [1, 2, 3, 6],
           "time_mult": [1, 10, 100],
           "execution_time": [10],
           'N': [1, 2, 3, 6, 32, 100, 1000, 10000, 100000, 1000000]}


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
        valid_idx(list): List with the indeces of correct changes/detections.
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


def yieldor(file_name):
    """Open a file and returns it's contents line by line after parsing it's content.

    Args:
        file_name(str): The filename.

    Yields:
        tuple: The next parsed line of the file.

    """

    file_object = open(file_name)
    for line in file_object:
        yield tuple(line[0]) + tuple(map(int, line[2:].split()))
    file_object.close()


def process_output(output_name='out.log'):
    """Process the output file and calculate statistical data.

    Args:
        output_name: The filename of the output file.

    Returns:
        tuple: Contains the total delay, total signal changes, total errors,
            total signals, average delay between 2 changes, numpy array with all the delay times.

    """
    # group entries by their signal id
    log_grouped = itertools.groupby(
        sorted(yieldor(output_name), key=operator.itemgetter(SID_IDX)),
        operator.itemgetter(SID_IDX)
    )

    stats = {
        "time_sum": 0,
        "errors_sum": 0,
        "signals_sum": 0,
        "total_changes": 0,
        "valid_changes": 0,
        "change_delay": 0,
        "all_delays": []
    }

    for _, flow in log_grouped:
        flow = sorted(flow,
                      key=lambda x: 10 * x[TIME_IDX] - (x[TYPE_IDX] == 'C'))

        total_elements = len(flow)
        if total_elements:
            valid_idx = find_valid(flow)

            total_errors = find_errors(valid_idx, flow)
            diff = np.array([flow[i + 1][TIME_IDX] - flow[i][TIME_IDX]
                             for i in valid_idx])

            stats['all_delays'].append(diff)
            changed = [x for x in flow if x[TYPE_IDX] == 'C']

            stats['time_sum'] += sum(diff)
            stats['errors_sum'] += total_errors
            stats['signals_sum'] += total_elements
            stats['total_changes'] += len(changed)
            stats['valid_changes'] += len(valid_idx)
            stats['change_delay'] += sum([changed[i + 1][TIME_IDX] - changed[i][TIME_IDX]
                                          for i in range(len(changed) - 1)])
    stats['change_delay'] = stats['change_delay'] / stats['total_changes']
    if stats['valid_changes']:
        delay = stats['time_sum'] / stats['valid_changes']
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
            stats['time_sum'],
            delay,
            stats['total_changes'],
            stats['signals_sum'] - stats['total_changes'],
            stats['errors_sum'],
            stats['errors_sum'] / stats['signals_sum'] * 100,
            stats['change_delay']
        )
    )

    return (
        delay,
        stats['total_changes'],
        stats['errors_sum'],
        stats['signals_sum'],
        stats['change_delay'],
        stats['all_delays'])



def remove_garbage(fname):
    """Remove a file if it exists

    Args:
        fname: The filename of the file.

    """
    # make sure we don't leave a garbage file
    from os import remove
    if os.path.isfile(fname):
        remove(fname)
    raise


def execute_pace(arguments, extra_code):
    """Execute the program.

    Args:
        arguments(dict): The arguments to pass to pace.

    Returns:
        str: The filename of the output file.
        extra_code(str): extra code to be appended to RESULTS_FOLD.
    """
    from subprocess import call

    n_signals = arguments['N']
    time_mult = arguments['time_mult']
    nthreads = arguments['nthreads']
    execution_time = arguments['execution_time']

    fname = RESULTS_FOLD + extra_code + \
        r"/out{0}_{1}_{2}_{3}.log".format(n_signals, time_mult, execution_time, nthreads)
    if not (CHECK_FILE_EXISTS and os.path.isfile(fname)):
        cmd = r"./pace" + \
            " {0} {1} {2} {3} > {4}".format(n_signals, time_mult, execution_time, nthreads, fname)
        print(cmd, file=stderr)
        try:
            call(cmd, shell=True)
        except Exception:
            remove_garbage(fname)
            raise
    return fname


def execution_thread(combination, extra_code):
    """Handle the execution of the program for a set of options.

    Args:
        combination: A combination of the lists in OPTIONS.
        extra_code(str): extra code to be appended to RESULTS_FOLD.

    """

    arguments = {key: combination[idx]
                 for idx, key in enumerate(OPTIONS.keys())}
    if arguments["N"] > arguments["nthreads"]:
        return
    output_name = execute_pace(arguments, extra_code)
    print("produced " + output_name, file=stderr)
    fname = output_name.replace('results', 'proc')
    if not (CHECK_FILE_EXISTS and os.path.isfile(fname)):
        try:
            res = process_output(output_name)
            with open(fname, 'wb') as file_object:
                pickle.dump(res, file_object)
        except Exception:
            remove_garbage(fname)
            raise
        print("checked " + output_name, file=stderr)


def execute_all(extra_code):
    """Calls execution_thread() for every possible combination in OPTIONS.

    Args:
        extra_code(str): extra code to be appended to RESULTS_FOLD.

    """
    for combination in itertools.product(*OPTIONS.values()):
        execution_thread(combination, extra_code)


def main():
    """main function"""
    from sys import argv
    if len(argv) > 1 and argv[1] == 'execute':
        extra_code = ""
        if len(argv) > 2:
            extra_code = argv[2]
        execute_all(extra_code)
    else:
        output_name = 'out.log'
        if len(argv) != 1:
            output_name = argv[1]
        process_output(output_name)

if __name__ == "__main__":
    main()
