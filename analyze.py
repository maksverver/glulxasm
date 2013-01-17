# Analyzes control and data flow in functions, in order to optimize

# Note: we assume that:
#   - only `stkcopy' and `call' affect the stack.
#   - `glk' instructions break the analysis

from sys import stderr

# Returns a control flow graph for the given instruction list:
def analyze_control_flow(instrs):
    edges = []

    addrs = {}
    for (i,instr) in enumerate(instrs):
        addrs[instr.offset()] = i

    for (i,instr) in enumerate(instrs):
        if instr.mnemonic == 'glk':
            # For now, don't optimize functions with glk instructions
            print >>stderr, 'Skipping analysis due to glk instruction'
            return None

        if instr.mnemonic not in ('tailcall', 'ret', 'throw', 'jump',
                                  'jumpabs', 'quit', 'restart'):

            if i + 1 >= len(instrs):
                print >>stderr, 'Skipping analysis of truncated function'
                return None

            edges.append((i, i + 1))

        if instr.is_branch():
            dest = instr.branch_target()
            if not dest:
                print >>stderr, 'Skipping analysis due to unknown branch target'
                return None
            if dest not in addrs:
                print >>stderr, 'Skipping analysis due to invalid branch target'
                return None
            edges.append((i, addrs[dest]))

        if instrs[i].mnemonic.startswith('stk'):
            print >>stderr, 'Skipping analysis due to occurrence of', \
                    instrs[i].mnemonic, 'instruction'
            return None

    return edges

def analyze_stack_pointer(instrs, edges):

    # Determine stack height at each instruction (using breadth-first-search):
    height = [None]*len(instrs)
    height[0] = 0
    todo, pos = [0], 0
    while pos < len(todo):
        i = todo[pos]
        h = height[i]
        instrs[i].sp = h

        # assign stack locations of loaded stack operands:
        for o, p in zip(instrs[i].operands, instrs[i].parameters):
            if o.is_stack_ref():
                if p in "lmf":
                    h -= 1
                    o._value = h  # hack: story stack location as value
                else:
                    assert p == 's'

        # adjust stack location after (tail)call instruction
        if instrs[i].mnemonic == 'call' or instrs[i].mnemonic == 'tailcall':
            o = instrs[i].operands[1]
            if not o.is_immediate():
                print >>stderr, 'Skipping analysis due to', \
                    'indeterminate number of call arguments'
                return None
            assert o.value() is not None
            h -= o.value()

        # assign stack locations of stored stack operands:
        for o, p in zip(instrs[i].operands, instrs[i].parameters):
            if o.is_stack_ref():
                if p == 's':
                    o._value = h  # hack: story stack location as value
                    h += 1

        pos += 1
        for j in [ y for (x,y) in edges if x == i ]:  # find branch targets:
            if height[j] is None:
                height[j] = h
                todo.append(j)
            elif height[j] != h:
                print >>stderr, 'Skipping analysis due to inconsistent stack height'
                return None

    return height

def optimize(instrs):
    edges = analyze_control_flow(instrs)
    if edges is None:
        return None

    height = analyze_stack_pointer(instrs, edges)
    if height is None:
        return None

    if None in height:
        print >>stderr, 'Warning: unreachable code detected!'
        # Just remove unused instructions and hope our analysis was correct!
        instrs[:] = (instr for (i,instr) in enumerate(instrs)
                            if height[i] is not None)
        height = [ h for h in height if h is not None ]

    if height.count(None) == len(height):
        return []
    else:
        return range(min(height), max(height))
