#!/usr/bin/env python3
"""sample_busy.py SAMPLE_TXT [N]: per-thread busy samples from a macOS `sample` call graph.
A sample counts as busy unless its leaf frame is a blocking wait (mach_msg, psynch, cond/semaphore
waits, kevent, sleep). Prints the N busiest threads (default 25) and the total, as busy samples and as
% of one core (samples / thread samples * 100).
Limit: a vCPU thread inside hv_vcpu_run counts as busy even while the guest idles in the hypervisor;
use a periodic per-thread `ps -M` sample of the VM process for vCPU load."""
import re, sys
WAIT = re.compile(r'mach_msg2_trap|__psynch_|__ulock_wait|semaphore_\w*wait|__semwait_signal|kevent|__select|'
                  r'__workq_kernreturn|mach_wait_until|__sigsuspend|__wait4|\bpoll\b|__recvfrom|__recvmsg|__read_nocancel|^read\b|'
                  r'__accept|__pselect|__ppoll|__waitid')
LINE = re.compile(r'^(?P<ind>[ +!:|]*)(?P<n>\d+) (?P<fn>.*)$')
def parse(path):
    threads, cur, stack = [], None, []
    for raw in open(path, errors='replace'):
        m = re.match(r'^    (\d+) (Thread_\S+?)(?::\s*(.*)|\s+(.*))?$', raw.rstrip())
        if m:
            cur = {'name': (m.group(3) or m.group(4) or m.group(2)).strip(), 'total': int(m.group(1)), 'leaves': []}
            threads.append(cur); stack = []; continue
        if cur is None: continue
        if not raw.strip():
            cur = None; continue
        m = LINE.match(raw[4:].rstrip('\n'))
        if not m: continue
        node = [len(m.group('ind')), int(m.group('n')), m.group('fn'), False]
        while stack and stack[-1][0] >= node[0]: stack.pop()
        if stack: stack[-1][3] = True
        stack.append(node); cur['leaves'].append(node)
    out = []
    for t in threads:
        busy = sum(n for d, n, fn, has_child in t['leaves'] if not has_child and not WAIT.search(fn))
        out.append((busy, t['name'], t['total']))
    return out
if __name__ == '__main__':
    rows = parse(sys.argv[1]); top = int(sys.argv[2]) if len(sys.argv) > 2 else 25
    rows.sort(reverse=True)
    tot = max((t for _, _, t in rows), default=1)
    for busy, name, total in rows[:top]:
        print(f"{100.0 * busy / total:6.1f}%  {name}")
    print(f"total {100.0 * sum(b for b, _, _ in rows) / tot:.1f}% of one core over {len(rows)} threads")
