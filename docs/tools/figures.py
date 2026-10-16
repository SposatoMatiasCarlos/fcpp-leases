"""
Generates the conceptual figures of the documentation, in Italian and English:
    pages/{it,en}/figures/*.html

The module overview is a block diagram. The other figures are sequences of
snapshots of a small network: devices are circles placed in the plane, neighbours
are joined by a line, the number inside a device is what that device observes
(how long a neighbour has been silent, how many devices it hears, how long its copy
of the lease still lasts), and bold arrows are the messages that matter at that moment.
Colours come from CSS classes defined in fcpp-leases.css, so the figures follow the
light/dark theme.

Run it from the docs directory (build.sh does it).
"""

import math
import os

LANGS = ('it', 'en')
R = 15          # node radius


def esc(s):
    return s.replace('&', '&amp;').replace('<', '&lt;').replace('>', '&gt;')


def svg(width, height, label, body):
    return '<svg viewBox="0 0 %d %d" role="img" aria-label="%s">\n%s\n</svg>' % (width, height, esc(label), '\n'.join(body))


def legend(items):
    return '<div class="fl-legend">' + ''.join('<span><i class="lg %s"></i>%s</span>' % (c, esc(t)) for c, t in items) + '</div>'


def figure(svg_text, items=None):
    return '<div class="fl-figure">\n<div class="fl-figbox">\n%s\n</div>\n%s</div>\n' % (svg_text, legend(items) if items else '')


# ------------------------------------------------------------------ snapshots of a network

class Snapshot:
    """One picture of a small network, placed at (ox, oy)."""

    def __init__(self, ox, oy, pos, edges, title):
        self.ox, self.oy = ox, oy
        self.pos = pos                      # name -> (x, y) inside the panel
        self.out = []
        if title:
            self.text(self.width() / 2, -24, title, 't-panel', 'middle')
        for a, b in edges:
            self.line(a, b, 'e-link')

    def width(self):
        return max(x for x, _ in self.pos.values())

    def height(self):
        return max(y for _, y in self.pos.values())

    def xy(self, n):
        x, y = self.pos[n]
        return self.ox + x, self.oy + y

    def ends(self, a, b, gap=R + 3):
        (x0, y0), (x1, y1) = self.xy(a), self.xy(b)
        L = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / L, (y1 - y0) / L
        return x0 + ux * gap, y0 + uy * gap, x1 - ux * gap, y1 - uy * gap, ux, uy

    def line(self, a, b, cls):
        x0, y0, x1, y1, _, _ = self.ends(a, b, R)
        self.out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" class="%s"/>' % (x0, y0, x1, y1, cls))

    def arrow(self, a, b, cls, offset=0):
        """A message from a to b; offset shifts it sideways (two messages on the same link)."""
        x0, y0, x1, y1, ux, uy = self.ends(a, b)
        nx, ny = -uy * offset, ux * offset
        x0, y0, x1, y1 = x0 + nx, y0 + ny, x1 + nx, y1 + ny
        h = 7
        bx, by = x1 - ux * h, y1 - uy * h
        self.out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" class="a %s"/>' % (x0, y0, bx, by, cls))
        self.out.append('<polygon points="%.1f,%.1f %.1f,%.1f %.1f,%.1f" class="ah %s"/>'
                        % (x1, y1, bx - uy * h * .55, by + ux * h * .55, bx + uy * h * .55, by - ux * h * .55, cls))

    def lost(self, a, b):
        """A link on which messages are lost."""
        x0, y0, x1, y1, _, _ = self.ends(a, b, R)
        self.out.append('<line x1="%.1f" y1="%.1f" x2="%.1f" y2="%.1f" class="e-lost"/>' % (x0, y0, x1, y1))
        mx, my, c = (x0 + x1) / 2, (y0 + y1) / 2, 4
        self.out.append('<path d="M%.1f,%.1f L%.1f,%.1f M%.1f,%.1f L%.1f,%.1f" class="e-cross"/>' % (mx - c, my - c, mx + c, my + c, mx - c, my + c, mx + c, my - c))

    def node(self, n, cls, value='', label=None, label_side='nw'):
        x, y = self.xy(n)
        self.out.append('<circle cx="%.1f" cy="%.1f" r="%d" class="n %s"/>' % (x, y, R, cls))
        if value:
            self.out.append('<text x="%.1f" y="%.1f" text-anchor="middle" class="t-val %s">%s</text>' % (x, y + 4.5, cls, esc(value)))
        lab = n if label is None else label
        if lab:
            dx = {'nw': -R - 3, 'ne': R + 3, 'sw': -R - 3, 'se': R + 3, 's': 0, 'w': -R - 6}[label_side]
            dy = {'nw': -R + 2, 'ne': -R + 2, 'sw': R + 10, 'se': R + 10, 's': R + 15, 'w': 4}[label_side]
            anchor = 'end' if dx < 0 else 'start' if dx > 0 else 'middle'
            self.out.append('<text x="%.1f" y="%.1f" text-anchor="%s" class="t-name">%s</text>' % (x + dx, y + dy, anchor, esc(lab)))

    def text(self, x, y, s, cls, anchor='start'):
        self.out.append('<text x="%.1f" y="%.1f" text-anchor="%s" class="%s">%s</text>' % (self.ox + x, self.oy + y, anchor, cls, esc(s)))


def row_of(panels, gap=60, top=44, left=30):
    """Places snapshot builders side by side; returns (body, width, height)."""
    body, x, h = [], left, 0
    for build in panels:
        s = build(x, top)
        body += s.out
        x += s.width() + gap
        h = max(h, s.height())
    return body, x - gap + left, top + h + 46


def grid_of(panels, pos, cols=2, gapx=90, gapy=100, top=44, left=40):
    """Places snapshot builders on a grid of `cols` columns; returns (body, width, height)."""
    w = max(x for x, _ in pos.values())
    h = max(y for _, y in pos.values())
    body = []
    for i, build in enumerate(panels):
        r, c = divmod(i, cols)
        body += build(left + c * (w + gapx), top + r * (h + gapy)).out
    rows = (len(panels) + cols - 1) // cols
    return body, left * 2 + cols * w + (cols - 1) * gapx, top + rows * h + (rows - 1) * gapy + 50


# ------------------------------------------------------------------ architecture: the library at work on a task

TASK_POS = {'O': (20, 85), 'R1': (90, 20), 'R2': (90, 150), 'S': (160, 85), 'R3': (230, 20), 'R4': (230, 150)}
TASK_EDGES = [('O', 'R1'), ('O', 'R2'), ('O', 'S'), ('R1', 'S'), ('R2', 'S'), ('S', 'R3'), ('S', 'R4'), ('R1', 'R3'), ('R2', 'R4')]


def in_action(lang):
    T = {
        'it': dict(label='Quattro istantanee di un task: O lavora e rinnova la lease, va in crash, le copie della lease scadono, S acquisisce e riprende dal checkpoint.',
                   titles=['O lavora', 'O in crash', 'le copie scadono', 'S riprende'],
                   notes=['rinnovo + checkpoint', 'le copie invecchiano da sole', 'S acquisisce l\'epoch 2', 'dal passo 6, non da capo'],
                   legend=[('n-act', 'owner attivo: passo'), ('n-settle', 'in settling'), ('n-idle', 'replica: ttl della lease'), ('n-crash', 'crash')]),
        'en': dict(label='Four snapshots of a task: O works and renews the lease, crashes, the copies of the lease expire, S acquires and resumes from the checkpoint.',
                   titles=['O works', 'O crashes', 'the copies expire', 'S resumes'],
                   notes=['renewal + checkpoint', 'the copies age by themselves', 'S acquires epoch 2', 'from step 6, not from scratch'],
                   legend=[('n-act', 'active owner: step'), ('n-settle', 'settling'), ('n-idle', 'replica: lease ttl'), ('n-crash', 'crash')]),
    }[lang]
    replicas = ['R1', 'R2', 'R3', 'R4']
    near_o = ['R1', 'R2', 'S']
    near_s = ['R1', 'R2', 'R3', 'R4']

    def panel(k):
        def build(ox, oy):
            s = Snapshot(ox, oy, TASK_POS, TASK_EDGES, T['titles'][k])
            if k == 0:
                for n in near_o:
                    s.arrow('O', n, 'm-ok')
            if k == 2:
                for n in ('R1', 'R2'):
                    s.arrow(n, 'S', 'm-echo')
            if k == 3:
                for n in near_s:
                    s.arrow('S', n, 'm-ok')
            ttl = [{'R1': '3', 'R2': '3', 'R3': '2', 'R4': '2'},
                   {'R1': '1', 'R2': '1', 'R3': '0', 'R4': '0'},
                   {'R1': '0', 'R2': '0', 'R3': '0', 'R4': '0'},
                   {'R1': '3', 'R2': '3', 'R3': '3', 'R4': '3'}][k]
            for n in replicas:
                s.node(n, 'n-idle', ttl[n], label='')
            s.node('O', 'n-act' if k == 0 else 'n-crash', '5' if k == 0 else '', label_side='w')
            if k < 2:
                s.node('S', 'n-idle', ['3', '1'][k], label_side='s')
            elif k == 2:
                s.node('S', 'n-settle', 'e2', label_side='s')
            else:
                s.node('S', 'n-act', '6', label_side='s')
            s.text(s.width() / 2, s.height() + 44, T['notes'][k], 't-note', 'middle')
            return s
        return build

    body, w, h = grid_of([panel(k) for k in range(4)], TASK_POS)
    return figure(svg(w, h, T['label'], body), T['legend'])


# ------------------------------------------------------------------ architecture: echoes versus plain lease

ECHO_POS = {'O': (20, 70), 'N1': (100, 15), 'N2': (100, 125), 'S': (180, 70)}
ECHO_EDGES = [('N1', 'S'), ('N2', 'S'), ('N1', 'N2')]


def echoes(lang):
    T = {
        'it': dict(label='Owner O in silence. Sopra, con le eco: O non vede tornare i rinnovi e si ferma prima che S subentri. Sotto, con una lease semplice: O continua e i due owner lavorano insieme.',
                   rows=['con le eco', 'lease semplice'],
                   titles=['O in silence', 'la lease scade', 'S subentra'],
                   legend=[('n-act', 'agisce'), ('n-settle', 'in settling'), ('n-stop', 'si è fermato'), ('n-conflict', 'due owner attivi'), ('n-idle', 'replica'), ('lg-lost', 'messaggi persi')]),
        'en': dict(label='Owner O in silence. Above, with echoes: O does not see its renewals come back and stops before S takes over. Below, with a plain lease: O goes on and both owners work.',
                   rows=['with echoes', 'plain lease'],
                   titles=['O in silence', 'the lease expires', 'S takes over'],
                   legend=[('n-act', 'acting'), ('n-settle', 'settling'), ('n-stop', 'stopped'), ('n-conflict', 'two active owners'), ('n-idle', 'replica'), ('lg-lost', 'lost messages')]),
    }[lang]
    body = []
    width = 0
    for r in range(2):
        top = 44 + r * 190
        x = 150
        body.append('<text x="20" y="%d" class="t-rowtitle">%s</text>' % (top + 74, esc(T['rows'][r])))
        for k in range(3):
            s = Snapshot(x, top, ECHO_POS, ECHO_EDGES, T['titles'][k] if r == 0 else '')
            s.lost('O', 'N1')
            s.lost('O', 'N2')
            if k == 2:
                s.arrow('S', 'N1', 'm-ok')
                s.arrow('S', 'N2', 'm-ok')
            for n in ('N1', 'N2'):
                s.node(n, 'n-idle', '', label='')
            if r == 0:
                o = ['n-act', 'n-stop', 'n-stop'][k]
                sc = ['n-idle', 'n-settle', 'n-act'][k]
            else:
                o = ['n-act', 'n-act', 'n-conflict'][k]
                sc = ['n-idle', 'n-settle', 'n-conflict'][k]
            s.node('O', o, '', label_side='w')
            s.node('S', sc, '', label_side='s')
            body += s.out
            x += s.width() + 70
        width = max(width, x - 70 + 30)
    return figure(svg(width, 44 + 190 + 125 + 30, T['label'], body), T['legend'])


# ------------------------------------------------------------------ architecture: overview of the modules

def overview(lang):
    T = {
        'it': dict(label='I moduli di fcpp-leases: l\'applicazione crea un processo per ogni job (spawn) e lì chiama run_lease; sotto, ciò che run_lease compone e i moduli su cui poggia.',
                   app='applicazione', app_sub='spawn di un job: i dati · progress_type · start · rank · work',
                   tasks_sub='il processo di un job, creato dall\'applicazione: un record per round',
                   rec=[('run_lease', 'run_lease.hpp · chi lavora, da dove riparte'),
                        ('lease_round', 'ownership/lease.hpp · al più un owner'),
                        ('checkpoint', 'model/checkpoint.hpp · fin dove è arrivato')],
                   base=[('model/', 'fault · event · checkpoint · lease · job'), ('common/', 'tipi, codifica'), ('sim/', 'guasti simulati')],
                   fcpp='FCPP: old · nbr · spawn · timer'),
        'en': dict(label='The modules of fcpp-leases: the application creates a process per job (spawn) and calls run_lease in it; below, what run_lease composes and the modules it rests on.',
                   app='application', app_sub='spawn of a job: its data · progress_type · start · rank · work',
                   tasks_sub='the process of a job, created by the application: one record per round',
                   rec=[('run_lease', 'run_lease.hpp · who works, from where'),
                        ('lease_round', 'ownership/lease.hpp · at most one owner'),
                        ('checkpoint', 'model/checkpoint.hpp · how far it got')],
                   base=[('model/', 'fault · event · checkpoint · lease · job'), ('common/', 'types, encoding'), ('sim/', 'simulated faults')],
                   fcpp='FCPP: old · nbr · spawn · timer'),
    }[lang]
    o = []
    W = 860

    def box(x, y, w, h, cls):
        o.append('<rect x="%d" y="%d" width="%d" height="%d" rx="6" class="%s"/>' % (x, y, w, h, cls))

    def txt(x, y, s, cls, anchor='start'):
        o.append('<text x="%d" y="%d" text-anchor="%s" class="%s">%s</text>' % (x, y, anchor, cls, esc(s)))

    def arrow(x0, y0, x1, y1, cls='a-flow'):
        o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" class="%s"/>' % (x0, y0, x1, y1, cls))
        L = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / L, (y1 - y0) / L
        a = 6
        o.append('<polygon points="%.1f,%.1f %.1f,%.1f %.1f,%.1f" class="%s-h"/>' % (x1, y1, x1 - ux * a * 1.4 - uy * a * .6, y1 - uy * a * 1.4 + ux * a * .6, x1 - ux * a * 1.4 + uy * a * .6, y1 - uy * a * 1.4 - ux * a * .6, cls))

    box(20, 10, W - 40, 44, 'b-app')
    txt(W // 2, 30, T['app'], 't-box-title', 'middle')
    txt(W // 2, 46, T['app_sub'], 't-box-sub', 'middle')

    # spawn(): run_lease, and the pieces it uses
    gx, gw, gy = 54, W - 108, 110
    box(gx - 14, gy - 40, gw + 28, len(T['rec']) * 46 + 52, 'b-group')
    txt(gx, gy - 16, 'spawn()', 't-col')
    txt(gx + gw, gy - 16, T['tasks_sub'], 't-col-sub', 'end')
    arrow(W // 2, 54, W // 2, gy - 42)
    names = T['rec']
    box(gx, gy, gw, 34, 'b-step')
    txt(gx + 14, gy + 22, names[0][0], 't-step')
    txt(gx + gw - 14, gy + 22, names[0][1], 't-step-note', 'end')
    for i, (n, note) in enumerate(names[1:]):
        y = gy + (i + 1) * 46 + 12
        box(gx + 30, y, gw - 30, 34, 'b-step')
        txt(gx + 44, y + 22, n, 't-step')
        txt(gx + gw - 14, y + 22, note, 't-step-note', 'end')
        o.append('<path d="M%d,%d L%d,%d L%d,%d" class="a-flow" fill="none"/>' % (gx + 14, gy + 34, gx + 14, y + 17, gx + 22, y + 17))
        arrow(gx + 20, y + 17, gx + 29, y + 17)

    # what it rests on
    by = gy + len(T['rec']) * 46 + 36
    widths = [W - 40 - 2 * 16 - 2 * 180, 180, 180]
    x = 20
    for (n, note), bw in zip(T['base'], widths):
        box(x, by, bw, 40, 'b-base')
        txt(x + 14, by + 25, n, 't-step')
        txt(x + bw - 14, by + 25, note, 't-step-note', 'end')
        x += bw + 16
    box(20, by + 54, W - 40, 34, 'b-fcpp')
    txt(W // 2, by + 76, T['fcpp'], 't-step-note', 'middle')
    return figure(svg(W, by + 98, T['label'], o))



# ------------------------------------------------------------------ data flow between files

class Flow:
    """A column of steps (file, call) joined by labelled arrows, with inputs on the right."""

    W = 860
    X, SW = 190, 370          # main column
    SX, SSW = 612, 228        # side inputs
    H = 36

    def __init__(self):
        self.o = []

    def box(self, x, y, w, h, cls):
        self.o.append('<rect x="%d" y="%d" width="%d" height="%d" rx="6" class="%s"/>' % (x, y, w, h, cls))

    def txt(self, x, y, s, cls, anchor='start'):
        self.o.append('<text x="%d" y="%d" text-anchor="%s" class="%s">%s</text>' % (x, y, anchor, cls, esc(s)))

    def arrow(self, x0, y0, x1, y1, cls='a-flow'):
        self.o.append('<line x1="%d" y1="%d" x2="%d" y2="%d" class="%s"/>' % (x0, y0, x1, y1, cls))
        L = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / L, (y1 - y0) / L
        a = 6
        self.o.append('<polygon points="%.1f,%.1f %.1f,%.1f %.1f,%.1f" class="%s-h"/>' % (x1, y1, x1 - ux * a * 1.4 - uy * a * .6, y1 - uy * a * 1.4 + ux * a * .6, x1 - ux * a * 1.4 + uy * a * .6, y1 - uy * a * 1.4 - ux * a * .6, cls))

    def path(self, pts, cls='a-flow'):
        d = 'M' + ' L'.join('%d,%d' % p for p in pts[:-1])
        self.o.append('<path d="%s" class="%s" fill="none"/>' % (d, cls))
        self.arrow(pts[-2][0], pts[-2][1], pts[-1][0], pts[-1][1], cls)

    def step(self, y, file, call, cls='b-step'):
        self.box(self.X, y, self.SW, self.H, cls)
        self.txt(self.X + 14, y + 23, file, 't-step')
        self.txt(self.X + self.SW - 14, y + 23, call, 't-step-note', 'end')

    def down(self, y0, y1, label):
        x = self.X + self.SW // 2
        self.arrow(x, y0, x, y1)
        self.txt(x + 10, (y0 + y1) // 2 + 4, label, 't-flow')

    def side(self, y, label, cls='b-base'):
        self.box(self.SX, y + 2, self.SSW, self.H - 4, cls)
        self.txt(self.SX + 12, y + 23, label, 't-step-note')
        self.arrow(self.SX - 2, y + self.H // 2, self.X + self.SW + 2, y + self.H // 2)


def tasks_flow(lang):
    T = {
        'it': dict(label='Cosa passa tra i file di recovery/ e ownership/ in un round, dentro il processo di un task.',
                   top='spawn(job) dell\'applicazione: run_lease(job, lease_config) nel processo di ogni job, su ogni dispositivo',
                   rows=[('applicazione', 'job.rank(CALL)'), ('run_lease.hpp', 'claim{want, rank}'),
                         ('run_lease.hpp', 'checkpoint più fresco'), ('run_lease.hpp', 'stallo? → preempt'),
                         ('lease.hpp', 'lease_round'), ('applicazione', 'job.work(progress, step)'),
                         ('run_lease.hpp', 'checkpoint.advanced(progress)')],
                   arrows=['rank', 'claim', 'checkpoint', 'claim con preempt', 'tenure: active, epoch, previous', 'nuovo progress', 'lease_state'],
                   side={0: 'su ogni dispositivo del processo', 1: 'lease_memory: libero? in pausa?', 2: 'record: checkpoint dei vicini',
                         3: 'lease_config.stall_timeout', 4: 'record: lease dei vicini, invecchiate', 5: 'solo se la tenure è active'},
                   group='nbr', loop='record inviato',
                   out='(lease_state, status) per spawn'),
        'en': dict(label='What passes between the files of recovery/ and ownership/ in a round, inside the process of a task.',
                   top='the application\'s spawn(job): run_lease(job, lease_config) in the process of each job, on every device',
                   rows=[('application', 'job.rank(CALL)'), ('run_lease.hpp', 'claim{want, rank}'),
                         ('run_lease.hpp', 'freshest checkpoint'), ('run_lease.hpp', 'stalled? → preempt'),
                         ('lease.hpp', 'lease_round'), ('application', 'job.work(progress, step)'),
                         ('run_lease.hpp', 'checkpoint.advanced(progress)')],
                   arrows=['rank', 'claim', 'checkpoint', 'claim with preempt', 'tenure: active, epoch, previous', 'new progress', 'lease_state'],
                   side={0: 'on every device of the process', 1: 'lease_memory: free? paused?', 2: 'records: neighbours\' checkpoints',
                         3: 'lease_config.stall_timeout', 4: 'records: neighbours\' leases, aged', 5: 'only if the tenure is active'},
                   group='nbr', loop='record sent',
                   out='(lease_state, status) for spawn'),
    }[lang]
    f = Flow()
    P = 72
    f.box(20, 10, f.W - 40, 32, 'b-fcpp')
    f.txt(f.W // 2, 31, T['top'], 't-step-note', 'middle')
    y0 = 72
    ys = []
    y = y0
    for i in range(len(T['rows'])):
        if i == 2:
            y += 26                  # room for the title of the nbr group
        ys.append(y)
        y += P
    # the nbr of run_lease: rows 2..6
    gy0, gy1 = ys[2] - 30, ys[6] + f.H + 10
    f.box(f.X - 16, gy0, f.SW + 32, gy1 - gy0, 'b-group')
    f.txt(f.X - 4, gy0 + 18, T['group'], 't-col')
    f.arrow(f.X + f.SW // 2, 42, f.X + f.SW // 2, y0 - 2)
    for i, (file, call) in enumerate(T['rows']):
        f.step(ys[i], file, call, 'b-app' if i == 5 else 'b-step')
        nxt = ys[i + 1] if i + 1 < len(ys) else y
        f.down(ys[i] + f.H, nxt - 2, T['arrows'][i])
        if i in T['side']:
            f.side(ys[i], T['side'][i])
    f.step(y, 'run_lease.hpp', T['out'], 'b-app')
    # loop: the new checkpoint leaves in this round, in the record of the device
    lx = 150
    a, b = ys[6] + f.H // 2, ys[2] + f.H // 2
    f.path([(f.X - 16, a), (lx, a), (lx, b), (f.X - 2, b)], 'a-link')
    f.txt(lx - 8, (a + b) // 2 + 4, T['loop'], 't-link', 'end')
    return figure(svg(f.W, y + f.H + 12, T['label'], f.o))

FIGURES = {
    'overview.html': overview,
    'in_action.html': in_action,
    'echoes.html': echoes,
    'tasks_flow.html': tasks_flow,
}

if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    for lang in LANGS:
        folder = os.path.join(here, '..', 'pages', lang, 'figures')
        os.makedirs(folder, exist_ok=True)
        for name, make in FIGURES.items():
            with open(os.path.join(folder, name), 'w', encoding='utf-8', newline='\n') as f:
                f.write(make(lang))
        print('written', os.path.normpath(folder))
