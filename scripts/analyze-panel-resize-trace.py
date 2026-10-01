#!/usr/bin/env python3
"""Summarize optional resize frame traces; timings are diagnostic only."""
import argparse
import json
from pathlib import Path


def statistics(values):
    values = sorted(values)
    if not values:
        return {'count': 0}
    return {'count': len(values), 'p50': values[(len(values) - 1) // 2],
            'p95': values[int((len(values) - 1) * .95)],
            'p99': values[int((len(values) - 1) * .99)], 'max': values[-1]}


def analyze(frames):
    groups = {name: [] for name in ('all', 'columnChange', 'added',
                                   'destroyedOnly', 'churn', 'stable')}
    previous = None
    for frame in frames:
        if previous is None:
            previous = frame
            continue
        row = dict(frame)
        row['columnChange'] = any(row[side + 'Columns'] != previous[side + 'Columns']
                                  for side in ('left', 'right'))
        for counter in ('Added', 'Destroyed'):
            row[counter.lower()] = sum(row[side + counter + 'Total']
                                       - previous[side + counter + 'Total']
                                       for side in ('left', 'right'))
            assert row[counter.lower()] >= 0, 'Lifecycle counter decreased'
        previous = frame
        if row['afterAnimatingMs'] < 500:
            continue
        groups['all'].append(row)
        if row['columnChange']:
            groups['columnChange'].append(row)
        if row['added']:
            groups['added'].append(row)
        if row['destroyed'] and not row['added']:
            groups['destroyedOnly'].append(row)
        if row['added'] or row['destroyed']:
            groups['churn'].append(row)
        elif not row['columnChange']:
            groups['stable'].append(row)
    summary = {}
    for name, rows in groups.items():
        summary[name] = {'rows': len(rows), 'added': sum(r['added'] for r in rows),
                         'destroyed': sum(r['destroyed'] for r in rows),
                         'longIntervals': sum(r['intervalMs'] > 13.889 for r in rows)}
        for field in ('intervalMs', 'preSyncElapsedMs', 'syncMs', 'renderMs'):
            summary[name][field] = statistics([r[field] for r in rows])
    summary['largestIntervals'] = sorted(groups['all'], key=lambda r: r['intervalMs'],
                                         reverse=True)[:10]
    summary['traceIdsUnique'] = len({f['id'] for f in frames}) == len(frames)
    return summary


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    args = parser.parse_args()
    results = []
    for path in sorted(args.directory.glob('*.json')):
        report = json.loads(path.read_text())
        for result in report['results']:
            if 'frameTrace' in result:
                results.append({'file': path.name, 'viewMode': report['viewMode'],
                                'rep': result['rep'], 'summary': analyze(result['frameTrace'])})
    print(json.dumps(results, indent=2))
