#!/usr/bin/env python3
# Tries m2c on #pragma GLOBAL_ASM functions and keeps the ones that match:
#   python auto_decomp.py [--limit N] [--max-size N] [--resume] [--apply] [func_... ...]
# --apply leaves m2c's output in place even when it doesn't match, to finish by hand.
# For each candidate (by default find_fresh_candidates.py's list, smallest first,
# in files the linker script builds), the pragma is replaced by m2c's output with
# the file's own declarations as context, and `make` checks the whole ROM against
# the original (build/conker.us.bin: OK). A function that doesn't compile or
# doesn't match goes back to its pragma. Results are appended to
# build/tmp/auto_decomp.log (ok / build error / N words differ). Run from
# conker/conker, in Windows (make runs through WSL).
import os, re, subprocess, sys

args = sys.argv[1:]
unknown = [a for a in args if a.startswith('-') and a not in ('--limit', '--max-size', '--resume', '--apply')]
if unknown:
    # Any other option (even --help) would otherwise start a whole batch: print the
    # usage in the comment above instead.
    usage = [line[2:] for line in open(__file__).read().splitlines()[1:11]]
    sys.exit('\n'.join(usage) + f'\nunknown option: {" ".join(unknown)}')
limit = int(args[args.index('--limit') + 1]) if '--limit' in args else None
max_size = int(args[args.index('--max-size') + 1]) if '--max-size' in args else 10 ** 9
flag_values = {args[i + 1] for i, a in enumerate(args[:-1]) if a in ('--limit', '--max-size')}
names = [a for a in args if not a.startswith('--') and a not in flag_values]
LOG = 'build/tmp/auto_decomp.log'

linked = set(re.findall(r'build/(src/[A-Za-z0-9_/]*\.c)\.o', open('conker.ld').read()))


def candidates():
    out = subprocess.run([sys.executable, 'find_fresh_candidates.py'], capture_output=True, text=True).stdout
    for line in out.splitlines():
        size, path, lineno, rel = line.split()
        path = path.replace('\\', '/')
        if path in linked and int(size) <= max_size:
            yield path, rel


def wsl(cmd):
    return subprocess.run(['wsl', 'bash', '-c', cmd], capture_output=True, text=True)


def context_stub(path):
    # The file's #includes and top-level declarations, without function bodies:
    # m2c's parser doesn't take old-style (K&R) definitions.
    depth = 0
    kept = []
    for line in open(path, encoding='utf-8', errors='replace').read().split('\n'):
        code = re.sub(r'//.*', '', line)
        if depth == 0 and (code.startswith('#include') or code.startswith('#define')
                           or (code.rstrip().endswith(';') and not code[:1].isspace())):
            kept.append(line)
        depth += code.count('{') - code.count('}')
    open('build/tmp/ctx_stub.c', 'w', newline='\n').write('\n'.join(kept) + '\n')


def expand_fields(text):
    # m2c's --valid-syntax M2C_FIELD(expr, type_ptr, offset), written out the way
    # the rest of the source does it (innermost first).
    while (i := text.rfind('M2C_FIELD(')) >= 0:
        args, depth, start, j = [], 0, i + len('M2C_FIELD('), i + len('M2C_FIELD(')
        while True:
            c = text[j]
            if c == '(':
                depth += 1
            elif c == ')' and depth == 0:
                args.append(text[start:j].strip())
                break
            elif c == ')':
                depth -= 1
            elif c == ',' and depth == 0:
                args.append(text[start:j].strip())
                start = j + 1
            j += 1
        expr, type_ptr, offset = args
        text = text[:i] + f'(*({type_ptr})((char *)({expr}) + {offset}))' + text[j + 1:]
    return text


def m2c(path, rel):
    context_stub(path)
    wsl(f'gcc -E -P -Iinclude -Iinclude/2.0L -Iinclude/2.0L/PR -Isrc -I{os.path.dirname(path)} -D_LANGUAGE_C '
        f'-ffreestanding -DF3DEX_GBI_2 -DM2CTX build/tmp/ctx_stub.c -o build/tmp/ctx.c')
    r = wsl(f'python3 ../../tools/m2c/m2c.py -t mips-ido-c --valid-syntax --context build/tmp/ctx.c '
            f'asm/nonmatchings/{rel}')
    unhandled = ('M2C_ERROR', 'M2C_TRAP_IF', 'M2C_BREAK', 'M2C_BITWISE', 'M2C_LWL', 'M2C_FIRST3BYTES',
                 'M2C_UNALIGNED32', 'GLUE_F64', 'MULT_HI', 'MULTU_HI', 'DMULT_HI', 'DMULTU_HI')
    if r.returncode != 0 or any(u in r.stdout for u in unhandled):
        return None
    source = open(path, encoding='utf-8', errors='replace').read()
    text = expand_fields(r.stdout)
    for unk, t in (('M2C_UNK8', 's8'), ('M2C_UNK16', 's16'), ('M2C_UNK32', 's32'), ('M2C_UNK64', 's64'),
                   ('M2C_UNK', 's32')):
        text = re.sub(r'\b' + unk + r'\b', t, text)
    lines = []
    for line in text.strip('\n').split('\n'):
        # m2c's prototypes for callees it found no declaration of: kept unless the
        # file declares the function itself; an unknown return type becomes void.
        m = re.match(r'^(.*?)\b(func_[0-9A-F]{8})\(.*\);\s*/\* (extern|static) \*/$', line)
        if m:
            if re.search(r'\b' + m.group(2) + r'\s*\(', source):
                continue
            line = re.sub(r'^\?\s*', 'void ', line)
        # m2c writes ? for types it couldn't work out (not the ?: operator).
        line = re.sub(r'(^|[(,]|\bextern)(\s*)\?(?=[\s*),;])', r'\1\2s32', line)
        lines.append(line)
    return '\n'.join(lines)


def build_ok():
    out = wsl('make -j8 2>&1').stdout
    lines = out.splitlines()
    # IDO's cfe prints "cfe: ... Error<file>, " and its message on the lines after.
    first = next((' '.join(lines[i:i + 3]) for i, l in enumerate(lines)
                  if ('cfe:' in l and 'Error' in l) or 'undefined reference' in l), '')
    return 'conker.us.bin: OK' in out, ('Error' in out and 'FAILED' not in out) and (first.strip() or 'error')


def differing_words(func):
    out = subprocess.run([sys.executable, 'find_code_diff.py'], capture_output=True, text=True).stdout
    total = sum(int(x) for x in re.findall(r'functions with non-reloc diffs: (\d+)', out))
    m = re.search(r'^\s+' + func + r' (\d+)$', out, re.M)
    return (int(m.group(1)) if m else 0), total


todo = list(candidates())
if names:
    todo = [(p, r) for p, r in todo if os.path.basename(r)[:-2] in names]
    found = {os.path.basename(r)[:-2] for _, r in todo}
    if set(names) - found:
        sys.exit(f'not candidates (no fresh #pragma GLOBAL_ASM in a linked file): {" ".join(sorted(set(names) - found))}')
elif '--resume' in args and os.path.exists(LOG):
    # Skip the functions an earlier run already tried.
    tried = {line.split('\t')[0] for line in open(LOG)}
    todo = [(p, r) for p, r in todo if os.path.basename(r)[:-2] not in tried]
if limit:
    todo = todo[:limit]
log = open(LOG, 'a')
kept = 0
for path, rel in todo:
    func = os.path.basename(rel)[:-2]
    pragma = f'#pragma GLOBAL_ASM("asm/nonmatchings/{rel}")'
    original = open(path, 'rb').read()
    text = original.decode()
    if text.count(pragma) != 1:
        continue
    code = m2c(path, rel)
    if code is None:
        result = 'm2c output not compilable'
    else:
        open(path, 'wb').write(text.replace(pragma, code).encode())
        ok, error = build_ok()
        if ok:
            result = 'ok'
            kept += 1
        elif error:
            result = f'build error: {error[:200]}'
        else:
            words, total = differing_words(func)
            result = f'{words} words differ ({total} functions differ)'
        if not ok and '--apply' not in args:
            open(path, 'wb').write(original)
    print(f'{func} ({path}): {result}', flush=True)
    log.write(f'{func}\t{path}\t{result}\n')
    log.flush()
# Leave the build in the committed state.
if '--apply' not in args:
    wsl('make -j8 >/dev/null 2>&1')
print(f'{kept} of {len(todo)} kept')
