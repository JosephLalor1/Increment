"""Checks every level in assets/lessons.

A level is a header (@id, @track, @title, optional @speed) and up to 16 coding steps. There
is nothing to read: any other line outside a code block is a problem.

- Structure: each step has a "~" line (difficulty 1-3, then up to 4 known skill:weight
  pairs), starter code, an @expect block and a @solution; @input is optional. The last step
  is the hardest (no step's difficulty is above the last one's). ASCII only; code lines fit
  the editor (66 columns).
- No words in code: starters and solutions have no comments, except the gap marker "// ?" in
  a starter, which marks where a line is missing; solutions must fill every gap.
- Running: the starter must compile but not already print the target, and the solution must
  compile without warnings and print it exactly (compared as the app does: trailing spaces
  and blank lines ignored). "@step broken" marks a step whose starter deliberately doesn't
  compile (a bug to fix); then it must fail to compile.
- The second run: the app refuses solutions that only print the answer by running them again
  with the step's given values changed (src/step_check.h). tools/step_variant.c builds that
  run here with the app's own code, and the changed solution must compile without warnings,
  finish normally and print the same thing twice. If it can't (a size that must match an
  initialiser list, say), mark the step "@step fixed" to switch its second run off.

usage: python -I tools/check_lessons.py [lesson.txt ...]
"""
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
LESSONS = os.path.join(ROOT, 'assets', 'lessons')
BIN = os.path.join(os.environ.get('RAYLIB_DIR', r'C:\raylib'), 'w64devkit', 'bin')
MAX_CODE_COLS = 66  # Consolas at the "Large" text size fits about this many columns
MAX_STEPS = 16
GAP_MARKER = '// ?'
# The skills a step's "~" line may name, and how many (src/skills.c).
SKILLS = ('program_structure', 'output', 'input', 'types', 'operators', 'conditions', 'loops', 'arrays', 'strings',
          'functions', 'scope', 'pointers', 'indirection', 'parameter_passing', 'pointer_arithmetic', 'dynamic_memory',
          'memory_safety', 'function_pointers', 'cpp_basics', 'references', 'classes', 'constructors', 'encapsulation',
          'copying', 'data_structures', 'tracing', 'debugging', 'problem_solving')
MAX_STEP_SKILLS = 4
# The skill names an "@speed" line may use (src/speed_round.c generates their questions).
SPEED_SKILLS = ('variables', 'arithmetic', 'conditions', 'loops', 'arrays', 'strings', 'functions',
                'pointers', 'pointer_to_pointer', 'pass_by_pointer', 'pointer_arithmetic', 'malloc',
                'function_pointers', 'references', 'new_delete', 'objects', 'linked_list')
FIELDS = {'@input': 'input', '@expect': 'expected', '@solution': 'solution'}


def parse(path):
    """Returns (header, steps, problems). Each step is a dict with its line number, whether it's
    "broken", its "~" tokens and its code blocks."""
    header, steps, problems = {}, [], []
    target = None  # which field of the current step the next code block fills
    code = None    # lines of the code block being read, or None outside one
    with open(path, encoding='utf-8') as f:
        lines = f.read().split('\n')
    for number, raw in enumerate(lines, 1):
        line = raw.rstrip('\r')
        if code is not None:
            if line.startswith('```'):
                if not steps:
                    problems.append(f'line {number}: code outside a step')
                else:
                    steps[-1][target or 'starter'] = '\n'.join(code)
                code, target = None, None
            else:
                code.append(line.replace('\t', '    '))
            continue
        if line.startswith('```'):
            code = []
        elif not line.strip():
            pass
        elif line.split(' ')[0] in ('@id', '@track', '@title', '@speed'):
            key, _, value = line.partition(' ')
            header[key[1:]] = value.strip()
        elif line == '@step' or (line.startswith('@step ') and set(line.split()[1:]) <= {'broken', 'fixed'}):
            steps.append({'line': number, 'broken': 'broken' in line.split(), 'fixed': 'fixed' in line.split(),
                          'skills': None,
                          'starter': None, 'input': None, 'expected': None, 'solution': None})
            target = None
        elif steps and line.startswith('~ '):
            steps[-1]['skills'] = line[2:].split()
        elif steps and line in FIELDS:
            target = FIELDS[line]
        else:
            problems.append(f'line {number}: levels have nothing to read, but this line is text: {line[:60]!r}')
    if code is not None:
        problems.append('a code block is never closed')
    return header, steps, problems


def normalize(text):
    """Output as the app compares it: no '\\r', no trailing spaces, no trailing blank lines."""
    lines = [l.expandtabs(8).rstrip() for l in (text or '').replace('\r', '').split('\n')]
    while lines and not lines[-1]:
        lines.pop()
    return lines


def build_variant_tool(workdir):
    """Compiles tools/step_variant.c; returns its path, or None with a problem printed."""
    exe = os.path.join(workdir, 'step_variant.exe')
    sources = [os.path.join(ROOT, 'tools', 'step_variant.c'), os.path.join(ROOT, 'src', 'step_check.c'),
               os.path.join(ROOT, 'src', 'code_skills.c')]
    r = subprocess.run([os.path.join(BIN, 'gcc.exe'), '-std=c99', '-Wall', *sources, '-o', exe],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print('could not build tools/step_variant.c:\n' + r.stderr)
        return None
    return exe


def check_variation(step, where, cpp, workdir, tag, tool):
    """Problems with the step's second run. Returns (problems, whether the step has one)."""
    paths = {}
    for field in ('starter', 'solution', 'input'):
        if step[field] is None:
            continue
        paths[field] = os.path.join(workdir, f'{tag}_{field}.txt')
        with open(paths[field], 'w', encoding='utf-8', newline='') as f:
            f.write(step[field])
    prefix = os.path.join(workdir, tag + '_varied')
    r = subprocess.run([tool, 'cpp' if cpp else 'c', paths['starter'], paths['solution'], paths.get('input', '-'),
                        prefix], capture_output=True, text=True)
    lines = r.stdout.strip().split('\n')
    problems = [f'{where}: the solution fails its own check ({line})' for line in lines[1:]]
    changes, _, summary = lines[0].partition(' ')
    if step['fixed'] or changes == '0':
        return problems, False
    code = open(prefix + '.code', encoding='utf-8').read()
    stdin = open(prefix + '.in', encoding='utf-8').read() + '\n' if step['input'] is not None else '\n'
    what = f'{where}: second run ({summary or "changed input"})'
    advice = '; fix the step or mark it "@step fixed"'
    ok, err, first = compile_and_run(code, cpp, workdir, tag + '_varied', stdin)
    if not ok or err.strip():
        return problems + [f'{what}: the changed solution does not compile cleanly{advice}:\n{err}'], True
    _, _, second = compile_and_run(code, cpp, workdir, tag + '_varied', stdin)
    if first == '<timed out>' or first != second:
        problems.append(f'{what}: the changed solution times out or prints different things each run{advice}')
    elif normalize(first) == normalize(step['expected']):
        return problems, False  # the change makes no difference: the app skips the second run too
    return problems, True


def compile_and_run(code, cpp, workdir, name, stdin):
    """Returns (compiled, compiler messages, output or None)."""
    src = os.path.join(workdir, name + ('.cpp' if cpp else '.c'))
    exe = os.path.join(workdir, name + '.exe')
    with open(src, 'w', encoding='utf-8') as f:
        f.write(code + '\n')
    cc = os.path.join(BIN, 'g++.exe' if cpp else 'gcc.exe')
    std = '-std=c++17' if cpp else '-std=c11'
    r = subprocess.run([cc, std, '-Wall', '-Wno-unused-variable', '-Wno-unused-but-set-variable',
                        src, '-o', exe], capture_output=True, text=True)
    if r.returncode != 0:
        return False, r.stderr, None
    try:
        p = subprocess.run([exe], input=stdin, capture_output=True, text=True, timeout=5)
        return True, r.stderr, p.stdout
    except subprocess.TimeoutExpired:
        return True, r.stderr, '<timed out>'


def check_skills(tokens, where):
    """Problems with a step's "~ <difficulty> skill:weight ..." line (tokens split on spaces)."""
    if not tokens:
        return [f'{where}: no "~" line ("~ <difficulty 1-3> [skill:weight ...]")']
    problems = []
    if tokens[0] not in ('1', '2', '3'):
        problems.append(f'{where}: the "~" line must start with a difficulty from 1 to 3')
    if len(tokens[1:]) > MAX_STEP_SKILLS:
        problems.append(f'{where}: more than {MAX_STEP_SKILLS} skills')
    for token in tokens[1:]:
        name, _, weight = token.partition(':')
        if name not in SKILLS:
            problems.append(f'{where}: unknown skill {name!r}')
        try:
            if not 0 < float(weight) <= 1:
                problems.append(f'{where}: weight of {name} must be over 0 and at most 1')
        except ValueError:
            problems.append(f'{where}: skill {token!r} needs a weight, as in {name}:0.5')
    return problems


def comment_problems(code, where, allow_gap):
    """Code may hold no words for the learner to read: no comments, bar the gap marker."""
    problems = []
    for line in code.split('\n'):
        stripped = line.strip()
        if allow_gap and stripped == GAP_MARKER:
            continue
        # Comments outside string literals (a "//" inside quotes is output, not a comment).
        outside = re.sub(r'"(\\.|[^"\\])*"|\'(\\.|[^\'\\])*\'', '""', line)
        if '//' in outside or '/*' in outside:
            problems.append(f'{where}: comment in code (only "{GAP_MARKER}" is allowed, on its own): {stripped!r}')
    return problems


def check_step(step, index, cpp, workdir, tag, tool):
    """Returns (problems, whether the step has a second run)."""
    where = f'step {index + 1} (line {step["line"]})'
    problems = check_skills(step['skills'], where)
    for field in ('starter', 'expected', 'solution'):
        if step[field] is None:
            problems.append(f'{where}: no {"starter code" if field == "starter" else "@" + field[:6]} block')
    if problems:
        return problems, False
    for field in ('starter', 'solution', 'input', 'expected'):
        text = step[field] or ''
        for n, line in enumerate(text.split('\n'), 1):
            if len(line) > MAX_CODE_COLS:
                problems.append(f'{where}: {field} line {n} is {len(line)} columns (max {MAX_CODE_COLS})')
            if any(ord(c) > 126 or (ord(c) < 32 and c != '\t') for c in line):
                problems.append(f'{where}: {field} line {n} is not plain ASCII')
    problems += comment_problems(step['starter'], where + ' starter', True)
    problems += comment_problems(step['solution'], where + ' solution', False)

    stdin = (step['input'] or '') + '\n'
    ok, err, out = compile_and_run(step['starter'], cpp, workdir, tag + '_starter', stdin)
    if step['broken'] and ok:
        problems.append(f'{where}: "@step broken", but the starter compiles')
    elif not step['broken'] and not ok:
        problems.append(f'{where}: starter does not compile (mark the step "@step broken" if that is the bug):\n{err}')
    elif ok and out is not None and normalize(out) == normalize(step['expected']):
        problems.append(f'{where}: the starter already prints the target')
    ok, err, out = compile_and_run(step['solution'], cpp, workdir, tag + '_solution', stdin)
    if not ok:
        problems.append(f'{where}: solution does not compile:\n{err}')
    elif err.strip():
        problems.append(f'{where}: solution has warnings:\n{err}')
    elif normalize(out) != normalize(step['expected']):
        problems.append(f'{where}: solution output differs.\n  expected: {normalize(step["expected"])}\n'
                        f'  actual:   {normalize(out)}')
    varied_problems, varied = check_variation(step, where, cpp, workdir, tag, tool)
    return problems + varied_problems, varied


def check(path, workdir, tool):
    """Returns (problems, steps, steps with a second run)."""
    name = os.path.splitext(os.path.basename(path))[0]
    header, steps, problems = parse(path)
    for key in ('id', 'track', 'title'):
        if not header.get(key):
            problems.append(f'no @{key}')
    if header.get('track') not in (None, 'C', 'C++'):
        problems.append(f'@track must be C or C++, not {header["track"]!r}')
    for skill in header.get('speed', '').split():
        if skill not in SPEED_SKILLS:
            problems.append(f'@speed names an unknown skill {skill!r}')
    if len(steps) > MAX_STEPS:
        problems.append(f'{len(steps)} steps (max {MAX_STEPS})')
    cpp = header.get('track') == 'C++'
    second_runs = 0
    for i, step in enumerate(steps):
        step_problems, varied = check_step(step, i, cpp, workdir, f'{name}_{i + 1}', tool)
        problems += step_problems
        second_runs += varied
    difficulties = [int(s['skills'][0]) for s in steps if s['skills'] and s['skills'][0] in ('1', '2', '3')]
    if difficulties and max(difficulties) > difficulties[-1]:
        problems.append('the last step should be the hardest: an earlier step has a higher difficulty')
    return problems, len(steps), second_runs


def main():
    paths = sys.argv[1:] or sorted(
        os.path.join(LESSONS, f) for f in os.listdir(LESSONS) if f.endswith('.txt'))
    total = steps = 0
    with tempfile.TemporaryDirectory() as work:
        tool = build_variant_tool(work)
        if not tool:
            return 1
        for p in paths:
            problems, count, second_runs = check(p, work, tool)
            steps += count
            status = 'not written yet' if count == 0 and not problems else 'ok' if not problems \
                else f'{len(problems)} problem(s)'
            print(f'{os.path.basename(p)}: {count} step(s), {second_runs} with a second run: {status}')
            for pr in problems:
                print('  - ' + pr.replace('\n', '\n    '))
            total += len(problems)
    print(f'\n{len(paths)} level(s), {steps} step(s), {total} problem(s)')
    return 1 if total else 0


sys.exit(main())
