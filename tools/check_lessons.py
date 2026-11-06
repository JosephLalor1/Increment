"""Checks every lesson in assets/lessons.

- Structure: title/track present, every question has 2+ choices, exactly one '*' answer,
  and an explanation; block/question limits; ASCII-only text; code lines short enough
  to fit the code box.
- Code: every code block containing main() is compiled (gcc -std=c11 for the C track,
  g++ -std=c++17 for C++) and run. If the block is followed by a paragraph "Output:" and
  then a code block, the program's stdout must match that block exactly.
- Exercises: the starter code must compile but not already produce the expected output,
  and the @solution must compile cleanly and produce it exactly (compared as the app
  does: trailing spaces and blank lines ignored).
- Optional exercise @hint lines are kept separate from descriptions and checked for
  empty clues and the same eight-hint limit as the app.

Markers inside a code block (as comments) change what's expected:
  // does not compile   -> compilation must fail
  // undefined behaviour -> compiled but not run
  // stdin: 3 7 4 9     -> whitespace-separated input fed to the program
Programs that read input with no stdin marker get an empty stdin.

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
MAX_BLOCKS, MAX_QUESTIONS, MAX_CHOICES = 256, 16, 6
MAX_EXERCISE_HINTS = 8


def parse(path):
    """Returns (meta, blocks, questions, exercises). blocks: list of (type, text)."""
    meta, blocks, questions, exercises = {}, [], [], []
    para, para_type, code, in_code, in_quiz = [], None, [], False, False
    is_memory = False  # the open fence is ```memory: a pointer diagram script, not code
    in_ex, target = False, None  # in an exercise; which field the next code block fills

    def flush():
        nonlocal para, para_type
        if para:
            blocks.append((para_type, ' '.join(para)))
        para, para_type = [], None

    for n, raw in enumerate(open(path, encoding='utf-8').read().split('\n'), 1):
        line = raw.rstrip('\r')
        if in_code:
            if line.startswith('```'):
                text = '\n'.join(code)
                if in_ex:
                    exercises[-1][target or 'starter'] = text
                    target = None
                elif in_quiz and questions:
                    questions[-1]['code'] = text
                else:
                    blocks.append(('memory' if is_memory else 'code', text))
                in_code, code = False, []
            else:
                code.append(line.replace('\t', '    '))
            continue
        if line.startswith('```'):
            flush()
            in_code = True
            is_memory = line.startswith('```memory') and not in_ex and not in_quiz
            continue
        m = re.match(r'@(title|id|track|kind) (.*)', line)
        if m:
            meta[m.group(1)] = m.group(2).strip()
            continue
        if line.startswith('@quiz'):
            flush()
            in_quiz, in_ex = True, False
            continue
        if line.startswith('@exercise'):
            flush()
            in_quiz, in_ex, target = False, True, None
            exercises.append({'title': line[9:].strip(), 'desc': '', 'starter': None, 'input': None,
                              'expected': None, 'solution': None, 'hints': [], 'line': n})
            continue
        if in_ex:
            if re.match(r'@hint(?:\s|$)', line):
                exercises[-1]['hints'].append(line[5:].strip())
                continue
            m = re.match(r'@(input|expect|solution)\b', line)
            if m:
                target = {'input': 'input', 'expect': 'expected', 'solution': 'solution'}[m.group(1)]
            elif line.strip():
                exercises[-1]['desc'] += ' ' + line
            continue
        if in_quiz:
            if line.startswith('? '):
                questions.append({'prompt': line[2:], 'choices': [], 'correct': [], 'explain': None, 'code': None, 'line': n})
            elif line[:2] in ('- ', '* ') and questions:
                if line[0] == '*':
                    questions[-1]['correct'].append(len(questions[-1]['choices']))
                questions[-1]['choices'].append(line[2:])
            elif line.startswith('! ') and questions:
                questions[-1]['explain'] = line[2:]
            elif line.strip():
                print(f'  {os.path.basename(path)}:{n}: unrecognised quiz line: {line!r}')
            continue
        if not line.strip():
            flush()
        elif line.startswith('# '):
            flush()
            blocks.append(('heading', line[2:]))
        elif line.startswith('- '):
            flush()
            blocks.append(('bullet', line[2:]))
        elif line.startswith('  ') and blocks and blocks[-1][0] == 'bullet' and not para:
            blocks[-1] = ('bullet', blocks[-1][1] + ' ' + line.strip())
        else:
            t = 'note' if line.startswith('> ') else 'text'
            if para and para_type != t:
                flush()
            para_type = t
            para.append(line[2:] if t == 'note' else line)
    flush()
    return meta, blocks, questions, exercises


def normalize(text):
    """Output as the app compares it: no '\\r', no trailing spaces, no trailing blank lines."""
    lines = [l.expandtabs(8).rstrip() for l in (text or '').replace('\r', '').split('\n')]
    while lines and not lines[-1]:
        lines.pop()
    return lines


def compile_and_run(code, cpp, workdir, name, stdin=None):
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
    if stdin is None:
        m = re.search(r'//\s*stdin:\s*(.*)', code)
        stdin = '\n'.join(m.group(1).split()) + '\n' if m else ''
    if 'undefined behaviour' in code:
        return True, r.stderr, None
    try:
        p = subprocess.run([exe], input=stdin, capture_output=True, text=True, timeout=5)
        return True, r.stderr, p.stdout
    except subprocess.TimeoutExpired:
        return True, r.stderr, '<timed out>'


def check(path, workdir):
    name = os.path.basename(path)
    problems = []
    meta, blocks, questions, exercises = parse(path)
    cpp = meta.get('track') in ('C++', 'cpp')

    # Exercises: the starter compiles but doesn't already pass; the solution passes.
    for ei, ex in enumerate(exercises):
        if len(ex['hints']) > MAX_EXERCISE_HINTS:
            problems.append(f'exercise {ei + 1}: too many hints (max {MAX_EXERCISE_HINTS})')
        if any(not hint for hint in ex['hints']):
            problems.append(f'exercise {ei + 1}: empty hint')
        where = f'exercise "{ex["title"]}" (line {ex["line"]})'
        if not ex['title']:
            problems.append(f'{where}: no title')
        if not ex['desc'].strip():
            problems.append(f'{where}: no description')
        if not ex['starter']:
            problems.append(f'{where}: no starter code')
            continue
        if ex['expected'] is None:
            problems.append(f'{where}: no @expect block')
            continue
        tag = f'{os.path.splitext(name)[0]}_ex{ei}'
        stdin = (ex['input'] or '') + '\n'
        ok, err, out = compile_and_run(ex['starter'], cpp, workdir, tag + '_starter', stdin)
        if not ok:
            problems.append(f'{where}: starter code does not compile:\n{err}')
        elif out is not None and normalize(out) == normalize(ex['expected']):
            problems.append(f'{where}: the starter code already produces the expected output')
        if not ex['solution']:
            problems.append(f'{where}: no @solution (needed to prove the exercise can be solved)')
            continue
        ok, err, out = compile_and_run(ex['solution'], cpp, workdir, tag + '_solution', stdin)
        if not ok:
            problems.append(f'{where}: solution does not compile:\n{err}')
        elif err.strip():
            problems.append(f'{where}: solution has warnings:\n{err}')
        elif normalize(out) != normalize(ex['expected']):
            problems.append(f'{where}: solution output differs.\n  expected: {normalize(ex["expected"])}\n'
                            f'  actual:   {normalize(out)}')

    for key in ('id', 'title', 'track'):
        if key not in meta:
            problems.append(f'missing @{key}')
    if len(blocks) > MAX_BLOCKS:
        problems.append(f'{len(blocks)} blocks (max {MAX_BLOCKS})')
    if len(questions) > MAX_QUESTIONS:
        problems.append(f'{len(questions)} questions (max {MAX_QUESTIONS})')
    if not questions:
        problems.append('no quiz questions')

    text = open(path, encoding='utf-8').read()
    for n, line in enumerate(text.split('\n'), 1):
        bad = [c for c in line if ord(c) > 126]
        if bad:
            problems.append(f'line {n}: non-ASCII {"".join(sorted(set(bad)))!r}')

    for q in questions:
        where = f'question at line {q["line"]}'
        if len(q['choices']) < 2 or len(q['choices']) > MAX_CHOICES:
            problems.append(f'{where}: {len(q["choices"])} choices')
        if len(q['correct']) != 1:
            problems.append(f'{where}: {len(q["correct"])} correct answers')
        if not q['explain']:
            problems.append(f'{where}: no explanation')
        if len(set(q['choices'])) != len(q['choices']):
            problems.append(f'{where}: duplicate choices')

    codes = [b[1] for b in blocks if b[0] == 'code'] + [q['code'] for q in questions if q['code']]
    # In a pointer diagram only the "> code" lines are shown as code. (Whether the whole
    # script is valid is checked by tests\test_memory_model.c, which uses the app's parser.)
    codes += ['\n'.join(re.sub(r'^> ?', '', l.strip()) for l in b[1].split('\n') if l.strip().startswith('>'))
              for b in blocks if b[0] == 'memory']
    for c in codes:
        for line in c.split('\n'):
            if len(line) > MAX_CODE_COLS:
                problems.append(f'code line too long ({len(line)}): {line.strip()[:50]}')

    runs = 0
    for i, (kind, body) in enumerate(blocks):
        if kind != 'code' or not re.search(r'\bmain\s*\(', body):
            continue
        runs += 1
        tag = f'{os.path.splitext(name)[0]}_{i}'
        expect_fail = 'does not compile' in body
        ok, err, out = compile_and_run(body, cpp, workdir, tag)
        first = body.strip().split('\n')[0][:40]
        if expect_fail:
            if ok:
                problems.append(f'program "{first}" was marked "does not compile" but compiled')
            continue
        if not ok:
            problems.append(f'program "{first}" failed to compile:\n{err}')
            continue
        if err.strip() and 'undefined behaviour' not in body:  # UB demos may warn on purpose
            problems.append(f'program "{first}" has warnings:\n{err}')
        if out == '<timed out>' and 'while (1)' not in body and 'while(1)' not in body:
            problems.append(f'program "{first}" timed out')
        # Expected output: a following "Output:" paragraph then a code block.
        if (out is not None and i + 2 < len(blocks) and blocks[i + 1] == ('text', 'Output:')
                and blocks[i + 2][0] == 'code'):
            want = [l.rstrip() for l in blocks[i + 2][1].strip('\n').split('\n')]
            # Tabs are compared as a terminal shows them (8-column stops).
            got = [l.expandtabs(8).rstrip() for l in out.replace('\r', '').strip('\n').split('\n')]
            if want != got:
                problems.append(f'program "{first}" output differs.\n  expected: {want}\n  actual:   {got}')
    # "What does this print?" questions: run the snippet and compare with the marked answer.
    for qi, q in enumerate(questions):
        if not q['code'] or not q['prompt'].lower().startswith('what does this print') or len(q['correct']) != 1:
            continue
        body = q['code']
        if cpp:
            head = '#include <iostream>\n#include <cstring>\nusing namespace std;\n'
        else:
            head = '#include <stdio.h>\n#include <string.h>\n#include <stdlib.h>\n'
        prog = head + (body if re.search(r'\bmain\s*\(', body) else 'int main() {\n' + body + '\n}')
        ok, err, out = compile_and_run(prog, cpp, workdir, f'{os.path.splitext(name)[0]}_q{qi}')
        runs += 1
        want = q['choices'][q['correct'][0]].strip()
        if not ok:
            problems.append(f'quiz snippet at line {q["line"]} failed to compile:\n{err}')
        elif out is not None and out.strip() != want:
            problems.append(f'quiz at line {q["line"]}: answer is {want!r} but the code prints {out.strip()!r}')
    return problems, runs, len(questions), len(exercises)


def main():
    paths = sys.argv[1:] or sorted(
        os.path.join(LESSONS, f) for f in os.listdir(LESSONS) if f.endswith('.txt'))
    total = 0
    with tempfile.TemporaryDirectory() as work:
        for p in paths:
            problems, runs, nq, nex = check(p, work)
            status = 'ok' if not problems else f'{len(problems)} problem(s)'
            print(f'{os.path.basename(p)}: {runs} program(s), {nq} question(s), {nex} exercise(s): {status}')
            for pr in problems:
                print('  - ' + pr.replace('\n', '\n    '))
            total += len(problems)
    print(f'\n{len(paths)} lesson(s), {total} problem(s)')
    return 1 if total else 0


sys.exit(main())
