#!/usr/bin/env python3
"""Run the real file-panel resize benchmark in an isolated configuration."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--platform', choices=('wayland', 'xcb', 'offscreen'), default='wayland')
parser.add_argument('--modes', type=int, choices=(0, 1, 2), nargs='+', default=[0, 1, 2])
parser.add_argument('--dividers', choices=('panels', 'sidebar', 'preview', 'middle-left', 'middle-right'), nargs='+', default=['panels'])
parser.add_argument('--runs', type=int, default=3)
parser.add_argument('--right-mode', type=int, choices=(0, 1, 2))
parser.add_argument('--capture', action='store_true', help='capture the active resize scene; exclude these runs from timing comparisons')
parser.add_argument('--trace', action='store_true', help='record frame stages, column changes, and content-child lifecycle; diagnostic timing only')
parser.add_argument('--output-dir', type=Path, required=True)
args = parser.parse_args()
if args.runs < 1 or args.runs > 10:
    parser.error("--runs must be between 1 and 10")
root = Path(__file__).resolve().parents[1]
args.output_dir.mkdir(parents=True, exist_ok=True)
for divider in args.dividers:
    for mode in args.modes:
        for repetition in range(args.runs):
            name = f'{args.platform}-{divider}-{mode}-{repetition}'
            if args.right_mode is not None:
                name += f'-right-{args.right_mode}'
            output = (args.output_dir / f'{name}.json').resolve()
            output.unlink(missing_ok=True)
            with tempfile.TemporaryDirectory(prefix='fm-resize-config-') as config:
                env = os.environ.copy()
                env.update(QT_QPA_PLATFORM=args.platform, XDG_CONFIG_HOME=config)
                if args.platform == 'offscreen':
                    env['QSG_RHI_BACKEND'] = 'software'
                command = [str(root / 'build/fm'), '--navigation-gui-benchmark', '--resize',
                           '--view-mode', str(mode), '--divider', divider,
                           '--resize-runs', '1', '--resize-output', str(output)]
                if args.capture:
                    command.extend(['--resize-capture', str((args.output_dir / f'{name}.png').resolve())])
                if args.trace:
                    command.append('--resize-trace')
                if args.right_mode is not None:
                    command.extend(['--right-view-mode', str(args.right_mode)])
                with (args.output_dir / f'{name}.log').open('w') as log:
                    result = subprocess.run(command, cwd=root, env=env, stdout=log,
                                            stderr=log, timeout=40)
            if output.exists():
                report = json.loads(output.read_text())
                for row in report['results']:
                    print(name, row['rep'], 'accepted', row['accepted'],
                          'frame p95', round(row['frameIntervalMs'].get('p95', 0), 2),
                          'input p95', round(row['inputCallMs'].get('p95', 0), 2), flush=True)
            if result.returncode:
                raise SystemExit(result.returncode)
